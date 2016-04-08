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


Cryptography Device Library
===========================

The cryptodev library provides a Crypto device framework for management and
provisioning of hardware and software Crypto poll mode drivers, defining generic
APIs which support a number of different Crypto operations. The framework
currently only supports cipher, authentication, chained cipher/authentication
and AEAD symmetric Crypto operations.


Design Principles
-----------------

The cryptodev library follows the same basic principles as those used in DPDKs
Ethernet Device framework. The Crypto framework provides a generic Crypto device
framework which supports both physical (hardware) and virtual (software) Crypto
devices as well as a generic Crypto API which allows Crypto devices to be
managed and configured and supports Crypto operations to be provisioned on
Crypto poll mode driver.


Device Management
-----------------

Device Creation
~~~~~~~~~~~~~~~

Physical Crypto devices are discovered during the PCI probe/enumeration of the
EAL function which is executed at DPDK initialization, based on
their PCI device identifier, each unique PCI BDF (bus/bridge, device,
function). Specific physical Crypto devices, like other physical devices in DPDK
can be white-listed or black-listed using the EAL command line options.

Virtual devices can be created by two mechanisms, either using the EAL command
line options or from within the application using an EAL API directly.

From the command line using the --vdev EAL option

.. code-block:: console

   --vdev  'cryptodev_aesni_mb_pmd0,max_nb_queue_pairs=2,max_nb_sessions=1024,socket_id=0'

Our using the rte_eal_vdev_init API within the application code.

.. code-block:: c

   rte_eal_vdev_init("cryptodev_aesni_mb_pmd",
                     "max_nb_queue_pairs=2,max_nb_sessions=1024,socket_id=0")

All virtual Crypto devices support the following initialization parameters:

* ``max_nb_queue_pairs`` - maximum number of queue pairs supported by the device.
* ``max_nb_sessions`` - maximum number of sessions supported by the device
* ``socket_id`` - socket on which to allocate the device resources on.


Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each device, whether virtual or physical is uniquely designated by two
identifiers:

- A unique device index used to designate the Crypto device in all functions
  exported by the cryptodev API.

- A device name used to designate the Crypto device in console messages, for
  administration or debugging purposes. For ease of use, the port name includes
  the port index.


Device Configuration
~~~~~~~~~~~~~~~~~~~~

The configuration of each Crypto device includes the following operations:

- Allocation of resources, including hardware resources if a physical device.
- Resetting the device into a well-known default state.
- Initialization of statistics counters.

The rte_cryptodev_configure API is used to configure a Crypto device.

.. code-block:: c

   int rte_cryptodev_configure(uint8_t dev_id,
                               struct rte_cryptodev_config *config)

The ``rte_cryptodev_config`` structure is used to pass the configuration parameters.
In contains parameter for socket selection, number of queue pairs and the
session mempool configuration.

.. code-block:: c

    struct rte_cryptodev_config {
        int socket_id;
        /**< Socket to allocate resources on */
        uint16_t nb_queue_pairs;
        /**< Number of queue pairs to configure on device */

        struct {
            uint32_t nb_objs;
            uint32_t cache_size;
        } session_mp;
        /**< Session mempool configuration */
    };


Configuration of Queue Pairs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each Crypto devices queue pair is individually configured through the
``rte_cryptodev_queue_pair_setup`` API.
Each queue pairs resources may be allocated on a specified socket.

.. code-block:: c

    int rte_cryptodev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
                const struct rte_cryptodev_qp_conf *qp_conf,
                int socket_id)

    struct rte_cryptodev_qp_conf {
        uint32_t nb_descriptors; /**< Number of descriptors per queue pair */
    };


Logical Cores, Memory and Queues Pair Relationships
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Crypto device Library as the Poll Mode Driver library support NUMA for when
a processor’s logical cores and interfaces utilize its local memory. Therefore
Crypto operations, and in the case of symmetric Crypto operations, the session
and the mbuf being operated on, should be allocated from memory pools created
in the local memory. The buffers should, if possible, remain on the local
processor to obtain the best performance results and buffer descriptors should
be populated with mbufs allocated from a mempool allocated from local memory.

The run-to-completion model also performs better, especially in the case of
virtual Crypto devices, if the Crypto operation and session and data buffer is
in local memory instead of a remote processor's memory. This is also true for
the pipe-line model provided all logical cores used are located on the same
processor.

