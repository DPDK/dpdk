..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

Toeplitz Hash Library
=====================

DPDK provides a Toeplitz Hash Library
to calculate the Toeplitz hash function and to use its properties.
The Toeplitz hash function is commonly used in a wide range of NICs
to calculate the RSS hash sum to spread the traffic among the queues.

.. _figure_rss_queue_assign:

.. figure:: img/rss_queue_assign.*

   RSS queue assignment example


Toeplitz hash function API
--------------------------

There are four functions that provide calculation of the Toeplitz hash sum:

* ``rte_softrss()``
* ``rte_softrss_be()``
* ``rte_thash_gfni()``
* ``rte_thash_gfni_bulk()``

First two functions are scalar implementation and take the parameters:

* A pointer to the tuple, containing fields extracted from the packet.
* A length of this tuple counted in double words.
* A pointer to the RSS hash key corresponding to the one installed on the NIC.

Both of above mentioned _softrss_ functions expect the tuple to be in
"host" byte order and a multiple of 4 bytes in length.
The ``rte_softrss()`` function expects the ``rss_key``
to be exactly the same as the one installed on the NIC.
The ``rte_softrss_be`` function is a faster implementation,
but it expects ``rss_key`` to be converted to the host byte order.

The last two functions are vectorized implementations using
Galois Fields New Instructions. Could be used if ``rte_thash_gfni_supported`` is true.
They expect the tuple to be in network byte order.

``rte_thash_gfni()`` calculates the hash value for a single tuple, and
``rte_thash_gfni_bulk()`` bulk implementation of the rte_thash_gfni().

``rte_thash_gfni()`` takes the parameters:

* A pointer to the matrices derived from the RSS hash key using ``rte_thash_complete_matrix()``.
* A pointer to the tuple.
* A length of the tuple in bytes.

``rte_thash_gfni_bulk()`` takes the parameters:

* A pointer to the matrices derived from the RSS hash key using ``rte_thash_complete_matrix()``.
* A length of the longest tuple in bytes.
* Array of the pointers on data to be hashed.
* Array of ``uint32_t`` where to put calculated Toeplitz hash values
* Number of tuples in a bulk.

``rte_thash_complete_matrix()`` is a function that calculates matrices required by
GFNI implementations from the RSS hash key. It takes the parameters:

* A pointer to the memory where the matrices will be written.
* A pointer to the RSS hash key.
* Length of the RSS hash key in bytes.


Predictable RSS
---------------

In some use cases it is useful to have a way to find partial collisions of the
Toeplitz hash function. In figure :numref:`figure_rss_queue_assign` only a few
of the least significant bits (LSB) of the hash value are used to indicate an
entry in the RSS Redirection Table (ReTa) and thus the index of the queue. So,
in this case it would be useful to find another tuple whose hash has the same
LSB's as the hash from the original tuple.

For example:

- In the case of SNAT (Source Network Address Translation) it is possible to
  find a special source port number on translation so that the hash of
  returning packets, of the given connection, will have desired LSB's.
- In the case of MPLS (Multiprotocol Label Switching), if the MPLS tag is used
  in the hash calculation, the Label Switching router can allocate a special
  MPLS tag to bind an LSP (Label Switching Path) to a given queue. This method
  can be used with the allocation of IPSec SPI, VXLan VNI, etc., to bind the
  tunnel to the desired queue.
- In the case of a TCP stack, a special source port could be chosen for
  outgoing connections, such that the response packets will be assigned to the
  desired queue.

This functionality is provided by the API shown below.
The API consists of 3 parts:

* Create the thash context.

* Create the thash helper, associated with a context.

* Use the helper run time to calculate the adjustable bits of the tuple to
  ensure a collision.


Thash context
~~~~~~~~~~~~~

The function ``rte_thash_init_ctx()`` initializes the context struct
associated with a particular NIC or a set of NICs. It expects:

* The log2 value of the size of the RSS redirection table for the
  corresponding NIC. It reflects the number of least significant bits of the
  hash value to produce a collision for.

* A predefined RSS hash key. This is optional, if ``NULL`` then a random key
  will be initialized.

* The length of the RSS hash key. This value is usually hardware/driver
  specific and can be found in the NIC datasheet.

* Optional flags, as shown below.

Supported flags:

* ``RTE_THASH_IGNORE_PERIOD_OVERFLOW`` - By default, and for security reasons,
  the library prohibits generating a repeatable sequence in the hash key. This
  flag disables such checking. The flag is mainly used for testing in the lab
  to generate an RSS hash key with a uniform hash distribution, if the input
  traffic also has a uniform distribution.

* ``RTE_THASH_MINIMAL_SEQ`` - By default, the library generates a special bit
  sequence in the hash key for all the bits of the subtuple. However, the
  collision generation task requires only the ``log2(RETA_SZ)`` bits in the
  subtuple. This flag forces the minimum bit sequence in the hash key to be
  generated for the required ``log2(RETA_SZ)`` least significant bits of the
  subtuple. The flag can be used in the case of a relatively large number of
  helpers that may overlap with their corresponding bit sequences of RSS hash
  keys.


Thash helper
~~~~~~~~~~~~

The function ``rte_thash_add_helper()`` initializes the helper struct
associated with a given context and a part of a target tuple of interest which
could be altered to produce a hash collision. On success it writes a specially
calculated bit sequence into the RSS hash key which is stored in the context
and calculates a table with values to be XORed with a subtuple.

