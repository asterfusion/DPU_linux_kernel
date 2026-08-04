// SPDX-License-Identifier: GPL-2.0
/* Generic two-wire GPIO waveform interface for userspace CPLD updaters. */

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/idr.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <uapi/linux/cpld-gpio-update.h>

#define CPLD_GPIO_UPDATE_NAME		"cpld-update"
#define CPLD_GPIO_UPDATE_MAX_DEVS	256
#define CPLD_GPIO_UPDATE_MAX_XFER	8192
#define CPLD_GPIO_UPDATE_DEF_DELAY_US	5
#define CPLD_GPIO_UPDATE_MAX_DELAY_US	1000

struct cpld_gpio_update {
	struct device *dev;
	struct gpio_desc *scl;
	struct gpio_desc *sda;
	struct miscdevice miscdev;
	/* Serializes waveform operations and device lifecycle transitions. */
	struct mutex io_lock;
	struct kref refcount;
	atomic_t opened;
	unsigned int delay_us;
	int id;
	bool disconnected;
	bool suspended;
	char name[32];
};

static DEFINE_IDA(cpld_gpio_update_ida);

static void cpld_gpio_update_free(struct kref *refcount);

static inline unsigned int cpld_gpio_half_delay(struct cpld_gpio_update *up)
{
	return (up->delay_us + 1) / 2;
}

static void cpld_gpio_set_sda(struct cpld_gpio_update *up, int value)
{
	gpiod_set_raw_value(up->sda, value);
	udelay(cpld_gpio_half_delay(up));
}

static void cpld_gpio_set_scl_low(struct cpld_gpio_update *up)
{
	gpiod_set_raw_value(up->scl, 0);
	udelay(up->delay_us / 2);
}

static void cpld_gpio_set_scl_high(struct cpld_gpio_update *up)
{
	gpiod_set_raw_value(up->scl, 1);
	udelay(up->delay_us);
}

static void cpld_gpio_release_lines(struct cpld_gpio_update *up)
{
	gpiod_set_raw_value(up->sda, 1);
	gpiod_set_raw_value(up->scl, 1);
}

static int cpld_gpio_write_byte(struct cpld_gpio_update *up, u8 value,
				bool ignore_nak)
{
	int bit;
	int ack;

	for (bit = 7; bit >= 0; bit--) {
		cpld_gpio_set_sda(up, !!(value & BIT(bit)));
		cpld_gpio_set_scl_high(up);
		cpld_gpio_set_scl_low(up);
	}

	cpld_gpio_set_sda(up, 1);
	cpld_gpio_set_scl_high(up);
	ack = !gpiod_get_raw_value(up->sda);
	cpld_gpio_set_scl_low(up);

	if (!ack && !ignore_nak)
		return -EIO;

	return 0;
}

static u8 cpld_gpio_read_byte(struct cpld_gpio_update *up)
{
	u8 value = 0;
	int bit;

	cpld_gpio_set_sda(up, 1);
	for (bit = 7; bit >= 0; bit--) {
		cpld_gpio_set_scl_high(up);
		if (gpiod_get_raw_value(up->sda))
			value |= BIT(bit);
		gpiod_set_raw_value(up->scl, 0);
		udelay(bit ? up->delay_us : up->delay_us / 2);
	}

	return value;
}

static void cpld_gpio_send_ack(struct cpld_gpio_update *up, bool ack)
{
	cpld_gpio_set_sda(up, ack ? 0 : 1);
	cpld_gpio_set_scl_high(up);
	cpld_gpio_set_scl_low(up);
}

static int cpld_gpio_xfer_write(struct cpld_gpio_update *up, u8 *buf,
				unsigned int len, u32 flags)
{
	bool ignore_nak = flags & CPLD_GPIO_XFER_F_IGNORE_NAK;
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = cpld_gpio_write_byte(up, buf[i], ignore_nak);
		if (ret)
			return ret;
	}

	return 0;
}

static void cpld_gpio_xfer_read(struct cpld_gpio_update *up, u8 *buf,
				unsigned int len, u32 flags)
{
	bool no_ack = flags & CPLD_GPIO_XFER_F_NO_RD_ACK;
	unsigned int i;

	for (i = 0; i < len; i++) {
		buf[i] = cpld_gpio_read_byte(up);
		if (!no_ack)
			cpld_gpio_send_ack(up, i + 1 < len);
	}
}

static int cpld_gpio_update_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct cpld_gpio_update *up;
	int ret = 0;

	up = container_of(miscdev, struct cpld_gpio_update, miscdev);
	mutex_lock(&up->io_lock);
	if (up->disconnected) {
		ret = -ENODEV;
	} else if (up->suspended) {
		ret = -EBUSY;
	} else if (atomic_cmpxchg(&up->opened, 0, 1)) {
		ret = -EBUSY;
	} else {
		kref_get(&up->refcount);
		cpld_gpio_release_lines(up);
		file->private_data = up;
	}
	mutex_unlock(&up->io_lock);

	return ret;
}

