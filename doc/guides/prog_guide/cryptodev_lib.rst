..  BSD LICENSE
    Copyright(c) 2016-2017 Intel Corporation. All rights reserved.

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

   --vdev  'crypto_aesni_mb0,max_nb_queue_pairs=2,max_nb_sessions=1024,socket_id=0'

Our using the rte_vdev_init API within the application code.

.. code-block:: c

   rte_vdev_init("crypto_aesni_mb",
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

The ``rte_cryptodev_config`` structure is used to pass the configuration
parameters for socket selection and number of queue pairs.

.. code-block:: c

    struct rte_cryptodev_config {
        int socket_id;
        /**< Socket to allocate resources on */
        uint16_t nb_queue_pairs;
        /**< Number of queue pairs to configure on device */
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
the device supports, such as a specific symmetric Crypto cipher,
authentication operation or Authenticated Encryption with Associated Data
(AEAD) operation.


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
                    .aad_size = { 0 },
                    .iv_size = { 0 }
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
        uint8_t driver_id;
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

The operation structure includes the operation type, the operation status
and the session type (session-based/less), a reference to the operation
specific data, which can vary in size and content depending on the operation
being provisioned. It also contains the source mempool for the operation,
if it allocated from a mempool.

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

Sessions are used in symmetric cryptographic processing to store the immutable
data defined in a cryptographic transform which is used in the operation
processing of a packet flow. Sessions are used to manage information such as
expand cipher keys and HMAC IPADs and OPADs, which need to be calculated for a
particular Crypto operation, but are immutable on a packet to packet basis for
a flow. Crypto sessions cache this immutable data in a optimal way for the
underlying PMD and this allows further acceleration of the offload of
Crypto workloads.

.. figure:: img/cryptodev_sym_sess.*

The Crypto device framework provides APIs to allocate and initizalize sessions
for crypto devices, where sessions are mempool objects.
It is the application's responsibility to create and manage the session mempools.
This approach allows for different scenarios such as having a single session
mempool for all crypto devices (where the mempool object size is big
enough to hold the private session of any crypto device), as well as having
multiple session mempools of different sizes for better memory usage.

An application can use ``rte_cryptodev_get_private_session_size()`` to
get the private session size of given crypto device. This function would allow
an application to calculate the max device session size of all crypto devices
to create a single session mempool.
If instead an application creates multiple session mempools, the Crypto device
framework also provides ``rte_cryptodev_get_header_session_size`` to get
the size of an uninitialized session.

Once the session mempools have been created, ``rte_cryptodev_sym_session_create()``
is used to allocate an uninitialized session from the given mempool.
The session then must be initialized using ``rte_cryptodev_sym_session_init()``
for each of the required crypto devices. A symmetric transform chain
is used to specify the operation and its parameters. See the section below for
details on transforms.

When a session is no longer used, user must call ``rte_cryptodev_sym_session_clear()``
for each of the crypto devices that are using the session, to free all driver
private session data. Once this is done, session should be freed using
``rte_cryptodev_sym_session_free`` which returns them to their mempool.


Transforms and Transform Chaining
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Symmetric Crypto transforms (``rte_crypto_sym_xform``) are the mechanism used
to specify the details of the Crypto operation. For chaining of symmetric
operations such as cipher encrypt and authentication generate, the next pointer
allows transform to be chained together. Crypto devices which support chaining
must publish the chaining of symmetric Crypto operations feature flag.

Currently there are three transforms types cipher, authentication and AEAD.
Also it is important to note that the order in which the
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
            struct rte_crypto_aead_xform aead;
            /**< AEAD xform */
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
a valid session (or transform chain if in session-less mode) and the minimum
authentication/ cipher/ AEAD parameters required depending on the type of operation
specified in the session or the transform
chain.

.. code-block:: c

    struct rte_crypto_sym_op {
        struct rte_mbuf *m_src;
        struct rte_mbuf *m_dst;

        union {
            struct rte_cryptodev_sym_session *session;
            /**< Handle for the initialised session context */
            struct rte_crypto_sym_xform *xform;
            /**< Session-less API Crypto operation parameters */
        };

        union {
            struct {
                struct {
                    uint32_t offset;
                    uint32_t length;
                } data; /**< Data offsets and length for AEAD */

                struct {
                    uint8_t *data;
                    phys_addr_t phys_addr;
                } digest; /**< Digest parameters */

                struct {
                    uint8_t *data;
                    phys_addr_t phys_addr;
                } aad;
                /**< Additional authentication parameters */
            } aead;

            struct {
                struct {
                    struct {
                        uint32_t offset;
                        uint32_t length;
                    } data; /**< Data offsets and length for ciphering */
                } cipher;

                struct {
                    struct {
                        uint32_t offset;
                        uint32_t length;
                    } data;
                    /**< Data offsets and length for authentication */

                    struct {
                        uint8_t *data;
                        phys_addr_t phys_addr;
                    } digest; /**< Digest parameters */
                } auth;
            };
        };
    };

Sample code
-----------

There are various sample applications that show how to use the cryptodev library,
such as the L2fwd with Crypto sample application (L2fwd-crypto) and
the IPSec Security Gateway application (ipsec-secgw).

While these applications demonstrate how an application can be created to perform
generic crypto operation, the required complexity hides the basic steps of
how to use the cryptodev APIs.

The following sample code shows the basic steps to encrypt several buffers
with AES-CBC (although performing other crypto operations is similar),
using one of the crypto PMDs available in DPDK.

.. code-block:: c

    /*
     * Simple example to encrypt several buffers with AES-CBC using
     * the Cryptodev APIs.
     */

    #define MAX_SESSIONS         1024
    #define NUM_MBUFS            1024
    #define POOL_CACHE_SIZE      128
    #define BURST_SIZE           32
    #define BUFFER_SIZE          1024
    #define AES_CBC_IV_LENGTH    16
    #define AES_CBC_KEY_LENGTH   16
    #define IV_OFFSET            (sizeof(struct rte_crypto_op) + \
                                 sizeof(struct rte_crypto_sym_op))

    struct rte_mempool *mbuf_pool, *crypto_op_pool, *session_pool;
    unsigned int session_size;
    int ret;

    /* Initialize EAL. */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

    uint8_t socket_id = rte_socket_id();

    /* Create the mbuf pool. */
    mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool",
                                    NUM_MBUFS,
                                    POOL_CACHE_SIZE,
                                    0,
                                    RTE_MBUF_DEFAULT_BUF_SIZE,
                                    socket_id);
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /*
     * The IV is always placed after the crypto operation,
     * so some private data is required to be reserved.
     */
    unsigned int crypto_op_private_data = AES_CBC_IV_LENGTH;

    /* Create crypto operation pool. */
    crypto_op_pool = rte_crypto_op_pool_create("crypto_op_pool",
                                            RTE_CRYPTO_OP_TYPE_SYMMETRIC,
                                            NUM_MBUFS,
                                            POOL_CACHE_SIZE,
                                            crypto_op_private_data,
                                            socket_id);
    if (crypto_op_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create crypto op pool\n");

    /* Create the virtual crypto device. */
    char args[128];
    const char *crypto_name = "crypto_aesni_mb0";
    snprintf(args, sizeof(args), "socket_id=%d", socket_id);
    ret = rte_vdev_init(crypto_name, args);
    if (ret != 0)
        rte_exit(EXIT_FAILURE, "Cannot create virtual device");

    uint8_t cdev_id = rte_cryptodev_get_dev_id(crypto_name);

    /* Get private session data size. */
    session_size = rte_cryptodev_get_private_session_size(cdev_id);

    /*
     * Create session mempool, with two objects per session,
     * one for the session header and another one for the
     * private session data for the crypto device.
     */
    session_pool = rte_mempool_create("session_pool",
                                    MAX_SESSIONS * 2,
                                    session_size,
                                    POOL_CACHE_SIZE,
                                    0, NULL, NULL, NULL,
                                    NULL, socket_id,
                                    0);

    /* Configure the crypto device. */
    struct rte_cryptodev_config conf = {
        .nb_queue_pairs = 1,
        .socket_id = socket_id
    };
    struct rte_cryptodev_qp_conf qp_conf = {
        .nb_descriptors = 2048
    };

    if (rte_cryptodev_configure(cdev_id, &conf) < 0)
        rte_exit(EXIT_FAILURE, "Failed to configure cryptodev %u", cdev_id);

    if (rte_cryptodev_queue_pair_setup(cdev_id, 0, &qp_conf,
                            socket_id, session_pool) < 0)
        rte_exit(EXIT_FAILURE, "Failed to setup queue pair\n");

    if (rte_cryptodev_start(cdev_id) < 0)
        rte_exit(EXIT_FAILURE, "Failed to start device\n");

    /* Create the crypto transform. */
    uint8_t cipher_key[16] = {0};
    struct rte_crypto_sym_xform cipher_xform = {
        .next = NULL,
        .type = RTE_CRYPTO_SYM_XFORM_CIPHER,
        .cipher = {
            .op = RTE_CRYPTO_CIPHER_OP_ENCRYPT,
            .algo = RTE_CRYPTO_CIPHER_AES_CBC,
            .key = {
                .data = cipher_key,
                .length = AES_CBC_KEY_LENGTH
            },
            .iv = {
                .offset = IV_OFFSET,
                .length = AES_CBC_IV_LENGTH
            }
        }
    };

    /* Create crypto session and initialize it for the crypto device. */
    struct rte_cryptodev_sym_session *session;
    session = rte_cryptodev_sym_session_create(session_pool);
    if (session == NULL)
        rte_exit(EXIT_FAILURE, "Session could not be created\n");

    if (rte_cryptodev_sym_session_init(cdev_id, session,
                    &cipher_xform, session_pool) < 0)
        rte_exit(EXIT_FAILURE, "Session could not be initialized "
                    "for the crypto device\n");

    /* Get a burst of crypto operations. */
    struct rte_crypto_op *crypto_ops[BURST_SIZE];
    if (rte_crypto_op_bulk_alloc(crypto_op_pool,
                            RTE_CRYPTO_OP_TYPE_SYMMETRIC,
                            crypto_ops, BURST_SIZE) == 0)
        rte_exit(EXIT_FAILURE, "Not enough crypto operations available\n");

    /* Get a burst of mbufs. */
    struct rte_mbuf *mbufs[BURST_SIZE];
    if (rte_pktmbuf_alloc_bulk(mbuf_pool, mbufs, BURST_SIZE) < 0)
        rte_exit(EXIT_FAILURE, "Not enough mbufs available");

    /* Initialize the mbufs and append them to the crypto operations. */
    unsigned int i;
    for (i = 0; i < BURST_SIZE; i++) {
        if (rte_pktmbuf_append(mbufs[i], BUFFER_SIZE) == NULL)
            rte_exit(EXIT_FAILURE, "Not enough room in the mbuf\n");
        crypto_ops[i]->sym->m_src = mbufs[i];
    }

    /* Set up the crypto operations. */
    for (i = 0; i < BURST_SIZE; i++) {
        struct rte_crypto_op *op = crypto_ops[i];
        /* Modify bytes of the IV at the end of the crypto operation */
        uint8_t *iv_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
                                                IV_OFFSET);

        generate_random_bytes(iv_ptr, AES_CBC_IV_LENGTH);

        op->sym->cipher.data.offset = 0;
        op->sym->cipher.data.length = BUFFER_SIZE;

        /* Attach the crypto session to the operation */
        rte_crypto_op_attach_sym_session(op, session);
    }

    /* Enqueue the crypto operations in the crypto device. */
    uint16_t num_enqueued_ops = rte_cryptodev_enqueue_burst(cdev_id, 0,
                                            crypto_ops, BURST_SIZE);

    /*
     * Dequeue the crypto operations until all the operations
     * are proccessed in the crypto device.
     */
    uint16_t num_dequeued_ops, total_num_dequeued_ops = 0;
    do {
        struct rte_crypto_op *dequeued_ops[BURST_SIZE];
        num_dequeued_ops = rte_cryptodev_dequeue_burst(cdev_id, 0,
                                        dequeued_ops, BURST_SIZE);
        total_num_dequeued_ops += num_dequeued_ops;

        /* Check if operation was processed successfully */
        for (i = 0; i < num_dequeued_ops; i++) {
            if (dequeued_ops[i]->status != RTE_CRYPTO_OP_STATUS_SUCCESS)
                rte_exit(EXIT_FAILURE,
                        "Some operations were not processed correctly");
        }

        rte_mempool_put_bulk(crypto_op_pool, (void **)dequeued_ops,
                                            num_dequeued_ops);
    } while (total_num_dequeued_ops < num_enqueued_ops);


Asymmetric Cryptography
-----------------------

Asymmetric functionality is currently not supported by the cryptodev API.


Crypto Device API
~~~~~~~~~~~~~~~~~

The cryptodev Library API is described in the *DPDK API Reference* document.