It expects:

* A pointer to the Thash context to be associated with.

* A length of the subtuple to be modified. The length is counted in bits.

* An offset of the subtuple to be modified from the beginning of the tuple. It
  is also counted in bits.

.. note::

   Adding a helper changes the key stored in the corresponding context. So the
   updated RSS hash key must be uploaded into the NIC after creating all the
   required helpers.


Calculation of the complementary bits to adjust the subtuple
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``rte_thash_get_complement()`` function returns a special bit sequence
with length ``N = log2(rss_reta_sz)`` (for the ``rss_reta_sz`` provided at
context initialization) to be xored with N least significant bits of the
subtuple.

It expects:

* A corresponding helper created for a given subtuple of the tuple.

* A hash value of the tuple we want to alter.

* The desired LSB's of the hash value the user expects to have.

After the returned bit sequence has been XORed with the subtuple, the resulted
LSB's of the new hash value, calculated from the altered tuple, will be the
same as in ``desired_hash``.


Adjust tuple API
~~~~~~~~~~~~~~~~~

The ``rte_thash_get_complement()`` function is a user-friendly wrapper around
a number of other functions. It alters a passed tuple to meet the above
mentioned requirements around the desired hash LSB's.

It expects:

* A Thash context and helper.

* A pointer to the tuple to be changed.

* The length of the tuple.

* A callback function and its userdata to check the tuple after it has been
  changed.

* The number of attempts to change the tuple. Basically, it makes sense if
  there is a callback and a limit on the number of attempts to change the
  tuple, if the callback function returns an error.


Use case example
----------------

There could be a number of different use cases, such as NAT, TCP stack, MPLS
tag allocation, etc. In the following we will consider a SNAT application.

Packets of a single bidirectional flow belonging to different directions can
end up being assigned to different queues and thus processed by different
lcores, as shown in :numref:`figure_predictable_snat_1`:

.. _figure_predictable_snat_1:

.. figure:: img/predictable_snat_1.*

   Bidirectional flow packets distribution in general

That leads to a situation where the same packet flow can be shared between two
cores. Such a situation is not ideal from a performance perspective and
requires extra synchronization efforts that might lead to various performance
penalties, for example:

* The connections table is global so locking/RCU on the flow insertion/removal
  is required.

* Connection metadata must be protected to avoid race conditions.

* More cache pressure if a single connection metadata is kept in different
  L1/L2 caches of a different CPU core.

* Cache pressure/less cache locality on packet handover to the different cores.

We can avoid all these penalties if it can be guaranteed that packets
belonging to one bidirectional flow will be assigned to the same queue, as
shown in :numref:`figure_predictable_snat_2`:

.. _figure_predictable_snat_2:

.. figure:: img/predictable_snat_2.*

   Bidirectional flow packets distribution with predictable RSS


To achieve this in a SNAT scenario it is possible to choose a source port not
randomly, but using the predictable RSS library to produce a partial hash
collision. This is shown in the code below.

.. code-block:: c

   int key_len = 40; /* The default Niantic RSS key length. */

   /** The default Niantic RSS reta size = 2^7 entries, LSBs of hash value are
    *  used as an indexes in RSS ReTa. */
   int reta_sz = 7;
   int ret;
   struct rte_thash_ctx *ctx;

   uint8_t initial_key[key_len] = {0}; /* Default empty key. */

   /* Create and initialize a new thash context. */
   ctx = rte_thash_init_ctx("SNAT", key_len, reta_sz, initial_key, 0);

   /** Add a helper and specify the variable tuple part and its length. In the
    *  SNAT case we want to choose a new source port on SNAT translation in a
    *  way that the reverse tuple will have the same LSBs as the original
    *  direction tuple so that the selected source port will be the
    *  destination port on reply.
    */
   ret = rte_thash_add_helper(ctx, "snat", sizeof(uint16_t) * 8,
                              offsetof(union rte_thash_tuple, v4.dport) * 8);

   if (ret != 0)
       return ret;

   /* Get handler of the required helper. */
   struct rte_thash_subtuple_helper *h = rte_thash_get_helper(ctx, "snat");

   /** After calling rte_thash_add_helper() the initial_key passed on ctx
    *  creation has been changed so we get the new one.
    */
   uint8_t *new_key = rte_thash_get_key(ctx);

   union rte_thash_tuple tuple, rev_tuple;

   /* A complete tuple from the packet. */
   complete_tuple(mbuf, &tuple);

   /* Calculate the RSS hash or get it from mbuf->hash.rss. */
   uint32_t orig_hash = rte_softrss((uint32_t *)&tuple, RTE_THASH_V4_L4_LEN, new_key);

   /** Complete the reverse tuple by translating the SRC address and swapping
    *  src and dst addresses and ports.
    */
   get_rev_tuple(&rev_tuple, &tuple, new_ip);

   /* Calculate the expected rss hash for the reverse tuple. */
   uint32_t rev_hash = rte_softrss((uint32_t *)&rev_tuple, RTE_THASH_V4_L4_LEN, new_key);

   /* Get the adjustment bits for the src port to get a new port. */
   uint32_t adj = rte_thash_get_compliment(h, rev_hash, orig_hash);

   /* Adjust the source port bits. */
   uint16_t new_sport = tuple.v4.sport ^ adj;

   /* Make an actual packet translation. */
   do_snat(mbuf, new_ip, new_sport);
