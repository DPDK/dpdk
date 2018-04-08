..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP

Rawdevice Library
=================

Introduction
------------

In terms of device flavor (type) support, DPDK currently has ethernet
(lib_ether), cryptodev (libcryptodev), eventdev (libeventdev) and vdev
(virtual device) support.

For a new type of device, for example an accelerator, there are not many
options except:
1. create another lib/librte_MySpecialDev, driver/MySpecialDrv and use it
through Bus/PMD model.
2. Or, create a vdev and implement necessary custom APIs which are directly
exposed from driver layer. However this may still require changes in bus code
in DPDK.

The DPDK Rawdev library is an abstraction that provides the DPDK framework a
way to manage such devices in a generic manner without expecting changes to
library or EAL for each device type. This library provides a generic set of
operations and APIs for framework and Applications to use, respectively, for
interfacing with such type of devices.

Design
------

Key factors guiding design of the Rawdevice library:

1. Following are some generic operations which can be treated as applicable
   to a large subset of device types. None of the operations are mandatory to
   be implemented by a driver. Application should also be design for proper
   handling for unsupported APIs.

  * Device Start/Stop - In some cases, 'reset' might also be required which
    has different semantics than a start-stop-start cycle.
  * Configuration - Device, Queue or any other sub-system configuration
  * I/O - Sending a series of buffers which can enclose any arbitrary data
  * Statistics - Fetch arbitrary device statistics
  * Firmware Management - Firmware load/unload/status

2. Application API should be able to pass along arbitrary state information
   to/from device driver. This can be achieved by maintaining context
   information through opaque data or pointers.

Figure below outlines the layout of the rawdevice library and device vis-a-vis
other well known device types like eth and crypto:

.. code-block:: console

     +-----------------------------------------------------------+
     |                        Application(s)                     |
     +------------------------------.----------------------------+
                                    |
                                    |
     +------------------------------'----------------------------+
     |                     DPDK Framework (APIs)                 |
     +--------------|----|-----------------|---------------------+
                   /      \                 \
            (crypto ops)  (eth ops)      (rawdev ops)        +----+
            /               \                 \              |DrvA|
     +-----'---+        +----`----+        +---'-----+       +----+
     | crypto  |        | ethdev  |        | raw     |
     +--/------+        +---/-----+        +----/----+       +----+
       /\                __/\                  /   ..........|DrvB|
      /  \              /    \                / ../    \     +----+
  +====+ +====+    +====+ +====+            +==/=+      ```Bus Probe
  |DevA| |DevB|    |DevC| |DevD|            |DevF|
  +====+ +====+    +====+ +====+            +====+
    |      |        |      |                 |
  ``|``````|````````|``````|`````````````````|````````Bus Scan
   (PCI)   |       (PCI)  (PCI)            (PCI)
         (BusA)

 * It is assumed above that DrvB is a PCI type driver which registers itself
   with PCI Bus
 * Thereafter, when the PCI scan is done, during probe DrvB would match the
   rawdev DevF ID and take control of device
 * Applications can then continue using the device through rawdev API
   interfaces


Device Identification
~~~~~~~~~~~~~~~~~~~~~

Physical rawdev devices are discovered during the Bus scan executed at DPDK
initialization, based on their identification and probing with corresponding
driver. Thus, a generic device needs to have an identifier and a driver
capable of identifying it through this identifier.

Virtual devices can be created by two mechanisms, either using the EAL command
line options or from within the application using an EAL API directly.

From the command line using the --vdev EAL option

.. code-block:: console

   --vdev 'rawdev_dev1'

Our using the rte_vdev_init API within the application code.

.. code-block:: c

    rte_vdev_init("rawdev_dev1", NULL)
