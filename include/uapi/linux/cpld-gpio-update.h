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

#define CPLD_UPDATE_TRANSPORT_GPIO 0
#define CPLD_UPDATE_TRANSPORT_I2C 1
#define CPLD_UPDATE_IOC_MAGIC 'U'

struct cpld_update_info {
	__u32 transport;
	__u32 i2c_address;
};

#define CPLD_UPDATE_IOC_GET_INFO _IOR(CPLD_UPDATE_IOC_MAGIC, 0x00, \
				      struct cpld_update_info)

struct cpld_update_i2c_xfer {
	__aligned_u64 tx_data;
	__aligned_u64 rx_data;
	__u32 tx_len;
	__u32 rx_len;
	__u32 flags;
	__u32 reserved;
};
#define CPLD_UPDATE_IOC_I2C_XFER _IOWR(CPLD_UPDATE_IOC_MAGIC, 0x01, \
				       struct cpld_update_i2c_xfer)

#endif /* _UAPI_LINUX_CPLD_GPIO_UPDATE_H */
