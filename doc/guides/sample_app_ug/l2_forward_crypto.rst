..  BSD LICENSE
    Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

.. _l2_fwd_crypto_app:

L2 Forwarding with Crypto Sample Application
============================================

The L2 Forwarding with Crypto (l2fwd-crypto) sample application is a simple example of packet processing using
the Data Plane Development Kit (DPDK), in conjunction with the Cryptodev library.

Overview
--------

The L2 Forwarding with Crypto sample application performs a crypto operation (cipher/hash)
specified by the user from command line (or using the default values),
with a crypto device capable of doing that operation,
for each packet that is received on a RX_PORT and performs L2 forwarding.
The destination port is the adjacent port from the enabled portmask, that is,
if the first four ports are enabled (portmask 0xf),
ports 0 and 1 forward into each other, and ports 2 and 3 forward into each other.
Also, if MAC addresses updating is enabled, the MAC addresses are affected as follows:

*   The source MAC address is replaced by the TX_PORT MAC address

*   The destination MAC address is replaced by  02:00:00:00:00:TX_PORT_ID

Compiling the Application
-------------------------

#.  Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/l2fwd-crypto

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    *See the DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./build/l2fwd-crypto [EAL options] -- [-p PORTMASK] [-q NQ] [-s] [-T PERIOD] /
    [--cdev_type HW/SW/ANY] [--chain HASH_CIPHER/CIPHER_HASH/CIPHER_ONLY/HASH_ONLY/AEAD] /
    [--cipher_algo ALGO] [--cipher_op ENCRYPT/DECRYPT] [--cipher_key KEY] /
    [--cipher_key_random_size SIZE] [--cipher_iv IV] [--cipher_iv_random_size SIZE] /
    [--auth_algo ALGO] [--auth_op GENERATE/VERIFY] [--auth_key KEY] /
    [--auth_key_random_size SIZE] [--auth_iv IV] [--auth_iv_random_size SIZE] /
    [--aead_algo ALGO] [--aead_op ENCRYPT/DECRYPT] [--aead_key KEY] /
    [--aead_key_random_size SIZE] [--aead_iv] [--aead_iv_random_size SIZE] /
    [--aad AAD] [--aad_random_size SIZE] /
    [--digest size SIZE] [--sessionless] [--cryptodev_mask MASK] /
    [--mac-updating] [--no-mac-updating]

where,

*   p PORTMASK: A hexadecimal bitmask of the ports to configure (default is all the ports)

*   q NQ: A number of queues (=ports) per lcore (default is 1)

*   s: manage all ports from single core

*   T PERIOD: statistics will be refreshed each PERIOD seconds

    (0 to disable, 10 default, 86400 maximum)

*   cdev_type: select preferred crypto device type: HW, SW or anything (ANY)

    (default is ANY)

*   chain: select the operation chaining to perform: Cipher->Hash (CIPHER_HASH),

    Hash->Cipher (HASH_CIPHER), Cipher (CIPHER_ONLY), Hash (HASH_ONLY)

    or AEAD (AEAD)

    (default is Cipher->Hash)

*   cipher_algo: select the ciphering algorithm (default is aes-cbc)

*   cipher_op: select the ciphering operation to perform: ENCRYPT or DECRYPT

    (default is ENCRYPT)

*   cipher_key: set the ciphering key to be used. Bytes has to be separated with ":"

*   cipher_key_random_size: set the size of the ciphering key,

    which will be generated randomly.

    Note that if --cipher_key is used, this will be ignored.

*   cipher_iv: set the cipher IV to be used. Bytes has to be separated with ":"

*   cipher_iv_random_size: set the size of the cipher IV, which will be generated randomly.

    Note that if --cipher_iv is used, this will be ignored.

*   auth_algo: select the authentication algorithm (default is sha1-hmac)

*   auth_op: select the authentication operation to perform: GENERATE or VERIFY

    (default is GENERATE)

*   auth_key: set the authentication key to be used. Bytes has to be separated with ":"

