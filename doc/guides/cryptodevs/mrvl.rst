..  BSD LICENSE
    Copyright(c) 2017 Marvell International Ltd.
    Copyright(c) 2017 Semihalf.
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
      * Neither the name of the copyright holder nor the names of its
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

MRVL Crypto Poll Mode Driver
============================

The MRVL CRYPTO PMD (**librte_crypto_mrvl_pmd**) provides poll mode crypto driver
support by utilizing MUSDK library, which provides cryptographic operations
acceleration by using Security Acceleration Engine (EIP197) directly from
user-space with minimum overhead and high performance.

Features
--------

MRVL CRYPTO PMD has support for:

* Symmetric crypto
* Sym operation chaining
* AES CBC (128)
* AES CBC (192)
* AES CBC (256)
* AES CTR (128)
* AES CTR (192)
* AES CTR (256)
* 3DES CBC
* 3DES CTR
* MD5
* MD5 HMAC
* SHA1
* SHA1 HMAC
* SHA256
* SHA256 HMAC
* SHA384
* SHA384 HMAC
* SHA512
* SHA512 HMAC
* AES GCM (128)

Limitations
-----------

* Hardware only supports scenarios where ICV (digest buffer) is placed just
  after the authenticated data. Other placement will result in error.

* Before running crypto test suite it is advised to increase limit of
  opened files:

  .. code-block:: console

     ulimit -n 20000

Installation
------------

MRVL CRYPTO PMD driver compilation is disabled by default due to external dependencies.
Currently there are two driver specific compilation options in
``config/common_base`` available:

- ``CONFIG_RTE_LIBRTE_MRVL_CRYPTO`` (default ``n``)

    Toggle compilation of the librte_pmd_mrvl driver.

- ``CONFIG_RTE_LIBRTE_MRVL_CRYPTO_DEBUG`` (default ``n``)

    Toggle display of debugging messages.

During compilation external MUSDK library, which provides direct access
to Marvell's EIP197 cryptographic engine, is necessary. Library sources are
available `here <https://github.com/MarvellEmbeddedProcessors/musdk-marvell/tree/musdk-armada-17.08>`__.

Alternatively, prebuilt library can be downloaded from
`Marvell Extranet <https://extranet.marvell.com>`_. Once approval has been
granted, library can be found by typing ``musdk`` in the search box.

For MUSDK library build instructions please refer to ``doc/musdk_get_started.txt``
in library sources directory.

MUSDK requires out of tree kernel modules to work. Kernel tree needed to build
them is available
`here <https://github.com/MarvellEmbeddedProcessors/linux-marvell/tree/linux-4.4.52-armada-17.08>`__.

Initialization
--------------

After successfully building MRVL CRYPTO PMD, the following modules need to be
loaded:

.. code-block:: console

   insmod musdk_uio.ko
   insmod mvpp2x_sysfs.ko
   insmod mv_pp_uio.ko
   insmod mv_sam_uio.ko
   insmod crypto_safexcel.ko

- `musdk_uio.ko`, `mv_pp2_uio.ko` and `mv_sam_uio.ko` are distributed together with MUSDK library.
- `crypto_safexcel.ko` is an in-kernel module.
- `mvpp2x_sysfs.ko` can be build from sources available `here <https://github.com/MarvellEmbeddedProcessors/mvpp2x-marvell/tree/mvpp2x-armada-17.08>`__.

The following parameters (all optional) are exported by the driver:

* max_nb_queue_pairs: maximum number of queue pairs in the device (8 by default).
* max_nb_sessions: maximum number of sessions that can be created (2048 by default).
* socket_id: socket on which to allocate the device resources on.

l2fwd-crypto example application can be used to verify MRVL CRYPTO PMD
operation:

.. code-block:: console

   ./l2fwd-crypto --vdev=net_mrvl,iface=eth0 --vdev=crypto_mrvl -- \
     --cipher_op ENCRYPT --cipher_algo aes-cbc \
     --cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f  \
     --auth_op GENERATE --auth_algo sha1-hmac \
     --auth_key 10:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:1e:1f

Example output:

.. code-block:: console

   [...]
   AAD: at [0x7f253ceb80], len=
   P ID 0 configuration ----
   Port mode               : KR
   MAC status              : disabled
   Link status             : link up
   Port speed              : 10G
   Port duplex             : full
   Port: Egress enable tx_port_num=16 qmap=0x1
   PORT: Port0 - link
   P ID 0 configuration ----
   Port mode               : KR
   MAC status              : disabled
   Link status             : link down
   Port speed              : 10G
   Port duplex             : full
   Port: Egress enable tx_port_num=16 qmap=0x1
   Port 0, MAC address: 00:50:43:02:21:20


   Checking link statusdone
   Port 0 Link Up - speed 0 Mbps - full-duplex
   Lcore 0: RX port 0
   Allocated session pool on socket 0
   eip197: 0:0 registers: paddr: 0xf2880000, vaddr: 0x0x7f56a80000
   DMA buffer (131136 bytes) for CDR #0 allocated: paddr = 0xb0585e00, vaddr = 0x7f09384e00
   DMA buffer (131136 bytes) for RDR #0 allocated: paddr = 0xb05a5f00, vaddr = 0x7f093a4f00
   DMA buffers allocated for 2049 operations. Tokens - 256 bytes
   Lcore 0: cryptodev 0
   L2FWD: lcore 1 has nothing to do
   L2FWD: lcore 2 has nothing to do
   L2FWD: lcore 3 has nothing to do
   L2FWD: entering main loop on lcore 0
   L2FWD:  -- lcoreid=0 portid=0
   L2FWD:  -- lcoreid=0 cryptoid=0
   Options:-
   nportmask: ffffffff
   ports per lcore: 1
   refresh period : 10000
   single lcore mode: disabled
   stats_printing: enabled
   sessionless crypto: disabled

   Crypto chain: Input --> Encrypt --> Auth generate --> Output

   ---- Cipher information ---
   Algorithm: aes-cbc
   Cipher key: at [0x7f56db4e80], len=16
   00000000: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | ................
   IV: at [0x7f56db4b80], len=16
   00000000: 20 F0 63 0E 45 EB 2D 84 72 D4 13 6E 36 B5 AF FE |  .c.E.-.r..n6...

   ---- Authentication information ---
   Algorithm: sha1-hmac
   Auth key: at [0x7f56db4d80], len=16
   00000000: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F | ................
   IV: at [0x7f56db4a80], len=0
   AAD: at [0x7f253ceb80], len=
