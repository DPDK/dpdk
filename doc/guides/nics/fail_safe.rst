..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 6WIND S.A.

Fail-safe poll mode driver library
==================================

The Fail-safe poll mode driver library (**librte_pmd_failsafe**) implements a
virtual device that allows using device supporting hotplug, without modifying
other components relying on such device (application, other PMDs).
In this context, hotplug support is meant as plugging or removing a device
from its bus suddenly.

Additionally to the Seamless Hotplug feature, the Fail-safe PMD offers the
ability to redirect operations to a secondary device when the primary has been
removed from the system.

.. note::

   The library is enabled by default. You can enable it or disable it manually
   by setting the ``CONFIG_RTE_LIBRTE_PMD_FAILSAFE`` configuration option.

Features
--------

The Fail-safe PMD only supports a limited set of features. If you plan to use a
device underneath the Fail-safe PMD with a specific feature, this feature must
also be supported by the Fail-safe PMD.

A notable exception is the device removal feature. The fail-safe PMD is not
meant to be removed itself, unlike its sub-devices which should support it.
If a sub-device supports hotplugging, the fail-safe PMD will enable its use
automatically by detecting capable devices and registering the relevant handler.

Check the feature matrix for the complete set of supported features.

Compilation option
------------------

Available options within the ``$RTE_TARGET/build/.config`` file:

- ``CONFIG_RTE_LIBRTE_PMD_FAILSAFE`` (default **y**)

  This option enables or disables compiling librte_pmd_failsafe.

Using the Fail-safe PMD from the EAL command line
-------------------------------------------------

The Fail-safe PMD can be used like most other DPDK virtual devices, by passing a
``--vdev`` parameter to the EAL when starting the application. The device name
must start with the *net_failsafe* prefix, followed by numbers or letters. This
name must be unique for each device. Each fail-safe instance must have at least one
sub-device, and at most two.

A sub-device can be any DPDK device, including possibly another fail-safe device.

Fail-safe command line parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **dev(<iface>)** parameter

  This parameter allows the user to define a sub-device. The ``<iface>`` part of
  this parameter must be a valid device definition. It follows the same format
  provided to any ``-w`` or ``--vdev`` options.

  Enclosing the device definition within parentheses here allows using
  additional sub-device parameters if need be. They will be passed on to the
  sub-device.

.. note::

   In case where the sub-device is also used as a whitelist device, using ``-w``
   on the EAL command line, the fail-safe PMD will use the device with the
   options provided to the EAL instead of its own parameters.

   When trying to use a PCI device automatically probed by the blacklist mode,
   the name for the fail-safe sub-device must be the full PCI id:
   Domain:Bus:Device.Function, *i.e.* ``00:00:00.0`` instead of ``00:00.0``,
   as the second form is historically accepted by the DPDK.

- **exec(<shell command>)** parameter

  This parameter allows the user to provide a command to the fail-safe PMD to
  execute and define a sub-device.
  It is done within a regular shell context.
  The first line of its output is read by the fail-safe PMD and otherwise
  interpreted as if passed to a **dev** parameter.
  Any other line is discarded.
  If the command fails or output an incorrect string, the sub-device is not
  initialized.
  All commas within the ``shell command`` are replaced by spaces before
  executing the command. This helps using scripts to specify devices.

- **fd(<file descriptor number>)** parameter

  This parameter reads a device definition from an arbitrary file descriptor
  number in ``<iface>`` format as described above.

  The file descriptor is read in non-blocking mode and is never closed in
  order to take only the last line into account (unlike ``exec()``) at every
  probe attempt.

- **mac** parameter [MAC address]

  This parameter allows the user to set a default MAC address to the fail-safe
  and all of its sub-devices.
  If no default mac address is provided, the fail-safe PMD will read the MAC
  address of the first of its sub-device to be successfully probed and use it as
  its default MAC address, trying to set it to all of its other sub-devices.
  If no sub-device was successfully probed at initialization, then a random MAC
  address is generated, that will be subsequently applied to all sub-devices once
  they are probed.