Multiple logical cores should never share the same queue pair for enqueuing
operations or dequeuing operations on the same Crypto device since this would
require global locks and hinder performance. It is however possible to use a
different logical core to dequeue an operation on a queue pair from the logical
core which it was enqueued on. This means that a crypto burst enqueue/dequeue
APIs are a logical place to transition from one logical core to another in a
packet processing pipeline.


Device Features and Capabilities
---------------------------------

Crypto devices define their functionality through two mechanisms, global device
features and algorithm capabilities. Global devices features identify device
wide level features which are applicable to the whole device such as
the device having hardware acceleration or supporting symmetric Crypto
operations,

The capabilities mechanism defines the individual algorithms/functions which
the device supports, such as a specific symmetric Crypto cipher or
authentication operation.


Device Features
~~~~~~~~~~~~~~~

Currently the following Crypto device features are defined:

* Symmetric Crypto operations
* Asymmetric Crypto operations
* Chaining of symmetric Crypto operations
* SSE accelerated SIMD vector operations
* AVX accelerated SIMD vector operations
* AVX2 accelerated SIMD vector operations
* AESNI accelerated instructions
* Hardware off-load processing


Device Operation Capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Crypto capabilities which identify particular algorithm which the Crypto PMD
supports are  defined by the operation type, the operation transform, the
transform identifier and then the particulars of the transform. For the full
scope of the Crypto capability see the definition of the structure in the
*DPDK API Reference*.

.. code-block:: c

   struct rte_cryptodev_capabilities;

Each Crypto poll mode driver defines its own private array of capabilities
for the operations it supports. Below is an example of the capabilities for a
PMD which supports the authentication algorithm SHA1_HMAC and the cipher
algorithm AES_CBC.

.. code-block:: c

    static const struct rte_cryptodev_capabilities pmd_capabilities[] = {
        {    /* SHA1 HMAC */
            .op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
            .sym = {
                .xform_type = RTE_CRYPTO_SYM_XFORM_AUTH,
                .auth = {
                    .algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
                    .block_size = 64,
                    .key_size = {
                        .min = 64,
                        .max = 64,
                        .increment = 0
                    },
                    .digest_size = {
                        .min = 12,
                        .max = 12,
                        .increment = 0
                    },
                    .aad_size = { 0 }
                }
            }
        },
        {    /* AES CBC */
            .op = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
            .sym = {
                .xform_type = RTE_CRYPTO_SYM_XFORM_CIPHER,
                .cipher = {
                    .algo = RTE_CRYPTO_CIPHER_AES_CBC,
                    .block_size = 16,
                    .key_size = {
                        .min = 16,
                        .max = 32,
                        .increment = 8
                    },
                    .iv_size = {
                        .min = 16,
                        .max = 16,
                        .increment = 0
                    }
                }
            }
        }
    }


Capabilities Discovery
~~~~~~~~~~~~~~~~~~~~~~

Discovering the features and capabilities of a Crypto device poll mode driver
is achieved through the ``rte_cryptodev_info_get`` function.

.. code-block:: c

   void rte_cryptodev_info_get(uint8_t dev_id,
                               struct rte_cryptodev_info *dev_info);

This allows the user to query a specific Crypto PMD and get all the device
features and capabilities. The ``rte_cryptodev_info`` structure contains all the
relevant information for the device.

.. code-block:: c

    struct rte_cryptodev_info {
        const char *driver_name;
        enum rte_cryptodev_type dev_type;
        struct rte_pci_device *pci_dev;

        uint64_t feature_flags;

        const struct rte_cryptodev_capabilities *capabilities;

        unsigned max_nb_queue_pairs;

        struct {
            unsigned max_nb_sessions;
        } sym;
    };


Operation Processing
--------------------

Scheduling of Crypto operations on DPDK's application data path is
performed using a burst oriented asynchronous API set. A queue pair on a Crypto
device accepts a burst of Crypto operations using enqueue burst API. On physical
Crypto devices the enqueue burst API will place the operations to be processed
on the devices hardware input queue, for virtual devices the processing of the
Crypto operations is usually completed during the enqueue call to the Crypto
device. The dequeue burst API will retrieve any processed operations available
from the queue pair on the Crypto device, from physical devices this is usually
directly from the devices processed queue, and for virtual device's from a
``rte_ring`` where processed operations are place after being processed on the
enqueue call.


