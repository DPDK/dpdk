..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Marvell.

Marvell CNXK GPIO Driver
========================

CNXK GPIO PMD configures and manages GPIOs available on the system using
standard enqueue/dequeue mechanism offered by raw device abstraction. PMD relies
both on standard sysfs GPIO interface provided by the Linux kernel and GPIO
kernel driver custom interface allowing one to install userspace interrupt
handlers.

Features
--------

Following features are available:

- export/unexport a GPIO
- read/write specific value from/to exported GPIO
- set GPIO direction
- set GPIO edge that triggers interrupt
- set GPIO active low
- register interrupt handler for specific GPIO

Requirements
------------

PMD relies on modified kernel GPIO driver which exposes ``ioctl()`` interface
for installing interrupt handlers for low latency signal processing.

Driver is shipped with Marvell SDK.

Device Setup
------------

CNXK GPIO PMD binds to virtual device which gets created by passing
`--vdev=cnxk_gpio,gpiochip=<number>` command line to EAL. `gpiochip` parameter
tells PMD which GPIO controller should be used. Available controllers are
available under `/sys/class/gpio`. For further details on how Linux represents
GPIOs in userspace please refer to
`sysfs.txt <https://www.kernel.org/doc/Documentation/gpio/sysfs.txt>`_.

If `gpiochip=<number>` was omitted then first gpiochip from the alphabetically
sort list of available gpiochips is used.

.. code-block:: console

   $ ls /sys/class/gpio
   export gpiochip448 unexport

In above scenario only one GPIO controller is present hence
`--vdev=cnxk_gpio,gpiochip=448` should be passed to EAL.

Before performing actual data transfer one needs to call
``rte_rawdev_queue_count()`` followed by ``rte_rawdev_queue_conf_get()``. The
former returns number GPIOs available in the system irrespective of GPIOs
being controllable or not. Thus it is user responsibility to pick the proper
ones. The latter call simply returns queue capacity.

Respective queue needs to be configured with ``rte_rawdev_queue_setup()``. This
call barely exports GPIO to userspace.

To perform actual data transfer use standard ``rte_rawdev_enqueue_buffers()``
and ``rte_rawdev_dequeue_buffers()`` APIs. Not all messages produce sensible
responses hence dequeueing is not always necessary.
