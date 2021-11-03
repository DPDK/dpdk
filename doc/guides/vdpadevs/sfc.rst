..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Xilinx Corporation.

Xilinx vDPA driver
==================

The Xilinx vDPA (vhost data path acceleration) driver (**librte_pmd_sfc_vdpa**)
provides support for the Xilinx SN1022 SmartNICs family of 10/25/40/50/100 Gbps
adapters that have support for latest Linux and FreeBSD operating systems.

More information can be found at Xilinx website https://www.xilinx.com.


Xilinx vDPA implementation
--------------------------

ef100 device can be configured in the net device or vDPA mode.
Adding "class=vdpa" parameter helps to specify that this
device is to be used in vDPA mode. If this parameter is not specified, device
will be probed by net/sfc driver and will used as a net device.

This PMD uses libefx (common/sfc_efx) code to access the device firmware.


Supported NICs
--------------

- Xilinx SN1022 SmartNICs


Features
--------

Features of the Xilinx vDPA driver are:

- Compatibility with virtio 0.95 and 1.0


Non-supported Features
----------------------

- Control Queue
- Multi queue
- Live Migration


Prerequisites
-------------

Requires firmware version: v1.0.7.0 or higher

Visit `Xilinx Support Downloads <https://www.xilinx.com/support.html>`_
to get Xilinx Utilities with the latest firmware.
Follow instructions from Alveo SN1000 SmartNICs User Guide to
update firmware and configure the adapter.


Per-Device Parameters
~~~~~~~~~~~~~~~~~~~~~

The following per-device parameters can be passed via EAL PCI device
allowlist option like "-a 02:00.0,arg1=value1,...".

Case-insensitive 1/y/yes/on or 0/n/no/off may be used to specify
boolean parameters value.

- ``class`` [net|vdpa] (default **net**)

  Choose the mode of operation of ef100 device.
  **net** device will work as network device and will be probed by net/sfc driver.
  **vdpa** device will work as vdpa device and will be probed by vdpa/sfc driver.
  If this parameter is not specified then ef100 device will operate as network device.

- ``mac`` [mac address]

  Configures MAC address which would be used to setup MAC filters.


Dynamic Logging Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~

One may leverage EAL option "--log-level" to change default levels
for the log types supported by the driver. The option is used with
an argument typically consisting of two parts separated by a colon.

Level value is the last part which takes a symbolic name (or integer).
Log type is the former part which may shell match syntax.
Depending on the choice of the expression, the given log level may
be used either for some specific log type or for a subset of types.

SFC vDPA PMD provides the following log types available for control:

- ``pmd.vdpa.sfc.driver`` (default level is **notice**)

  Affects driver-wide messages unrelated to any particular devices.

- ``pmd.vdpa.sfc.main`` (default level is **notice**)

  Matches a subset of per-port log types registered during runtime.
  A full name for a particular type may be obtained by appending a
  dot and a PCI device identifier (``XXXX:XX:XX.X``) to the prefix.

- ``pmd.vdpa.sfc.mcdi`` (default level is **notice**)

  Extra logging of the communication with the NIC's management CPU.
  The format of the log is consumed by the netlogdecode cross-platform
  tool. May be managed per-port, as explained above.
