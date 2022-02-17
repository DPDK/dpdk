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

In order to allow using only subset of available GPIOs `allowlist` PMD param may
be used. For example passing `--vdev=cnxk_gpio,gpiochip=448,allowlist=[0,1,2,3]`
to EAL will deny using all GPIOs except those specified explicitly in the
`allowlist`.

Respective queue needs to be configured with ``rte_rawdev_queue_setup()``. This
call barely exports GPIO to userspace.

To perform actual data transfer use standard ``rte_rawdev_enqueue_buffers()``
and ``rte_rawdev_dequeue_buffers()`` APIs. Not all messages produce sensible
responses hence dequeueing is not always necessary.

CNXK GPIO PMD
-------------

PMD accepts ``struct cnxk_gpio_msg`` messages which differ by type and payload.
Message types along with description are listed below. As for the usage examples
please refer to ``cnxk_gpio_selftest()``. There's a set of convenient wrappers
available, one for each existing command.

Set GPIO value
~~~~~~~~~~~~~~

Message is used to set output to low or high. This does not work for GPIOs
configured as input.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_SET_PIN_VALUE``.

Payload must be an integer set to 0 (low) or 1 (high).

Consider using ``rte_pmd_gpio_set_pin_value()`` wrapper.

Set GPIO edge
~~~~~~~~~~~~~

Message is used to set edge that triggers interrupt.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_SET_PIN_EDGE``.

Payload must be `enum cnxk_gpio_pin_edge`.

Consider using ``rte_pmd_gpio_set_pin_edge()`` wrapper.

Set GPIO direction
~~~~~~~~~~~~~~~~~~

Message is used to change GPIO direction to either input or output.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_SET_PIN_DIR``.

Payload must be `enum cnxk_gpio_pin_dir`.

Consider using ``rte_pmd_gpio_set_pin_dir()`` wrapper.

Set GPIO active low
~~~~~~~~~~~~~~~~~~~

Message is used to set whether pin is active low.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_SET_PIN_ACTIVE_LOW``.

Payload must be an integer set to 0 or 1. The latter activates inversion.

Consider using ``rte_pmd_gpio_set_pin_active_low()`` wrapper.

Get GPIO value
~~~~~~~~~~~~~~

Message is used to read GPIO value. Value can be 0 (low) or 1 (high).

Message must have type set to ``CNXK_GPIO_MSG_TYPE_GET_PIN_VALUE``.

Payload contains integer set to either 0 or 1.

Consider using ``rte_pmd_gpio_get_pin_value()`` wrapper.

Get GPIO edge
~~~~~~~~~~~~~

Message is used to read GPIO edge.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_GET_PIN_EDGE``.

Payload contains `enum cnxk_gpio_pin_edge`.

Consider using ``rte_pmd_gpio_get_pin_edge()`` wrapper.

Get GPIO direction
~~~~~~~~~~~~~~~~~~

Message is used to read GPIO direction.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_GET_PIN_DIR``.

Payload contains `enum cnxk_gpio_pin_dir`.

Consider using ``rte_pmd_gpio_get_pin_dir()`` wrapper.

Get GPIO active low
~~~~~~~~~~~~~~~~~~~

Message is used check whether inverted logic is active.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_GET_PIN_ACTIVE_LOW``.

Payload contains an integer set to 0 or 1. The latter means inverted logic
is turned on.

Consider using ``rte_pmd_gpio_get_pin_active_low()`` wrapper.

Request interrupt
~~~~~~~~~~~~~~~~~

Message is used to install custom interrupt handler.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_REGISTER_IRQ``.

Payload needs to be set to ``struct cnxk_gpio_irq`` which describes interrupt
being requested.

Consider using ``rte_pmd_gpio_register_gpio()`` wrapper.

Free interrupt
~~~~~~~~~~~~~~

Message is used to remove installed interrupt handler.

Message must have type set to ``CNXK_GPIO_MSG_TYPE_UNREGISTER_IRQ``.

Consider using ``rte_pmd_gpio_unregister_gpio()`` wrapper.

Self test
---------

On EAL initialization CNXK GPIO device will be probed and populated into
the list of raw devices on condition ``--vdev=cnxk_gpio,gpiochip=<number>`` was
passed. ``rte_rawdev_get_dev_id("CNXK_GPIO")`` returns unique device id. Use
this identifier for further rawdev function calls.

Selftest rawdev API can be used to verify the PMD functionality. Note it blindly
assumes that all GPIOs are controllable so some errors during test are expected.
