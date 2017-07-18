..  BSD LICENSE
    Copyright 2017 6WIND S.A.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of 6WIND S.A. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Fail-safe poll mode driver library
==================================

The Fail-safe poll mode driver library (**librte_pmd_failsafe**) is a virtual
device that allows using any device supporting hotplug (sudden device removal
and plugging on its bus), without modifying other components relying on such
device (application, other PMDs).

Additionally to the Seamless Hotplug feature, the Fail-safe PMD offers the
ability to redirect operations to secondary devices when the primary has been
removed from the system.

.. note::

   The library is enabled by default. You can enable it or disable it manually
   by setting the ``CONFIG_RTE_LIBRTE_PMD_FAILSAFE`` configuration option.

Features
--------

The Fail-safe PMD only supports a limited set of features. If you plan to use a
device underneath the Fail-safe PMD with a specific feature, this feature must
be supported by the Fail-safe PMD to avoid throwing any error.

A notable exception is the device removal feature. The fail-safe PMD being a
virtual device, it cannot currently be removed in the sense of a specific bus
hotplug, like for PCI for example. It will however enable this feature for its
sub-device automatically, detecting those that are capable and register the
relevant callback for such event.

Check the feature matrix for the complete set of supported features.

Compilation option
------------------

This option can be modified in the ``$RTE_TARGET/build/.config`` file.

- ``CONFIG_RTE_LIBRTE_PMD_FAILSAFE`` (default **y**)

  Toggle compiling librte_pmd_failsafe.

Using the Fail-safe PMD from the EAL command line
-------------------------------------------------

The Fail-safe PMD can be used like most other DPDK virtual devices, by passing a
``--vdev`` parameter to the EAL when starting the application. The device name
must start with the *net_failsafe* prefix, followed by numbers or letters. This
name must be unique for each device. Each fail-safe instance must have at least one
sub-device, up to ``RTE_MAX_ETHPORTS-1``.

A sub-device can be any legal DPDK device, including possibly another fail-safe
instance.

Fail-safe command line parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **dev(<iface>)** parameter

  This parameter allows the user to define a sub-device. The ``<iface>`` part of
  this parameter must be a valid device definition. It could be the argument
  provided to any ``-w`` device specification or the argument that would be
  given to a ``--vdev`` parameter (including a fail-safe).
  Enclosing the device definition within parenthesis here allows using
  additional sub-device parameters if need be. They will be passed on to the
  sub-device.

- **exec(<shell command>)** parameter

  This parameter allows the user to provide a command to the fail-safe PMD to
  execute and define a sub-device.
  It is done within a regular shell context.
  The first line of its output is read by the fail-safe PMD and otherwise
  interpreted as if passed by the regular **dev** parameter.
  Any other line is discarded.
  If the command fail or output an incorrect string, the sub-device is not
  initialized.
  All commas within the ``shell command`` are replaced by spaces before
  executing the command. This helps using scripts to specify devices.

- **mac** parameter [MAC address]

  This parameter allows the user to set a default MAC address to the fail-safe
  and all of its sub-devices.
  If no default mac address is provided, the fail-safe PMD will read the MAC
  address of the first of its sub-device to be successfully probed and use it as
  its default MAC address, trying to set it to all of its other sub-devices.
  If no sub-device was successfully probed at initialization, then a random MAC
  address is generated, that will be subsequently applied to all sub-device once
  they are probed.

- **hotplug_poll** parameter [UINT64] (default **2000**)

  This parameter allows the user to configure the amount of time in milliseconds
  between two slave upkeep round.

Usage example
~~~~~~~~~~~~~

This section shows some example of using **testpmd** with a fail-safe PMD.

#. To build a PMD and configure DPDK, refer to the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`.

#. Start testpmd. The slave device should be blacklisted from normal EAL
   operations to avoid probing it twice when in PCI blacklist mode.

   .. code-block:: console

      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 \
         --vdev 'net_failsafe0,mac=de:ad:be:ef:01:02,dev(84:00.0),dev(net_ring0)'
         -b 84:00.0 -b 00:04.0 -- -i

   If the slave device being used is not blacklisted, it will be probed by the
   EAL first. When the fail-safe then tries to initialize it the probe operation
   fails.

   Note that PCI blacklist mode is the default PCI operating mode.

#. Alternatively, it can be used alongside any other device in whitelist mode.

   .. code-block:: console

      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 \
         --vdev 'net_failsafe0,mac=de:ad:be:ef:01:02,dev(84:00.0),dev(net_ring0)'
         -w 81:00.0 -- -i

#. Start testpmd using a flexible device definition

   .. code-block:: console

      $RTE_TARGET/build/app/testpmd -c 0xff -n 4 --no-pci \
         --vdev='net_failsafe0,exec(echo 84:00.0)' -- -i

Using the Fail-safe PMD from an application
-------------------------------------------

This driver strives to be as seamless as possible to existing applications, in
order to propose the hotplug functionality in the easiest way possible.

Care must be taken, however, to respect the **ether** API concerning device
access, and in particular, using the ``RTE_ETH_FOREACH_DEV`` macro to iterate
over ethernet devices, instead of directly accessing them or by writing one's
own device iterator.

Plug-in feature
---------------

A sub-device can be defined without existing on the system when the fail-safe
PMD is initialized. Upon probing this device, the fail-safe PMD will detect its
absence and postpone its use. It will then register for a periodic check on any
missing sub-device.

During this time, the fail-safe PMD can be used normally, configured and told to
emit and receive packets. It will store any applied configuration, and try to
apply it upon the probing of its missing sub-device. After this configuration
pass, the new sub-device will be synchronized with other sub-devices, i.e. be
started if the fail-safe PMD has been started by the user before.

Plug-out feature
----------------

A sub-device supporting the device removal event can be removed from its bus at
any time. The fail-safe PMD will register a callback for such event and react
accordingly. It will try to safely stop, close and uninit the sub-device having
emitted this event, allowing it to free its eventual resources.

Fail-safe glossary
------------------

Fallback device : Secondary device
    The fail-safe will fail-over onto this device when the preferred device is
    absent.

Preferred device : Primary device
    The first declared sub-device in the fail-safe parameters.
    When this device is plugged, it is always used as emitting device.
    It is the main sub-device and is used as target for configuration
    operations if there is any ambiguity.

Upkeep round
    Periodical process when slaves are serviced. Each devices having a state
    different to that of the fail-safe device itself, is synchronized with it.
    Additionally, each slave having the remove flag set are cleaned-up.

Slave
    In the context of the fail-safe PMD, synonymous to sub-device.

Sub-device
    A device being utilized by the fail-safe PMD.
    This is another PMD running underneath the fail-safe PMD.
    Any sub-device can disappear at any time. The fail-safe will ensure
    that the device removal happens gracefully.
