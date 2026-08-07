===============================
CPLD GPIO update userspace API
===============================

The ``cpld-gpio-update`` driver exposes one exclusive misc device per
firmware node.  Device-tree aliases named ``cpldupdateN`` produce stable
device names such as ``/dev/cpld-update0``.

The driver only generates GPIO waveforms.  Firmware parsing and the CPLD
programming protocol remain in userspace.  Opening a device releases both
open-drain lines.  A second open returns ``EBUSY``.  Closing the descriptor,
including after process termination, releases both lines again.

Applications include ``<linux/cpld-gpio-update.h>`` and use these ioctls:

``CPLD_GPIO_IOC_SET_SCL``
  Set the physical SCL level from a ``__u32`` value.  Zero pulls the line low
  and one releases it high.

``CPLD_GPIO_IOC_SET_SDA``
  Set the physical SDA level using the same zero/one convention.

``CPLD_GPIO_IOC_GET_SDA``
  Return the physical SDA level in a ``__u32`` value.

``CPLD_GPIO_IOC_XFER``
  Transfer between 1 and 8192 bytes using ``struct cpld_gpio_ioc_xfer``.
  Bytes are shifted most-significant bit first.  The operation does not emit
  a start condition, stop condition, or device address; userspace creates any
  required framing with the line-control ioctls.

For ``CPLD_GPIO_XFER_WRITE``, the driver samples an acknowledge bit after
each byte and returns ``EIO`` on NAK.  Setting
``CPLD_GPIO_XFER_F_IGNORE_NAK`` continues after NAK.  For
``CPLD_GPIO_XFER_READ``, the driver sends ACK after every byte except the
last, where it sends NAK.  ``CPLD_GPIO_XFER_F_NO_RD_ACK`` suppresses all
automatic ACK/NAK cycles.

All reserved fields and unsupported flags must be zero.  The fixed-width,
aligned UAPI has the same layout for native and compat processes.
