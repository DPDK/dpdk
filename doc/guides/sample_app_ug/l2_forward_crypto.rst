..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2017 Intel Corporation.

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

To compile the sample application see :doc:`compiling`.

The application is located in the ``l2fwd-crypt`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd-crypto [EAL options] -- [-p PORTMASK] [-q NQ] [-s] [-T PERIOD] /
    [--cdev_type HW/SW/ANY] [--chain HASH_CIPHER/CIPHER_HASH/CIPHER_ONLY/HASH_ONLY/AEAD] /
    [--cipher_algo ALGO] [--cipher_op ENCRYPT/DECRYPT] [--cipher_dataunit_len SIZE] /
    [--cipher_key KEY] [--cipher_key_random_size SIZE] [--cipher_iv IV] /
    [--cipher_iv_random_size SIZE] /
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

*   cipher_dataunit_len: set the length of the cipher data-unit.

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

To run the application in linux environment with 2 lcores, 2 ports and 2 crypto devices, issue the command:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-l2fwd-crypto -l 0-1 -n 4 --vdev "crypto_aesni_mb0" \
    --vdev "crypto_aesni_mb1" -- -p 0x3 --chain CIPHER_HASH \
    --cipher_op ENCRYPT --cipher_algo aes-cbc \
    --cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f \
    --auth_op GENERATE --auth_algo aes-xcbc-mac \
    --auth_key 10:11:12:13:14:15:16:17:18:19:1a:1b:1c:1d:1e:1f

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

.. Note::

    * The ``l2fwd-crypto`` sample application requires IPv4 packets for crypto operation.

    * If multiple Ethernet ports is passed, then equal number of crypto devices are to be passed.

    * All crypto devices shall use the same session.

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
(go to "Running the Application" section for all the options).

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

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Check if device supports cipher algo. 8<
    :end-before: >8 End of check if device supports cipher algo.
    :dedent: 2

If a capable crypto device is found, key sizes are checked to see if they are supported
(cipher key and IV for the ciphering):

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Check if capable cipher is supported. 8<
    :end-before: >8 End of checking if cipher is supported.
    :dedent: 2

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

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Session is created and is later attached to the crypto operation. 8<
    :end-before: >8 End of creation of session.

Crypto operation creation
~~~~~~~~~~~~~~~~~~~~~~~~~

Given N packets received from a RX PORT, N crypto operations are allocated
and filled:

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Allocate and fillcrypto operations. 8<
    :end-before: >8 End of crypto operation allocated and filled.
    :dedent: 3

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

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Crypto enqueue. 8<
    :end-before: >8 End of crypto enqueue.

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: l2fwd_crypto_send_burst 8<
    :end-before: >8 End of l2fwd_crypto_send_burst.

After this, the operations are dequeued from the device, and the transformed mbuf
is extracted from the operation. Then, the operation is freed and the mbuf is
forwarded as it is done in the L2 forwarding application.

.. literalinclude:: ../../../examples/l2fwd-crypto/main.c
    :language: c
    :start-after: Dequeue packets from Crypto device. 8<
    :end-before: >8 End of dequeue packets from crypto device.
    :dedent: 3
