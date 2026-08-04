/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_CPLD_GPIO_UPDATE_H
#define _UAPI_LINUX_CPLD_GPIO_UPDATE_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CPLD_GPIO_XFER_WRITE		0
#define CPLD_GPIO_XFER_READ		1

#define CPLD_GPIO_XFER_F_IGNORE_NAK	(1U << 0)
#define CPLD_GPIO_XFER_F_NO_RD_ACK	(1U << 1)

struct cpld_gpio_ioc_xfer {
	__aligned_u64 data;
	__u32 len;
	__u32 direction;
	__u32 flags;
	__u32 reserved;
};

#define CPLD_GPIO_IOC_MAGIC	'C'
#define CPLD_GPIO_IOC_SET_SCL	_IOW(CPLD_GPIO_IOC_MAGIC, 0x00, __u32)
#define CPLD_GPIO_IOC_SET_SDA	_IOW(CPLD_GPIO_IOC_MAGIC, 0x01, __u32)
#define CPLD_GPIO_IOC_GET_SDA	_IOR(CPLD_GPIO_IOC_MAGIC, 0x02, __u32)
#define CPLD_GPIO_IOC_XFER	_IOWR(CPLD_GPIO_IOC_MAGIC, 0x03, \
				      struct cpld_gpio_ioc_xfer)

#endif /* _UAPI_LINUX_CPLD_GPIO_UPDATE_H */
