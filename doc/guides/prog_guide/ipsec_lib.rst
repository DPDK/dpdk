..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2020 Intel Corporation.

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

RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In that mode the library functions perform same operations as in
``RTE_SECURITY_ACTION_TYPE_NONE``. The only difference is that crypto operations
are performed with CPU crypto synchronous API.


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

SA database API
----------------

SA database(SAD) is a table with <key, value> pairs.

Value is an opaque user provided pointer to the user defined SA data structure.

According to RFC4301 each SA can be uniquely identified by a key
which is either:

  - security parameter index(SPI)
  - or SPI and destination IP(DIP)
  - or SPI, DIP and source IP(SIP)

In case of multiple matches, longest matching key will be returned.

Create/destroy
~~~~~~~~~~~~~~

librte_ipsec SAD implementation provides ability to create/destroy SAD tables.

To create SAD table user has to specify how many entries of each key type is
required and IP protocol type (IPv4/IPv6).
As an example:


.. code-block:: c

    struct rte_ipsec_sad *sad;
    struct rte_ipsec_sad_conf conf;

    conf.socket_id = -1;
    conf.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = some_nb_rules_spi_only;
    conf.max_sa[RTE_IPSEC_SAD_SPI_DIP] = some_nb_rules_spi_dip;
    conf.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = some_nb_rules_spi_dip_sip;
    conf.flags = RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY;

    sad = rte_ipsec_sad_create("test", &conf);

.. note::

    for more information please refer to ipsec library API reference

Add/delete rules
~~~~~~~~~~~~~~~~

Library also provides methods to add or delete key/value pairs from the SAD.
To add user has to specify key, key type and a value which is an opaque pointer to SA.
The key type reflects a set of tuple fields that will be used for lookup of the SA.
As mentioned above there are 3 types of a key and the representation of a key type is:

.. code-block:: c

        RTE_IPSEC_SAD_SPI_ONLY,
        RTE_IPSEC_SAD_SPI_DIP,
        RTE_IPSEC_SAD_SPI_DIP_SIP,

As an example, to add new entry into the SAD for IPv4 addresses:

.. code-block:: c

    struct rte_ipsec_sa *sa;
    union rte_ipsec_sad_key key;

    key.v4.spi = rte_cpu_to_be_32(spi_val);
    if (key_type >= RTE_IPSEC_SAD_SPI_DIP) /* DIP is optional*/
        key.v4.dip = rte_cpu_to_be_32(dip_val);
    if (key_type == RTE_IPSEC_SAD_SPI_DIP_SIP) /* SIP is optional*/
        key.v4.sip = rte_cpu_to_be_32(sip_val);

    rte_ipsec_sad_add(sad, &key, key_type, sa);

.. note::

    By performance reason it is better to keep spi/dip/sip in net byte order
    to eliminate byteswap on lookup

To delete user has to specify key and key type.

Delete code would look like:

.. code-block:: c

    union rte_ipsec_sad_key key;

    key.v4.spi = rte_cpu_to_be_32(necessary_spi);
    if (key_type >= RTE_IPSEC_SAD_SPI_DIP) /* DIP is optional*/
        key.v4.dip = rte_cpu_to_be_32(necessary_dip);
    if (key_type == RTE_IPSEC_SAD_SPI_DIP_SIP) /* SIP is optional*/
        key.v4.sip = rte_cpu_to_be_32(necessary_sip);

    rte_ipsec_sad_del(sad, &key, key_type);


Lookup
~~~~~~
Library provides lookup by the given {SPI,DIP,SIP} tuple of
inbound ipsec packet as a key.

The search key is represented by:

.. code-block:: c

    union rte_ipsec_sad_key {
        struct rte_ipsec_sadv4_key  v4;
        struct rte_ipsec_sadv6_key  v6;
    };

where v4 is a tuple for IPv4:

.. code-block:: c

    struct rte_ipsec_sadv4_key {
        uint32_t spi;
        uint32_t dip;
        uint32_t sip;
    };

and v6 is a tuple for IPv6:

.. code-block:: c

    struct rte_ipsec_sadv6_key {
        uint32_t spi;
        uint8_t dip[16];
        uint8_t sip[16];
    };

As an example, lookup related code could look like that:

.. code-block:: c

    int i;
    union rte_ipsec_sad_key keys[BURST_SZ];
    const union rte_ipsec_sad_key *keys_p[BURST_SZ];
    void *vals[BURST_SZ];

    for (i = 0; i < BURST_SZ_MAX; i++) {
        keys[i].v4.spi = esp_hdr[i]->spi;
        keys[i].v4.dip = ipv4_hdr[i]->dst_addr;
        keys[i].v4.sip = ipv4_hdr[i]->src_addr;
        keys_p[i] = &keys[i];
    }
    rte_ipsec_sad_lookup(sad, keys_p, vals, BURST_SZ);

    for (i = 0; i < BURST_SZ_MAX; i++) {
        if (vals[i] == NULL)
            printf("SA not found for key index %d\n", i);
        else
            printf("SA pointer is %p\n", vals[i]);
    }


Supported features
------------------

*  ESP protocol tunnel mode both IPv4/IPv6.

*  ESP protocol transport mode both IPv4/IPv6.

*  ESN and replay window.

*  NAT-T / UDP encapsulated ESP.

*  TSO (only for inline crypto mode)

*  algorithms: 3DES-CBC, AES-CBC, AES-CTR, AES-GCM, AES_CCM, CHACHA20_POLY1305,
   AES_GMAC, HMAC-SHA1, NULL.


Telemetry support
------------------
Telemetry support implements SA details and IPsec packet add data counters
statistics. Per SA telemetry statistics can be enabled using
``rte_ipsec_telemetry_sa_add`` and disabled using
``rte_ipsec_telemetry_sa_del``. Note that these calls are not thread safe.


Limitations
-----------

The following features are not properly supported in the current version:

*  Hard/soft limit for SA lifetime (time interval/byte count).
