..  BSD LICENSE
    Copyright (C) Cavium, Inc. 2017. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Cavium, Inc nor the names of its
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

OCTEONTX Board Support Package
==============================

This doc has information about steps to setup octeontx platform
and information about common offload hw block drivers of
**Cavium OCTEONTX** SoC family.


More information about SoC can be found at `Cavium, Inc Official Website
<http://www.cavium.com/OCTEON-TX_ARM_Processors.html>`_.

Common Offload HW Block Drivers
-------------------------------

1. **Eventdev Driver**
   See :doc: `../eventdevs/octeontx.rst` for octeontx ssovf eventdev driver
   information.

2. **Mempool Driver**
   See :doc: `../mempool/octeontx.rst` for octeontx fpavf mempool driver
   information.

Steps To Setup Platform
-----------------------

There are three main pre-prerequisites for setting up Platform drivers on
OCTEONTX compatible board:

1. **OCTEONTX Linux kernel PF driver for Network acceleration HW blocks**

   The OCTEONTX Linux kernel drivers (includes the required PF driver for the
   Platform drivers) are available on Github at `octeontx-kmod <https://github.com/caviumnetworks/octeontx-kmod>`_
   along with build, install and dpdk usage instructions.

2. **ARM64 Tool Chain**

   For example, the *aarch64* Linaro Toolchain, which can be obtained from
   `here <https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/aarch64-linux-gnu>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `<http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

   As an alternative method, Platform drivers can also be executed using images provided
   as part of SDK from Cavium. The SDK includes all the above prerequisites necessary
   to bring up a OCTEONTX board.

   SDK and related information can be obtained from: `Cavium support site <https://support.cavium.com/>`_.

- Follow the DPDK :doc: `../linux_gsg/index.rst` to setup the basic DPDK environment.