*   auth_key_random_size: set the size of the authentication key,

    which will be generated randomly.

    Note that if --auth_key is used, this will be ignored.

*   auth_iv: set the auth IV to be used. Bytes has to be separated with ":"

*   auth_iv_random_size: set the size of the auth IV, which will be generated randomly.

    Note that if --auth_iv is used, this will be ignored.

*   aead_algo: select the AEAD algorithm (default is aes-gcm)

*   aead_op: select the AEAD operation to perform: ENCRYPT or DECRYPT

    (default is ENCRYPT)

*   aead_key: set the AEAD key to be used. Bytes has to be separated with ":"

*   aead_key_random_size: set the size of the AEAD key,

    which will be generated randomly.

    Note that if --aead_key is used, this will be ignored.

*   aead_iv: set the AEAD IV to be used. Bytes has to be separated with ":"

*   aead_iv_random_size: set the size of the AEAD IV, which will be generated randomly.

    Note that if --aead_iv is used, this will be ignored.

*   aad: set the AAD to be used. Bytes has to be separated with ":"

*   aad_random_size: set the size of the AAD, which will be generated randomly.

    Note that if --aad is used, this will be ignored.

*   digest_size: set the size of the digest to be generated/verified.

*   sessionless: no crypto session will be created.

*   cryptodev_mask: A hexadecimal bitmask of the cryptodevs to be used by the
    application.

    (default is all cryptodevs).

*   [no-]mac-updating: Enable or disable MAC addresses updating (enabled by default).


The application requires that crypto devices capable of performing
the specified crypto operation are available on application initialization.
This means that HW crypto device/s must be bound to a DPDK driver or
a SW crypto device/s (virtual crypto PMD) must be created (using --vdev).

To run the application in linuxapp environment with 2 lcores, 2 ports and 2 crypto devices, issue the command:

.. code-block:: console

    $ ./build/l2fwd-crypto -l 0-1 -n 4 --vdev "crypto_aesni_mb0" \
    --vdev "crypto_aesni_mb1" -- -p 0x3 --chain CIPHER_HASH \
    --cipher_op ENCRYPT --cipher_algo aes-cbc \
    --cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f \
    --auth_op GENERATE --auth_algo aes-xcbc-mac \
    --auth_key 10:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:1e:1f

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The L2 forward with Crypto application demonstrates the performance of a crypto operation
on a packet received on a RX PORT before forwarding it to a TX PORT.

The following figure illustrates a sample flow of a packet in the application,
from reception until transmission.

.. _figure_l2_fwd_encrypt_flow:

.. figure:: img/l2_fwd_encrypt_flow.*

   Encryption flow Through the L2 Forwarding with Crypto Application


The following sections provide some explanation of the application.

