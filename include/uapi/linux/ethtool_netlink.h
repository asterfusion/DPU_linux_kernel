/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * include/uapi/linux/ethtool_netlink.h - netlink interface for ethtool
 *
 * See Documentation/networking/ethtool-netlink.txt in kernel source tree for
 * doucumentation of the interface.
 */

#ifndef _UAPI_LINUX_ETHTOOL_NETLINK_H_
#define _UAPI_LINUX_ETHTOOL_NETLINK_H_

#include <linux/ethtool.h>

/* message types - userspace to kernel */
enum {
	ETHTOOL_MSG_USER_NONE,
	ETHTOOL_MSG_RINGS_GET = 15,
	ETHTOOL_MSG_RINGS_SET,

	/* add new constants above here */
	__ETHTOOL_MSG_USER_CNT,
	ETHTOOL_MSG_USER_MAX = __ETHTOOL_MSG_USER_CNT - 1
};

/* message types - kernel to userspace */
enum {
	ETHTOOL_MSG_KERNEL_NONE,
	ETHTOOL_MSG_RINGS_GET_REPLY = 16,
	ETHTOOL_MSG_RINGS_NTF,

	/* add new constants above here */
	__ETHTOOL_MSG_KERNEL_CNT,
	ETHTOOL_MSG_KERNEL_MAX = __ETHTOOL_MSG_KERNEL_CNT - 1
};

/* request header */

/* use compact bitsets in reply */
#define ETHTOOL_FLAG_COMPACT_BITSETS	(1 << 0)
/* provide optional reply for SET or ACT requests */
#define ETHTOOL_FLAG_OMIT_REPLY	(1 << 1)

#define ETHTOOL_FLAG_ALL (ETHTOOL_FLAG_COMPACT_BITSETS | \
			  ETHTOOL_FLAG_OMIT_REPLY)

enum {
	ETHTOOL_A_HEADER_UNSPEC,
	ETHTOOL_A_HEADER_DEV_INDEX,		/* u32 */
	ETHTOOL_A_HEADER_DEV_NAME,		/* string */
	ETHTOOL_A_HEADER_FLAGS,			/* u32 - ETHTOOL_FLAG_* */

	/* add new constants above here */
	__ETHTOOL_A_HEADER_CNT,
	ETHTOOL_A_HEADER_MAX = __ETHTOOL_A_HEADER_CNT - 1
};

/* bit sets */

enum {
	ETHTOOL_A_BITSET_BIT_UNSPEC,
	ETHTOOL_A_BITSET_BIT_INDEX,		/* u32 */
	ETHTOOL_A_BITSET_BIT_NAME,		/* string */
	ETHTOOL_A_BITSET_BIT_VALUE,		/* flag */

	/* add new constants above here */
	__ETHTOOL_A_BITSET_BIT_CNT,
	ETHTOOL_A_BITSET_BIT_MAX = __ETHTOOL_A_BITSET_BIT_CNT - 1
};

enum {
	ETHTOOL_A_BITSET_BITS_UNSPEC,
	ETHTOOL_A_BITSET_BITS_BIT,		/* nest - _A_BITSET_BIT_* */

	/* add new constants above here */
	__ETHTOOL_A_BITSET_BITS_CNT,
	ETHTOOL_A_BITSET_BITS_MAX = __ETHTOOL_A_BITSET_BITS_CNT - 1
};

enum {
	ETHTOOL_A_BITSET_UNSPEC,
	ETHTOOL_A_BITSET_NOMASK,		/* flag */
	ETHTOOL_A_BITSET_SIZE,			/* u32 */
	ETHTOOL_A_BITSET_BITS,			/* nest - _A_BITSET_BITS_* */
	ETHTOOL_A_BITSET_VALUE,			/* binary */
	ETHTOOL_A_BITSET_MASK,			/* binary */

	/* add new constants above here */
	__ETHTOOL_A_BITSET_CNT,
	ETHTOOL_A_BITSET_MAX = __ETHTOOL_A_BITSET_CNT - 1
};

/* RINGS */

enum {
	ETHTOOL_TCP_DATA_SPLIT_UNKNOWN = 0,
	ETHTOOL_TCP_DATA_SPLIT_DISABLED,
	ETHTOOL_TCP_DATA_SPLIT_ENABLED,
};

enum {
	ETHTOOL_A_RINGS_UNSPEC,
	ETHTOOL_A_RINGS_HEADER,				/* nest - _A_HEADER_* */
	ETHTOOL_A_RINGS_RX_MAX,				/* u32 */
	ETHTOOL_A_RINGS_RX_MINI_MAX,			/* u32 */
	ETHTOOL_A_RINGS_RX_JUMBO_MAX,			/* u32 */
	ETHTOOL_A_RINGS_TX_MAX,				/* u32 */
	ETHTOOL_A_RINGS_RX,				/* u32 */
	ETHTOOL_A_RINGS_RX_MINI,			/* u32 */
	ETHTOOL_A_RINGS_RX_JUMBO,			/* u32 */
	ETHTOOL_A_RINGS_TX,				/* u32 */
	ETHTOOL_A_RINGS_RX_BUF_LEN,                     /* u32 */
	ETHTOOL_A_RINGS_TCP_DATA_SPLIT,			/* u8 */
	ETHTOOL_A_RINGS_CQE_SIZE,			/* u32 */

	/* add new constants above here */
	__ETHTOOL_A_RINGS_CNT,
	ETHTOOL_A_RINGS_MAX = (__ETHTOOL_A_RINGS_CNT - 1)
};

/* generic netlink info */
#define ETHTOOL_GENL_NAME "ethtool"
#define ETHTOOL_GENL_VERSION 1

#define ETHTOOL_MCGRP_MONITOR_NAME "monitor"

#endif /* _UAPI_LINUX_ETHTOOL_NETLINK_H_ */
