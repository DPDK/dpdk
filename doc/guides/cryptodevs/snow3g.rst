..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
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

SNOW 3G Crypto Poll Mode Driver
===============================

The SNOW 3G PMD (**librte_pmd_snow3g**) provides poll mode crypto driver
support for utilizing Intel Libsso library, which implements F8 and F9 functions
for SNOW 3G UEA2 cipher and UIA2 hash algorithms.

Features
--------

SNOW 3G PMD has support for:

Cipher algorithm:

* RTE_CRYPTO_SYM_CIPHER_SNOW3G_UEA2

Authentication algorithm:

* RTE_CRYPTO_SYM_AUTH_SNOW3G_UIA2

Limitations
-----------

* Chained mbufs are not supported.
* Snow3g(UEA2) supported only if cipher length, cipher offset fields are byte-aligned.
* Snow3g(UIA2) supported only if hash length, hash offset fields are byte-aligned.

Installation
------------

To build DPDK with the SNOW3G_PMD the user is required to get
the export controlled libsso library, sending a request to
`DPDKUser_software_access@intel.com`, and compile it
on their user system before building DPDK:

.. code-block:: console

	make -f Makefile_snow3g

The environmental variable LIBSSO_PATH must be exported with the path
where you extracted and built the libsso library and finally set
CONFIG_RTE_LIBRTE_PMD_SNOW3G=y in config/common_base.