Enqueue / Dequeue Burst APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The burst enqueue API uses a Crypto device identifier and a queue pair
identifier to specify the Crypto device queue pair to schedule the processing on.
The ``nb_ops`` parameter is the number of operations to process which are
supplied in the ``ops`` array of ``rte_crypto_op`` structures.
The enqueue function returns the number of operations it actually enqueued for
processing, a return value equal to ``nb_ops`` means that all packets have been
enqueued.

.. code-block:: c

   uint16_t rte_cryptodev_enqueue_burst(uint8_t dev_id, uint16_t qp_id,
                                        struct rte_crypto_op **ops, uint16_t nb_ops)

The dequeue API uses the same format as the enqueue API of processed but
the ``nb_ops`` and ``ops`` parameters are now used to specify the max processed
operations the user wishes to retrieve and the location in which to store them.
The API call returns the actual number of processed operations returned, this
can never be larger than ``nb_ops``.

.. code-block:: c

   uint16_t rte_cryptodev_dequeue_burst(uint8_t dev_id, uint16_t qp_id,
                                        struct rte_crypto_op **ops, uint16_t nb_ops)


Operation Representation
~~~~~~~~~~~~~~~~~~~~~~~~

An Crypto operation is represented by an rte_crypto_op structure, which is a
generic metadata container for all necessary information required for the
Crypto operation to be processed on a particular Crypto device poll mode driver.

.. figure:: img/crypto_op.*

The operation structure includes the operation type and the operation status,
a reference to the operation specific data, which can vary in size and content
depending on the operation being provisioned. It also contains the source
mempool for the operation, if it allocate from a mempool. Finally an
opaque pointer for user specific data is provided.

If Crypto operations are allocated from a Crypto operation mempool, see next
section, there is also the ability to allocate private memory with the
operation for applications purposes.

Application software is responsible for specifying all the operation specific
fields in the ``rte_crypto_op`` structure which are then used by the Crypto PMD
to process the requested operation.


Operation Management and Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The cryptodev library provides an API set for managing Crypto operations which
utilize the Mempool Library to allocate operation buffers. Therefore, it ensures
that the crytpo operation is interleaved optimally across the channels and
ranks for optimal processing.
A ``rte_crypto_op`` contains a field indicating the pool that it originated from.
When calling ``rte_crypto_op_free(op)``, the operation returns to its original pool.

.. code-block:: c

   extern struct rte_mempool *
   rte_crypto_op_pool_create(const char *name, enum rte_crypto_op_type type,
                             unsigned nb_elts, unsigned cache_size, uint16_t priv_size,
                             int socket_id);

During pool creation ``rte_crypto_op_init()`` is called as a constructor to
initialize each Crypto operation which subsequently calls
``__rte_crypto_op_reset()`` to configure any operation type specific fields based
on the type parameter.


``rte_crypto_op_alloc()`` and ``rte_crypto_op_bulk_alloc()`` are used to allocate
Crypto operations of a specific type from a given Crypto operation mempool.
``__rte_crypto_op_reset()`` is called on each operation before being returned to
allocate to a user so the operation is always in a good known state before use
by the application.

.. code-block:: c

   struct rte_crypto_op *rte_crypto_op_alloc(struct rte_mempool *mempool,
                                             enum rte_crypto_op_type type)

   unsigned rte_crypto_op_bulk_alloc(struct rte_mempool *mempool,
                                     enum rte_crypto_op_type type,
                                     struct rte_crypto_op **ops, uint16_t nb_ops)

``rte_crypto_op_free()`` is called by the application to return an operation to
its allocating pool.

.. code-block:: c

   void rte_crypto_op_free(struct rte_crypto_op *op)


Symmetric Cryptography Support
------------------------------

The cryptodev library currently provides support for the following symmetric
Crypto operations; cipher, authentication, including chaining of these
operations, as well as also supporting AEAD operations.


Session and Session Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Session are used in symmetric cryptographic processing to store the immutable
data defined in a cryptographic transform which is used in the operation
processing of a packet flow. Sessions are used to manage information such as
expand cipher keys and HMAC IPADs and OPADs, which need to be calculated for a
particular Crypto operation, but are immutable on a packet to packet basis for
a flow. Crypto sessions cache this immutable data in a optimal way for the
underlying PMD and this allows further acceleration of the offload of
Crypto workloads.

