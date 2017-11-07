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

OCTEONTX FPAVF Mempool Driver
=============================

The OCTEONTX FPAVF PMD (**librte_mempool_octeontx**) is a mempool
driver for offload mempool device found in **Cavium OCTEONTX** SoC
family.

More information can be found at `Cavium, Inc Official Website
<http://www.cavium.com/OCTEON-TX_ARM_Processors.html>`_.

Features
--------

Features of the OCTEONTX FPAVF PMD are:

- 32 SR-IOV Virtual functions
- 32 Pools
- HW mempool manager

Supported OCTEONTX SoCs
-----------------------

- CN83xx

Prerequisites
-------------

See :doc: `../platform/octeontx.rst` for setup information.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_MBUF_DEFAULT_MEMPOOL_OPS`` ( set to ``octeontx_fpavf``)

  Set default mempool ops to octeontx_fpavf.

- ``CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL`` (default ``y``)

  Toggle compilation of the ``librte_mempool_octeontx`` driver.

- ``CONFIG_RTE_LIBRTE_OCTEONTX_MEMPOOL_DEBUG`` (default ``n``)

  Toggle display of generic debugging messages

Driver Compilation
~~~~~~~~~~~~~~~~~~

To compile the OCTEONTX FPAVF MEMPOOL PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-thunderx-linuxapp-gcc test-build


Initialization
--------------

The octeontx fpavf mempool initialization similar to other mempool
drivers like ring. However user need to pass --base-virtaddr as
command line input to application example test_mempool.c application.

Example:

.. code-block:: console

    ./build/app/test -c 0xf --base-virtaddr=0x100000000000 \
                        --mbuf-pool-ops-name="octeontx_fpavf"
