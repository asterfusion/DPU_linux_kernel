#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>   
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#define DRV_NAME	"tera_wdt"
#define DRV_VERSION	"1.0"

//#define GHC83N
#define GHC96N

#ifdef GHC83N
    #define GPIO_BASE_ADDR 432
    #define ET1528_WDT_GPIO (GPIO_BASE_ADDR+34)
    #define ET1528_WDT_FEED_GPIO (GPIO_BASE_ADDR+16)
#elif defined(GHC96N)
    #define GPIO_BASE_ADDR 448
    #define ET1528_WDT_GPIO (GPIO_BASE_ADDR+32)
    #define ET1528_WDT_FEED_GPIO (GPIO_BASE_ADDR+33)
#endif


#define TIMER_MARGIN	60		/* Default is 60 seconds */
static int soft_margin = TIMER_MARGIN;	/* in seconds */
module_param(soft_margin, int, 0);
MODULE_PARM_DESC(soft_margin,
	"Watchdog soft_margin in seconds. (0 < soft_margin < 65536, default="
					__MODULE_STRING(TIMER_MARGIN) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

#ifdef ONLY_TESTING
static int soft_noboot = 1;
#else
static int soft_noboot = 0;
#endif  /* ONLY_TESTING */

module_param(soft_noboot, int, 0);
MODULE_PARM_DESC(soft_noboot,
	"Asterfusion_WatchDog action, set to 1 to ignore reboots, 0 to reboot "
					"(default depends on ONLY_TESTING)");

static struct task_struct *task_tpk;
/*
 *	Our timer
 */

static void watchdog_fire(unsigned long);

static struct timer_list watchdog_ticktock =
		TIMER_INITIALIZER(watchdog_fire, 0, 0);
static unsigned long orphan_timer;


/*
 *	If the timer expires..
 */

static void watchdog_fire(unsigned long data)
{
	if (test_and_clear_bit(0, &orphan_timer))
		module_put(THIS_MODULE);

	if (soft_noboot)
		printk(KERN_CRIT "Triggered - Reboot ignored.\n");
	else {
		printk(KERN_CRIT "Initiating system reboot.\n");
		emergency_restart();
		printk(KERN_CRIT "Reboot didn't ?????\n");
	}
}

/*
 *	Softdog operations
 */


static int softdog_stop(void)
{
    kthread_stop(task_tpk);
    del_timer(&watchdog_ticktock);
	return 0;
}

int dog_start;
static int  thread_feed_dog(void *data)
{
    do{
        gpio_direction_output(ET1528_WDT_FEED_GPIO, 1);
        msleep(700);
        gpio_direction_output(ET1528_WDT_FEED_GPIO, 0);
        msleep(300);

    }while(!kthread_should_stop());

	return 0;
}
/*
 *	/dev/watchdog handling
 */

static int softdog_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static int softdog_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t softdog_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
	return len;
}

static long softdog_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	switch(cmd){
        case 2:
			if(!IS_ERR(task_tpk) && dog_start){
				kthread_stop(task_tpk);
				printk(KERN_INFO "cmd=2 stop kthread, then reset system\n");
            }
            return 1;
		case 1:
			if(!dog_start){
				task_tpk = kthread_run(thread_feed_dog, NULL, DRV_NAME"_thread");
				if(IS_ERR(task_tpk)){
					printk(KERN_INFO "create kthread failed\n");
				}else{
                    msleep(1000);
                    dog_start = 1;
                    gpio_direction_output(ET1528_WDT_GPIO, 0); //start watchdog
                }
			}
			return 1;
		case 0:
			if(!IS_ERR(task_tpk) && dog_start){
                gpio_direction_output(ET1528_WDT_GPIO, 1); //stop watchdog
                msleep(1000);
                kthread_stop(task_tpk);
				printk(KERN_INFO "stop kthread\n");
				dog_start = 0;
			}
			return 1;
	}
	return 0;
}

/*
 *	Notifier for system down
 */

static int softdog_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
    printk(KERN_ERR "tera_wdt get notify code %d\n", code);
	if (code == SYS_DOWN || code == SYS_HALT)
		softdog_stop();
	return NOTIFY_DONE;
}

/*
 *	Kernel Interfaces
 */

static const struct file_operations softdog_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= softdog_write,
	.unlocked_ioctl	= softdog_ioctl,
	.open		= softdog_open,
	.release	= softdog_release,
};

static struct miscdevice softdog_miscdev = {
	.minor	    = WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &softdog_fops,
};

static struct notifier_block softdog_notifier = {
	.notifier_call	= softdog_notify_sys,
};

static char banner[] __initdata = KERN_INFO "Asterfusion Watchdog Timer: 1.00 "
	"initialized. soft_noboot=%d soft_margin=%d sec (nowayout= %d)\n";

static int __init watchdog_init(void)
{
	int ret = 0;
	dog_start = 0;

	ret = misc_register(&softdog_miscdev);
	if (ret) {
		printk(KERN_ERR "cannot register miscdev on minor=%d (err=%d)\n",
						softdog_miscdev.minor, ret);
		return ret;
	}
    register_reboot_notifier(&softdog_notifier);
    register_restart_handler(&softdog_notifier);

    task_tpk = kthread_run(thread_feed_dog, NULL, DRV_NAME"_thread");
    msleep(1000);
	gpio_direction_output(ET1528_WDT_GPIO, 0); //start watchdog
    dog_start = 1;

	printk(banner, soft_noboot, soft_margin, nowayout);

	return 0;
}

static void __exit watchdog_exit(void)
{
    printk(KERN_INFO "tera_wdt exit\n");
    misc_deregister(&softdog_miscdev);
	unregister_reboot_notifier(&softdog_notifier);
    unregister_restart_handler(&softdog_notifier);
    softdog_stop();
}

module_init(watchdog_init);
module_exit(watchdog_exit);

MODULE_AUTHOR("marvin");
MODULE_DESCRIPTION("Asterfusion Watchdog Device Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS(DRV_NAME);
