.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2026 Intel Corporation.

Device Arguments
================

Introduction
------------

Device arguments (devargs) provide a standardized way
to specify and configure devices in DPDK applications.
Devargs are used both during EAL initialization
(via ``rte_eal_init()`` command-line options)
and at runtime (via hotplug API)
to identify devices and pass configuration parameters to them.

A devargs string can specify identifiers and arguments at multiple levels:

Bus identifier and arguments
  Which bus handles the device (e.g., ``pci``, ``vdev``) and bus-level parameters.

Class identifier and arguments
  Device class (e.g., ``eth``, ``crypto``) and class-level parameters.

Driver identifier and arguments
  The specific driver and its driver-specific parameters.

DPDK supports two devargs syntaxes:

#. **Multi-layer syntax** -
   Generic syntax for device filtering by bus, class, or driver.
#. **Simplified syntax** -
   Legacy simplified syntax.


Devargs Syntax
--------------

Generic Syntax
~~~~~~~~~~~~~~

Generic devargs syntax is meant to cover all use cases supported by devargs infrastructure,
such as passing arguments to various layers or matching multiple devices.
The basic syntax format is as follows:

.. code-block:: none

   bus=<busname>,<bus_args>/class=<classname>,<class_args>/driver=<drvname>,<driver_args>

This allows the user to specify:

* ``bus`` layer: bus identifier, as well as bus-level arguments
* ``class`` layer: class identifier, as well as class-level arguments
* ``driver`` layer: driver identifier, as well as driver-specific parameters

.. note::

   By default, multi-layer syntax is driver-centric
   and will match all devices using a particular bus, class, and driver combination.
   To target a specific device, provide a device-identifying argument to the bus layer.
   The specific key depends on the bus:
   for PCI use ``addr=`` (e.g. ``bus=pci,addr=02:00.0``),
   for most other buses use ``name=`` (e.g. ``bus=vdev,name=net_ring0``).
   As far as PCI BDFs go, the domain part (``0000:``) can be omitted if it is zero,
   so ``0000:02:00.0`` and ``02:00.0`` are equivalent.

**Example**:

.. code-block:: none

   bus=pci/class=eth/driver=ice,representor=[0-3]

The above example will pass ``representor=[0-3]`` devarg
to each device matching the provided criteria
(PCI bus, Ethernet class, in use by ``ice`` driver).

.. code-block:: none

   bus=pci,addr=02:00.0/driver=ice,representor=[0-3]

The above example will pass ``representor=[0-3]`` devarg
only to device corresponding to PCI address ``0000:02:00.0``,
and only if it is managed by ``ice`` driver.


Simplified syntax
~~~~~~~~~~~~~~~~~

In cases where the full syntax is not required,
devargs can be used with simplified format that targets specific devices only:

.. code-block:: none

   [bus:]device_identifier[,arg1=val1,arg2=val2,...]

Where:

* ``bus:`` is an optional prefix specifying the bus name (pci, vdev, auxiliary, etc.)
* ``device_identifier`` uniquely identifies the device on its bus
* ``arg1=val1,arg2=val2,...`` are optional driver-specific configuration parameters

The arguments are provided to the driver,
but the driver name itself is inferred from the device
and does not need to be specified.

**Examples**:

.. code-block:: none

   0000:02:00.0                          # PCI device, no arguments
   02:00.0,txq_inline=128                # PCI device with driver arguments
   pci:0000:02:00.0                      # Explicit bus prefix
   net_ring0                             # Virtual device
   vdev:net_pcap0,rx_pcap=input.pcap     # Virtual device with arguments and bus prefix

If the bus prefix is omitted, DPDK tries each registered bus in turn
to see if it can recognize the device identifier.


Virtual Device Arguments
------------------------

Virtual devices (vdevs) are software-based devices
that don't correspond to physical hardware.
Unlike PCI or other hardware buses where devices are discovered by scanning,
virtual devices must be explicitly created by specifying their driver name.


Understanding Virtual Device Naming
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A virtual device name consists of:

Driver name
   The vdev driver to use (e.g., ``net_ring``, ``net_pcap``, ``crypto_null``).
Instance identifier
   A numeric suffix to create multiple instances of the same driver.

**Format**:

.. code-block:: none

   [vdev:]<driver_name><instance_number>[,arg=val,...]

**Examples**:

Create a ring-based virtual port:

.. code-block:: none

   net_ring0

Create a PCAP virtual device with input file:

.. code-block:: none

   net_pcap0,rx_pcap=input.pcap


Using Devargs in Applications
-----------------------------

At EAL Initialization
~~~~~~~~~~~~~~~~~~~~~

Devargs can be specified on the command line when starting a DPDK application:

**Allow-list (permit specific devices)**:

.. code-block:: console

   ./myapp -a 0000:02:00.0 -a 03:00.0,txq_inline=128

**Block-list (exclude specific devices)**:

.. code-block:: console

   ./myapp -b 04:00.0

**Virtual devices**:

.. code-block:: console

   ./myapp --vdev net_ring0 --vdev 'net_pcap0,rx_pcap=input.pcap'

See :doc:`../linux_gsg/linux_eal_parameters` for complete EAL parameter documentation.


At Runtime (Hotplug)
~~~~~~~~~~~~~~~~~~~~

Devargs are used with hotplug APIs to attach devices dynamically:

.. code-block:: c

   #include <rte_dev.h>

   /* Attach a PCI device */
   ret = rte_dev_probe("0000:05:00.0");
   if (ret < 0) {
       /* Handle error */
   }

   /* Attach a virtual device */
   ret = rte_dev_probe("net_ring1");
   if (ret < 0) {
       /* Handle error */
   }

   /* Attach with arguments */
   ret = rte_dev_probe("05:00.0,txq_inline=256");
   if (ret < 0) {
       /* Handle error */
   }

See :doc:`device_hotplug` for detailed information on runtime device management.


Parsing Devargs in Drivers
--------------------------

PMD drivers can parse devargs using the kvargs library:

.. code-block:: c

   #include <rte_kvargs.h>

   static int
   my_driver_parse_devargs(struct rte_devargs *devargs)
   {
       struct rte_kvargs *kvlist;
       const char *valid_args[] = {"arg1", "arg2", NULL};

       if (devargs == NULL || devargs->args == NULL)
           return 0;

       kvlist = rte_kvargs_parse(devargs->args, valid_args);
       if (kvlist == NULL) {
           RTE_LOG(ERR, PMD, "Failed to parse devargs\n");
           return -EINVAL;
       }

       /* Process each argument */
       rte_kvargs_process(kvlist, "arg1", my_arg1_handler, NULL);
       rte_kvargs_process(kvlist, "arg2", my_arg2_handler, NULL);

       rte_kvargs_free(kvlist);
       return 0;
   }

For Ethernet devices, use ``rte_eth_devargs_parse()``
to parse standard Ethernet arguments like representors:

.. code-block:: c

   struct rte_eth_devargs eth_da[RTE_MAX_ETHPORTS];
   int n_devargs;

   n_devargs = rte_eth_devargs_parse(devargs_str, eth_da, RTE_MAX_ETHPORTS);
   if (n_devargs < 0) {
       /* Handle error */
   }


Finding Devices by Devargs
--------------------------

DPDK provides iterator helpers that accept devargs-style filters for device enumeration.
Examples:

.. code-block:: c

   RTE_DEV_FOREACH(dev, "bus=pci", &it) {
      /* Handle each PCI device */
   }

   /* Ethernet devices can also be filtered by class */
   RTE_ETH_FOREACH_MATCHING_DEV(port_id, "class=eth,mac=00:11:22:33:44:55", &it) {
      /* Handle each Ethernet device */
   }

For more information, see:

* API documentation: ``rte_devargs_parse()``, ``rte_devargs_layers_parse()`` (in ``rte_devargs.h``)
* API documentation: ``rte_eth_devargs_parse()`` (in ``ethdev_driver.h``)
