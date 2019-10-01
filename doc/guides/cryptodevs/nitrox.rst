..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2019 Marvell International Ltd.

Marvell NITROX Crypto Poll Mode Driver
======================================

The Nitrox crypto poll mode driver provides support for offloading
cryptographic operations to the NITROX V security processor. Detailed
information about the NITROX V security processor can be obtained here:

* https://www.marvell.com/security-solutions/nitrox-security-processors/nitrox-v/

Installation
------------

For compiling the Nitrox crypto PMD, please check if the
CONFIG_RTE_LIBRTE_PMD_NITROX setting is set to `y` in config/common_base file.

* ``CONFIG_RTE_LIBRTE_PMD_NITROX=y``

Initialization
--------------

Nitrox crypto PMD depend on Nitrox kernel PF driver being installed on the
platform. Nitrox PF driver is required to create VF devices which will
be used by the PMD. Each VF device can enable one cryptodev PMD.

Nitrox kernel PF driver is available as part of CNN55XX-Driver SDK. The SDK
and it's installation instructions can be obtained from:
`Marvell Technical Documentation Portal <https://support.cavium.com/>`_.