static int cpld_gpio_update_release(struct inode *inode, struct file *file)
{
	struct cpld_gpio_update *up = file->private_data;

	mutex_lock(&up->io_lock);
	if (!up->disconnected)
		cpld_gpio_release_lines(up);
	atomic_set(&up->opened, 0);
	mutex_unlock(&up->io_lock);
	kref_put(&up->refcount, cpld_gpio_update_free);

	return 0;
}

static long cpld_gpio_update_xfer(struct cpld_gpio_update *up,
				  unsigned long arg)
{
	struct cpld_gpio_ioc_xfer xfer;
	void __user *data;
	u32 allowed_flags;
	u8 *buf;
	int ret = 0;

	if (copy_from_user(&xfer, (void __user *)arg, sizeof(xfer)))
		return -EFAULT;
	if (!xfer.data || !xfer.len || xfer.len > CPLD_GPIO_UPDATE_MAX_XFER ||
	    xfer.reserved)
		return -EINVAL;

	switch (xfer.direction) {
	case CPLD_GPIO_XFER_WRITE:
		allowed_flags = CPLD_GPIO_XFER_F_IGNORE_NAK;
		break;
	case CPLD_GPIO_XFER_READ:
		allowed_flags = CPLD_GPIO_XFER_F_NO_RD_ACK;
		break;
	default:
		return -EINVAL;
	}
	if (xfer.flags & ~allowed_flags)
		return -EINVAL;

	data = u64_to_user_ptr(xfer.data);
	if (xfer.direction == CPLD_GPIO_XFER_WRITE) {
		buf = memdup_user(data, xfer.len);
		if (IS_ERR(buf))
			return PTR_ERR(buf);
	} else {
		buf = kmalloc(xfer.len, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
	}

	if (xfer.direction == CPLD_GPIO_XFER_WRITE) {
		ret = cpld_gpio_xfer_write(up, buf, xfer.len, xfer.flags);
	} else {
		cpld_gpio_xfer_read(up, buf, xfer.len, xfer.flags);
		if (copy_to_user(data, buf, xfer.len))
			ret = -EFAULT;
	}

	kfree(buf);
	return ret;
}

static long cpld_gpio_update_ioctl(struct file *file, unsigned int cmd,
				   unsigned long arg)
{
	struct cpld_gpio_update *up = file->private_data;
	__u32 value;
	int ret = 0;

	if (_IOC_TYPE(cmd) != CPLD_GPIO_IOC_MAGIC)
		return -ENOTTY;

	mutex_lock(&up->io_lock);
	if (up->disconnected) {
		ret = -ENODEV;
		goto unlock;
	}

	switch (cmd) {
	case CPLD_GPIO_IOC_SET_SCL:
		if (copy_from_user(&value, (void __user *)arg, sizeof(value))) {
			ret = -EFAULT;
			break;
		}
		if (value > 1) {
			ret = -EINVAL;
			break;
		}
		if (value)
			cpld_gpio_set_scl_high(up);
		else
			cpld_gpio_set_scl_low(up);
		break;
	case CPLD_GPIO_IOC_SET_SDA:
		if (copy_from_user(&value, (void __user *)arg, sizeof(value))) {
			ret = -EFAULT;
			break;
		}
		if (value > 1) {
			ret = -EINVAL;
			break;
		}
		cpld_gpio_set_sda(up, value);
		break;
	case CPLD_GPIO_IOC_GET_SDA:
		value = gpiod_get_raw_value(up->sda);
		if (copy_to_user((void __user *)arg, &value, sizeof(value)))
			ret = -EFAULT;
		break;
	case CPLD_GPIO_IOC_XFER:
		ret = cpld_gpio_update_xfer(up, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

unlock:
	mutex_unlock(&up->io_lock);
	return ret;
}

static const struct file_operations cpld_gpio_update_fops = {
	.owner = THIS_MODULE,
	.open = cpld_gpio_update_open,
	.release = cpld_gpio_update_release,
	.unlocked_ioctl = cpld_gpio_update_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.llseek = no_llseek,
};

static void cpld_gpio_update_free(struct kref *refcount)
{
	struct cpld_gpio_update *up;

	up = container_of(refcount, struct cpld_gpio_update, refcount);
	ida_free(&cpld_gpio_update_ida, up->id);
	kfree(up);
}

static int cpld_gpio_update_alloc_id(struct device *dev)
{
	int alias;

	alias = of_alias_get_id(dev->of_node, "cpldupdate");
	if (alias >= CPLD_GPIO_UPDATE_MAX_DEVS)
		return -EINVAL;
	if (alias >= 0)
		return ida_alloc_range(&cpld_gpio_update_ida, alias, alias,
				       GFP_KERNEL);

	return ida_alloc_max(&cpld_gpio_update_ida,
			     CPLD_GPIO_UPDATE_MAX_DEVS - 1, GFP_KERNEL);
}

static int cpld_gpio_update_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cpld_gpio_update *up;
	u32 delay_us;
	int ret;

	up = kzalloc(sizeof(*up), GFP_KERNEL);
	if (!up)
		return -ENOMEM;

	up->dev = dev;
	mutex_init(&up->io_lock);
	kref_init(&up->refcount);
	atomic_set(&up->opened, 0);

	up->id = cpld_gpio_update_alloc_id(dev);
	if (up->id < 0) {
		ret = up->id;
		goto free_up;
	}

	up->scl = devm_gpiod_get(dev, "scl", GPIOD_OUT_HIGH_OPEN_DRAIN);
	if (IS_ERR(up->scl)) {
		ret = dev_err_probe(dev, PTR_ERR(up->scl),
				    "failed to get SCL GPIO\n");
		goto free_id;
	}
	up->sda = devm_gpiod_get(dev, "sda", GPIOD_OUT_HIGH_OPEN_DRAIN);
	if (IS_ERR(up->sda)) {
		ret = dev_err_probe(dev, PTR_ERR(up->sda),
				    "failed to get SDA GPIO\n");
		goto free_id;
	}
	if (gpiod_cansleep(up->scl) || gpiod_cansleep(up->sda)) {
		dev_err(dev, "sleeping GPIO controllers are not supported\n");
		ret = -EOPNOTSUPP;
		goto free_id;
	}

	if (device_property_read_u32(dev, "delay-us", &delay_us))
		delay_us = CPLD_GPIO_UPDATE_DEF_DELAY_US;
	if (!delay_us || delay_us > CPLD_GPIO_UPDATE_MAX_DELAY_US) {
		dev_err(dev, "delay-us must be between 1 and %u\n",
			CPLD_GPIO_UPDATE_MAX_DELAY_US);
		ret = -EINVAL;
		goto free_id;
	}
	up->delay_us = delay_us;

	snprintf(up->name, sizeof(up->name), "%s%d",
		 CPLD_GPIO_UPDATE_NAME, up->id);
	up->miscdev.minor = MISC_DYNAMIC_MINOR;
	up->miscdev.name = up->name;
	up->miscdev.fops = &cpld_gpio_update_fops;
	up->miscdev.parent = dev;
	up->miscdev.mode = 0600;

	ret = misc_register(&up->miscdev);
	if (ret) {
		dev_err(dev, "failed to register %s: %d\n", up->name, ret);
		goto free_id;
	}

	platform_set_drvdata(pdev, up);
	dev_info(dev, "registered /dev/%s with %u us delay\n",
		 up->name, up->delay_us);
	return 0;

free_id:
	ida_free(&cpld_gpio_update_ida, up->id);
free_up:
	kfree(up);
	return ret;
}

static int cpld_gpio_update_remove(struct platform_device *pdev)
{
	struct cpld_gpio_update *up = platform_get_drvdata(pdev);

	misc_deregister(&up->miscdev);
	mutex_lock(&up->io_lock);
	up->disconnected = true;
	cpld_gpio_release_lines(up);
	mutex_unlock(&up->io_lock);
	kref_put(&up->refcount, cpld_gpio_update_free);

	return 0;
}

static int cpld_gpio_update_suspend(struct device *dev)
{
	struct cpld_gpio_update *up = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&up->io_lock);
	if (atomic_read(&up->opened)) {
		ret = -EBUSY;
	} else {
		up->suspended = true;
		cpld_gpio_release_lines(up);
	}
	mutex_unlock(&up->io_lock);

	return ret;
}

static int cpld_gpio_update_resume(struct device *dev)
{
	struct cpld_gpio_update *up = dev_get_drvdata(dev);

	mutex_lock(&up->io_lock);
	cpld_gpio_release_lines(up);
	up->suspended = false;
	mutex_unlock(&up->io_lock);

	return 0;
}

static const struct dev_pm_ops cpld_gpio_update_pm_ops = {
	.suspend = cpld_gpio_update_suspend,
	.resume = cpld_gpio_update_resume,
	.freeze = cpld_gpio_update_suspend,
	.thaw = cpld_gpio_update_resume,
	.poweroff = cpld_gpio_update_suspend,
	.restore = cpld_gpio_update_resume,
};

static const struct of_device_id cpld_gpio_update_of_match[] = {
	{ .compatible = "marvell,cpld-gpio-upgrade" },
	{ }
};
MODULE_DEVICE_TABLE(of, cpld_gpio_update_of_match);

static struct platform_driver cpld_gpio_update_driver = {
	.probe = cpld_gpio_update_probe,
	.remove = cpld_gpio_update_remove,
	.driver = {
		.name = "cpld-gpio-update",
		.of_match_table = cpld_gpio_update_of_match,
		.pm = &cpld_gpio_update_pm_ops,
	},
};
module_platform_driver(cpld_gpio_update_driver);

MODULE_DESCRIPTION("Generic GPIO waveform interface for CPLD updates");
MODULE_LICENSE("GPL");
