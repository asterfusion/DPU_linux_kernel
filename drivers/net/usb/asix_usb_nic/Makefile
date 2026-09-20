# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_USB_NET_AX88179_178A) += ax_usb_nic.o

ax_usb_nic-y := \
	ax_main.o \
	ax88179_178a.o \
	ax88179a_772d.o

ax_usb_nic-$(CONFIG_USB_AX_USB_NIC_PTP) += ax_ptp.o

ccflags-y += -fno-pie
