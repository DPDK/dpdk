..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.
    All rights reserved.

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

FM10K Poll Mode Driver
======================

The FM10K poll mode driver library provides support for the Intel FM10000
(FM10K) family of 40GbE/100GbE adapters.


Limitations
-----------


Switch manager
~~~~~~~~~~~~~~

The Intel FM10000 family of NICs integrate a hardware switch and multiple host
interfaces. The FM10000 PMD driver only manages host interfaces. For the
switch component another switch driver has to be loaded prior to to the
FM10000 PMD driver.  The switch driver can be acquired for Intel support or
from the `Match Interface <https://github.com/match-interface>`_ project.
Only Testpoint is validated with DPDK, the latest version that has been
validated with DPDK2.2 is 4.1.6.

CRC striping
~~~~~~~~~~~~

The FM10000 family of NICs strip the CRC for every packets coming into the
host interface.  So, CRC will be stripped even when the
``rxmode.hw_strip_crc`` member is set to 0 in ``struct rte_eth_conf``.


Maximum packet length
~~~~~~~~~~~~~~~~~~~~~

The FM10000 family of NICS support a maximum of a 15K jumbo frame. The value
is fixed and cannot be changed. So, even when the ``rxmode.max_rx_pkt_len``
member of ``struct rte_eth_conf`` is set to a value lower than 15364, frames
up to 15364 bytes can still reach the host interface.