.. figure:: img/cryptodev_sym_sess.*

The Crypto device framework provides a set of session pool management APIs for
the creation and freeing of the sessions, utilizing the Mempool Library.

The framework also provides hooks so the PMDs can pass the amount of memory
required for that PMDs private session parameters, as well as initialization
functions for the configuration of the session parameters and freeing function
so the PMD can managed the memory on destruction of a session.

**Note**: Sessions created on a particular device can only be used on Crypto
devices of the same type, and if you try to use a session on a device different
to that on which it was created then the Crypto operation will fail.

``rte_cryptodev_sym_session_create()`` is used to create a symmetric session on
Crypto device. A symmetric transform chain is used to specify the particular
operation and its parameters. See the section below for details on transforms.

.. code-block:: c

   struct rte_cryptodev_sym_session * rte_cryptodev_sym_session_create(
          uint8_t dev_id, struct rte_crypto_sym_xform *xform);

**Note**: For AEAD operations the algorithm selected for authentication and
ciphering must aligned, eg AES_GCM.


Transforms and Transform Chaining
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Symmetric Crypto transforms (``rte_crypto_sym_xform``) are the mechanism used
to specify the details of the Crypto operation. For chaining of symmetric
operations such as cipher encrypt and authentication generate, the next pointer
allows transform to be chained together. Crypto devices which support chaining
must publish the chaining of symmetric Crypto operations feature flag.

Currently there are two transforms types cipher and authentication, to specify
an AEAD operation it is required to chain a cipher and an authentication
transform together. Also it is important to note that the order in which the
transforms are passed indicates the order of the chaining.

.. code-block:: c

    struct rte_crypto_sym_xform {
        struct rte_crypto_sym_xform *next;
        /**< next xform in chain */
        enum rte_crypto_sym_xform_type type;
        /**< xform type */
        union {
            struct rte_crypto_auth_xform auth;
            /**< Authentication / hash xform */
            struct rte_crypto_cipher_xform cipher;
            /**< Cipher xform */
        };
    };

The API does not place a limit on the number of transforms that can be chained
together but this will be limited by the underlying Crypto device poll mode
driver which is processing the operation.

.. figure:: img/crypto_xform_chain.*


Symmetric Operations
~~~~~~~~~~~~~~~~~~~~

The symmetric Crypto operation structure contains all the mutable data relating
to performing symmetric cryptographic processing on a referenced mbuf data
buffer. It is used for either cipher, authentication, AEAD and chained
operations.

As a minimum the symmetric operation must have a source data buffer (``m_src``),
the session type (session-based/less), a valid session (or transform chain if in
session-less mode) and the minimum authentication/ cipher parameters required
depending on the type of operation specified in the session or the transform
chain.

.. code-block:: c

    struct rte_crypto_sym_op {
        struct rte_mbuf *m_src;
        struct rte_mbuf *m_dst;

        enum rte_crypto_sym_op_sess_type type;

        union {
            struct rte_cryptodev_sym_session *session;
            /**< Handle for the initialised session context */
            struct rte_crypto_sym_xform *xform;
            /**< Session-less API Crypto operation parameters */
        };

        struct {
            struct {
                uint32_t offset;
                uint32_t length;
            } data;   /**< Data offsets and length for ciphering */

            struct {
                uint8_t *data;
                phys_addr_t phys_addr;
                uint16_t length;
            } iv;     /**< Initialisation vector parameters */
        } cipher;

        struct {
            struct {
                uint32_t offset;
                uint32_t length;
            } data;   /**< Data offsets and length for authentication */

            struct {
                uint8_t *data;
                phys_addr_t phys_addr;
                uint16_t length;
            } digest; /**< Digest parameters */

            struct {
                uint8_t *data;
                phys_addr_t phys_addr;
                uint16_t length;
            } aad;    /**< Additional authentication parameters */
        } auth;
    }


Asymmetric Cryptography
-----------------------

Asymmetric functionality is currently not supported by the cryptodev API.


Crypto Device API
~~~~~~~~~~~~~~~~~

The cryptodev Library API is described in the *DPDK API Reference* document.
