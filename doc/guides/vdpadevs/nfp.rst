.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2023 Corigine, Inc.

NFP vDPA driver
===============

The NFP vDPA (vhost data path acceleration) driver (**librte_nfp_vdpa**)
provides support for the Netronome and Corigine's
NFP-6xxx, NFP-4xxx and NFP-38xx product lines.

NFP vDPA implementation
-----------------------

NFP VF device can be configured in the net device or vDPA mode.
Adding "class=vdpa" parameter helps to specify
that this device is to be used in vDPA mode.
If this parameter is not specified,
device will be probed by net/nfp driver and will used as a VF net device.

This PMD uses (common/nfp) code to access the device firmware.

Per-Device Parameters
~~~~~~~~~~~~~~~~~~~~~

The following per-device parameters can be passed via EAL PCI device
allow-list option like "-a 02:00.0,arg1=value1,...".

- ``class`` [net|vdpa] (default **net**)

  Choose the mode of operation of nfp device.
  **net** device will work as network device and will be probed by net/nfp driver.
  **vdpa** device will work as vdpa device and will be probed by vdpa/nfp driver.
  If this parameter is not specified then nfp device will operate as network device.

Dynamic Logging Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~

One may leverage EAL option "--log-level" to change default levels
for the log types supported by the driver.
The option is used with an argument
typically consisting of two parts separated by a colon.

Level value is the last part which takes a symbolic name (or integer).
Log type is the former part which may shell match syntax.
Depending on the choice of the expression, the given log level may
be used either for some specific log type or for a subset of types.

NFP vDPA PMD provides the following log types available for control:

- ``pmd.vdpa.nfp.vdpa`` (default level is **notice**)

  Affects driver-wide messages unrelated to any particular devices.

- ``pmd.vdpa.nfp.core`` (default level is **notice**)

  Affects the core logic of this PMD.