- **hotplug_poll** parameter [UINT64] (default **2000**)

  This parameter allows the user to configure the amount of time in milliseconds
  between two sub-device upkeep round.

Usage example
~~~~~~~~~~~~~

This section shows some example of using **testpmd** with a fail-safe PMD.

#. To build a PMD and configure DPDK, refer to the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`.

#. Start testpmd. The sub-device ``84:00.0`` should be blacklisted from normal EAL
   operations to avoid probing it twice, as the PCI bus is in blacklist mode.

   .. code-block:: console

      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 \
         --vdev 'net_failsafe0,mac=de:ad:be:ef:01:02,dev(84:00.0),dev(net_ring0)' \
         -b 84:00.0 -b 00:04.0 -- -i

   If the sub-device ``84:00.0`` is not blacklisted, it will be probed by the
   EAL first. When the fail-safe then tries to initialize it the probe operation
   fails.

   Note that PCI blacklist mode is the default PCI operating mode.

#. Alternatively, it can be used alongside any other device in whitelist mode.

   .. code-block:: console

      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 \
         --vdev 'net_failsafe0,mac=de:ad:be:ef:01:02,dev(84:00.0),dev(net_ring0)' \
         -w 81:00.0 -- -i

#. Start testpmd using a flexible device definition

   .. code-block:: console

      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 -w ff:ff.f \
         --vdev='net_failsafe0,exec(echo 84:00.0)' -- -i

#. Start testpmd, automatically probing the device 84:00.0 and using it with
   the fail-safe.
 
   .. code-block:: console
 
      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 \
         --vdev 'net_failsafe0,dev(0000:84:00.0),dev(net_ring0)' -- -i


Using the Fail-safe PMD from an application
-------------------------------------------

This driver strives to be as seamless as possible to existing applications, in
order to propose the hotplug functionality in the easiest way possible.

Care must be taken, however, to respect the **ether** API concerning device
access, and in particular, using the ``RTE_ETH_FOREACH_DEV`` macro to iterate
over ethernet devices, instead of directly accessing them or by writing one's
own device iterator.

   .. code-block:: C

      unsigned int i;

      /* VALID iteration over eth-dev. */
      RTE_ETH_FOREACH_DEV(i) {
              [...]
      }

      /* INVALID iteration over eth-dev. */
      for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
              [...]
      }

Plug-in feature
---------------

A sub-device can be defined without existing on the system when the fail-safe
PMD is initialized. Upon probing this device, the fail-safe PMD will detect its
absence and postpone its use. It will then register for a periodic check on any
missing sub-device.

During this time, the fail-safe PMD can be used normally, configured and told to
emit and receive packets. It will store any applied configuration but will fail
to emit anything, returning ``0`` from its TX function. Any unsent packet must
be freed.

Upon the probing of its missing sub-device, the current stored configuration
will be applied. After this configuration pass, the new sub-device will be
synchronized with other sub-devices, i.e. be started if the fail-safe PMD has
been started by the user before.

Plug-out feature
----------------

A sub-device supporting the device removal event can be removed from its bus at
any time. The fail-safe PMD will register a callback for such event and react
accordingly. It will try to safely stop, close and uninit the sub-device having
emitted this event, allowing it to free its eventual resources.

Fail-safe glossary
------------------

Fallback device
    Also called **Secondary device**.

    The fail-safe will fail-over onto this device when the preferred device is
    absent.

Preferred device
    Also called **Primary device**.

    The first declared sub-device in the fail-safe parameters.
    When this device is plugged, it is always used as emitting device.
    It is the main sub-device and is used as target for configuration
    operations if there is any ambiguity.

Upkeep round
    Periodical event during which sub-devices are serviced. Each devices having a state
    different to that of the fail-safe device itself, is synchronized with it
    (brought down or up accordingly). Additionally, any sub-device marked for
    removal is cleaned-up.

Slave
    In the context of the fail-safe PMD, synonymous to sub-device.

Sub-device
    A device being utilized by the fail-safe PMD.
    This is another PMD running underneath the fail-safe PMD.
    Any sub-device can disappear at any time. The fail-safe will ensure
    that the device removal happens gracefully.