Crypto operation specification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All the packets received in all the ports get transformed by the crypto device/s
(ciphering and/or authentication).
The crypto operation to be performed on the packet is parsed from the command line
(go to "Running the Application section for all the options).

If no parameter is passed, the default crypto operation is:

* Encryption with AES-CBC with 128 bit key.

* Authentication with SHA1-HMAC (generation).

* Keys, IV and AAD are generated randomly.

There are two methods to pass keys, IV and ADD from the command line:

* Passing the full key, separated bytes by ":"::

   --cipher_key 00:11:22:33:44

* Passing the size, so key is generated randomly::

   --cipher_key_random_size 16

**Note**:
   If full key is passed (first method) and the size is passed as well (second method),
   the latter will be ignored.

Size of these keys are checked (regardless the method), before starting the app,
to make sure that it is supported by the crypto devices.

Crypto device initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once the encryption operation is defined, crypto devices are initialized.
The crypto devices must be either bound to a DPDK driver (if they are physical devices)
or created using the EAL option --vdev (if they are virtual devices),
when running the application.

The initialize_cryptodevs() function performs the device initialization.
It iterates through the list of the available crypto devices and
check which ones are capable of performing the operation.
Each device has a set of capabilities associated with it,
which are stored in the device info structure, so the function checks if the operation
is within the structure of each device.

The following code checks if the device supports the specified cipher algorithm
(similar for the authentication algorithm):

.. code-block:: c

   /* Check if device supports cipher algo */
   i = 0;
   opt_cipher_algo = options->cipher_xform.cipher.algo;
   cap = &dev_info.capabilities[i];
   while (cap->op != RTE_CRYPTO_OP_TYPE_UNDEFINED) {
           cap_cipher_algo = cap->sym.cipher.algo;
           if (cap->sym.xform_type ==
                           RTE_CRYPTO_SYM_XFORM_CIPHER) {
                   if (cap_cipher_algo == opt_cipher_algo) {
                           if (check_type(options, &dev_info) == 0)
                                   break;
                   }
           }
           cap = &dev_info.capabilities[++i];
   }

If a capable crypto device is found, key sizes are checked to see if they are supported
(cipher key and IV for the ciphering):

.. code-block:: c

   /*
    * Check if length of provided cipher key is supported
    * by the algorithm chosen.
    */
   if (options->ckey_param) {
           if (check_supported_size(
                           options->cipher_xform.cipher.key.length,
                           cap->sym.cipher.key_size.min,
                           cap->sym.cipher.key_size.max,
                           cap->sym.cipher.key_size.increment)
                                   != 0) {
                   printf("Unsupported cipher key length\n");
                   return -1;
           }
   /*
    * Check if length of the cipher key to be randomly generated
    * is supported by the algorithm chosen.
    */
   } else if (options->ckey_random_size != -1) {
           if (check_supported_size(options->ckey_random_size,
                           cap->sym.cipher.key_size.min,
                           cap->sym.cipher.key_size.max,
                           cap->sym.cipher.key_size.increment)
                                   != 0) {
                   printf("Unsupported cipher key length\n");
                   return -1;
           }
           options->cipher_xform.cipher.key.length =
                                   options->ckey_random_size;
   /* No size provided, use minimum size. */
   } else
           options->cipher_xform.cipher.key.length =
                           cap->sym.cipher.key_size.min;

After all the checks, the device is configured and it is added to the
crypto device list.

**Note**:
   The number of crypto devices that supports the specified crypto operation
   must be at least the number of ports to be used.

Session creation
~~~~~~~~~~~~~~~~

The crypto operation has a crypto session associated to it, which contains
information such as the transform chain to perform (e.g. ciphering then hashing),
pointers to the keys, lengths... etc.

This session is created and is later attached to the crypto operation:

.. code-block:: c

   static struct rte_cryptodev_sym_session *
   initialize_crypto_session(struct l2fwd_crypto_options *options,
                   uint8_t cdev_id)
   {
           struct rte_crypto_sym_xform *first_xform;
           struct rte_cryptodev_sym_session *session;
           uint8_t socket_id = rte_cryptodev_socket_id(cdev_id);
           struct rte_mempool *sess_mp = session_pool_socket[socket_id];


           if (options->xform_chain == L2FWD_CRYPTO_AEAD) {
                   first_xform = &options->aead_xform;
           } else if (options->xform_chain == L2FWD_CRYPTO_CIPHER_HASH) {
                   first_xform = &options->cipher_xform;
                   first_xform->next = &options->auth_xform;
           } else if (options->xform_chain == L2FWD_CRYPTO_HASH_CIPHER) {
                   first_xform = &options->auth_xform;
                   first_xform->next = &options->cipher_xform;
           } else if (options->xform_chain == L2FWD_CRYPTO_CIPHER_ONLY) {
                   first_xform = &options->cipher_xform;
           } else {
                   first_xform = &options->auth_xform;
           }

           session = rte_cryptodev_sym_session_create(sess_mp);

           if (session == NULL)
                   return NULL;

          if (rte_cryptodev_sym_session_init(cdev_id, session,
                                first_xform, sess_mp) < 0)
                   return NULL;

          return session;
   }

   ...

   port_cparams[i].session = initialize_crypto_session(options,
                                port_cparams[i].dev_id);

Crypto operation creation
~~~~~~~~~~~~~~~~~~~~~~~~~

Given N packets received from a RX PORT, N crypto operations are allocated
and filled:

.. code-block:: c

   if (nb_rx) {
   /*
    * If we can't allocate a crypto_ops, then drop
    * the rest of the burst and dequeue and
    * process the packets to free offload structs
    */
   if (rte_crypto_op_bulk_alloc(
                   l2fwd_crypto_op_pool,
                   RTE_CRYPTO_OP_TYPE_SYMMETRIC,
                   ops_burst, nb_rx) !=
                                   nb_rx) {
           for (j = 0; j < nb_rx; j++)
                   rte_pktmbuf_free(pkts_burst[i]);

           nb_rx = 0;
   }

After filling the crypto operation (including session attachment),
the mbuf which will be transformed is attached to it::

   op->sym->m_src = m;

Since no destination mbuf is set, the source mbuf will be overwritten
after the operation is done (in-place).

Crypto operation enqueuing/dequeuing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once the operation has been created, it has to be enqueued in one of the crypto devices.
Before doing so, for performance reasons, the operation stays in a buffer.
When the buffer has enough operations (MAX_PKT_BURST), they are enqueued in the device,
which will perform the operation at that moment:

.. code-block:: c

   static int
   l2fwd_crypto_enqueue(struct rte_crypto_op *op,
                   struct l2fwd_crypto_params *cparams)
   {
           unsigned lcore_id, len;
           struct lcore_queue_conf *qconf;

           lcore_id = rte_lcore_id();

           qconf = &lcore_queue_conf[lcore_id];
           len = qconf->op_buf[cparams->dev_id].len;
           qconf->op_buf[cparams->dev_id].buffer[len] = op;
           len++;

           /* enough ops to be sent */
           if (len == MAX_PKT_BURST) {
                   l2fwd_crypto_send_burst(qconf, MAX_PKT_BURST, cparams);
                   len = 0;
           }

           qconf->op_buf[cparams->dev_id].len = len;
           return 0;
   }

   ...

   static int
   l2fwd_crypto_send_burst(struct lcore_queue_conf *qconf, unsigned n,
                   struct l2fwd_crypto_params *cparams)
   {
           struct rte_crypto_op **op_buffer;
           unsigned ret;

           op_buffer = (struct rte_crypto_op **)
                           qconf->op_buf[cparams->dev_id].buffer;

           ret = rte_cryptodev_enqueue_burst(cparams->dev_id,
                           cparams->qp_id, op_buffer, (uint16_t) n);

           crypto_statistics[cparams->dev_id].enqueued += ret;
           if (unlikely(ret < n)) {
                   crypto_statistics[cparams->dev_id].errors += (n - ret);
                   do {
                           rte_pktmbuf_free(op_buffer[ret]->sym->m_src);
                           rte_crypto_op_free(op_buffer[ret]);
                   } while (++ret < n);
           }

           return 0;
   }

After this, the operations are dequeued from the device, and the transformed mbuf
is extracted from the operation. Then, the operation is freed and the mbuf is
forwarded as it is done in the L2 forwarding application.

.. code-block:: c

   /* Dequeue packets from Crypto device */
   do {
           nb_rx = rte_cryptodev_dequeue_burst(
                           cparams->dev_id, cparams->qp_id,
                           ops_burst, MAX_PKT_BURST);

           crypto_statistics[cparams->dev_id].dequeued +=
                           nb_rx;

           /* Forward crypto'd packets */
           for (j = 0; j < nb_rx; j++) {
                   m = ops_burst[j]->sym->m_src;

                   rte_crypto_op_free(ops_burst[j]);
                   l2fwd_simple_forward(m, portid);
           }
   } while (nb_rx == MAX_PKT_BURST);
