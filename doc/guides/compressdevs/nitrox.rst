..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2024 Marvell.

Marvell NITROX Compression Poll Mode Driver
===========================================

The Nitrox compression poll mode driver provides support for offloading
compression and decompression operations to the NITROX V processor.
Detailed information about the NITROX V processor can be obtained here:

* https://www.marvell.com/security-solutions/nitrox-security-processors/nitrox-v/

Features
--------

NITROX V compression PMD has support for:

Compression/Decompression algorithm:

* DEFLATE

Huffman code type:

* FIXED
* DYNAMIC

Window size support:

* Min - 2 bytes
* Max - 32KB

Checksum generation:

* CRC32, Adler

Limitations
-----------

* Compressdev level 0, no compression, is not supported.

Initialization
--------------

Nitrox compression PMD depends on Nitrox kernel PF driver being installed on the platform.
Nitrox PF driver is required to create VF devices which will be used by the PMD.
Each VF device can enable one compressdev PMD.

Nitrox kernel PF driver is available as part of CNN55XX-Driver SDK.
The SDK and its installation instructions can be obtained from:
`Marvell Customer Portal <https://www.marvell.com/support/extranets.html>`_.
