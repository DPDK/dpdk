..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

IPsec Packet Processing Library
===============================

DPDK provides a library for IPsec data-path processing.
The library utilizes the existing DPDK crypto-dev and
security API to provide the application with a transparent and
high performant IPsec packet processing API.
The library is concentrated on data-path protocols processing
(ESP and AH), IKE protocol(s) implementation is out of scope
for this library.

SA level API
------------

This API operates on the IPsec Security Association (SA) level.
It provides functionality that allows user for given SA to process
inbound and outbound IPsec packets.

To be more specific:

*  for inbound ESP/AH packets perform decryption, authentication, integrity checking, remove ESP/AH related headers
*  for outbound packets perform payload encryption, attach ICV, update/add IP headers, add ESP/AH headers/trailers,
*  setup related mbuf fields (ol_flags, tx_offloads, etc.).
*  initialize/un-initialize given SA based on user provided parameters.

The SA level API is based on top of crypto-dev/security API and relies on
them to perform actual cipher and integrity checking.

Due to the nature of the crypto-dev API (enqueue/dequeue model) the library
introduces an asynchronous API for IPsec packets destined to be processed by
the crypto-device.

The expected API call sequence for data-path processing would be:

.. code-block:: c

    /* enqueue for processing by crypto-device */
    rte_ipsec_pkt_crypto_prepare(...);
    rte_cryptodev_enqueue_burst(...);
    /* dequeue from crypto-device and do final processing (if any) */
    rte_cryptodev_dequeue_burst(...);
    rte_ipsec_pkt_crypto_group(...); /* optional */
    rte_ipsec_pkt_process(...);

For packets destined for inline processing no extra overhead
is required and the synchronous API call: rte_ipsec_pkt_process()
is sufficient for that case.

.. note::

    For more details about the IPsec API, please refer to the *DPDK API Reference*.

The current implementation supports all four currently defined
rte_security types:

RTE_SECURITY_ACTION_TYPE_NONE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In that mode the library functions perform

* for inbound packets:

  - check SQN
  - prepare *rte_crypto_op* structure for each input packet
  - verify that integrity check and decryption performed by crypto device
    completed successfully
  - check padding data
  - remove outer IP header (tunnel mode) / update IP header (transport mode)
  - remove ESP header and trailer, padding, IV and ICV data
  - update SA replay window

* for outbound packets:

  - generate SQN and IV
  - add outer IP header (tunnel mode) / update IP header (transport mode)
  - add ESP header and trailer, padding and IV data
  - prepare *rte_crypto_op* structure for each input packet
  - verify that crypto device operations (encryption, ICV generation)
    were completed successfully

RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In that mode the library functions perform

* for inbound packets:

  - verify that integrity check and decryption performed by *rte_security*
    device completed successfully
  - check SQN
  - check padding data
  - remove outer IP header (tunnel mode) / update IP header (transport mode)
  - remove ESP header and trailer, padding, IV and ICV data
  - update SA replay window

* for outbound packets:

  - generate SQN and IV
  - add outer IP header (tunnel mode) / update IP header (transport mode)
  - add ESP header and trailer, padding and IV data
  - update *ol_flags* inside *struct  rte_mbuf* to indicate that
    inline-crypto processing has to be performed by HW on this packet
  - invoke *rte_security* device specific *set_pkt_metadata()* to associate
    security device specific data with the packet

RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In that mode the library functions perform

* for inbound packets:

  - verify that integrity check and decryption performed by *rte_security*
    device completed successfully

* for outbound packets:

  - update *ol_flags* inside *struct  rte_mbuf* to indicate that
    inline-crypto processing has to be performed by HW on this packet
  - invoke *rte_security* device specific *set_pkt_metadata()* to associate
    security device specific data with the packet

RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In that mode the library functions perform

* for inbound packets:

  - prepare *rte_crypto_op* structure for each input packet
  - verify that integrity check and decryption performed by crypto device
    completed successfully

* for outbound packets:

  - prepare *rte_crypto_op* structure for each input packet
  - verify that crypto device operations (encryption, ICV generation)
    were completed successfully

To accommodate future custom implementations function pointers
model is used for both *crypto_prepare* and *process* implementations.


Supported features
------------------

*  ESP protocol tunnel mode both IPv4/IPv6.

*  ESP protocol transport mode both IPv4/IPv6.

*  ESN and replay window.

*  algorithms: 3DES-CBC, AES-CBC, AES-CTR, AES-GCM, HMAC-SHA1, NULL.


Limitations
-----------

The following features are not properly supported in the current version:

*  ESP transport mode for IPv6 packets with extension headers.
*  Multi-segment packets.
*  Updates of the fields in inner IP header for tunnel mode
   (as described in RFC 4301, section 5.1.2).
*  Hard/soft limit for SA lifetime (time interval/byte count).
