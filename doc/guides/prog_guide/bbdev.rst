..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation

Wireless Baseband Device Library
================================

The Wireless Baseband library provides a common programming framework that
abstracts HW accelerators based on FPGA and/or Fixed Function Accelerators that
assist with 3GPP Physical Layer processing. Furthermore, it decouples the
application from the compute-intensive wireless functions by abstracting their
optimized libraries to appear as virtual bbdev devices.

The functional scope of the BBDEV library are those functions in relation to
the 3GPP Layer 1 signal processing (channel coding, modulation, ...).

The framework currently only supports Turbo Code FEC function.


Design Principles
-----------------

The Wireless Baseband library follows the same ideology of DPDK's Ethernet
Device and Crypto Device frameworks. Wireless Baseband provides a generic
acceleration abstraction framework which supports both physical (hardware) and
virtual (software) wireless acceleration functions.

Device Management
-----------------

Device Creation
~~~~~~~~~~~~~~~

Physical bbdev devices are discovered during the PCI probe/enumeration of the
EAL function which is executed at DPDK initialization, based on
their PCI device identifier, each unique PCI BDF (bus/bridge, device,
function).

Virtual devices can be created by two mechanisms, either using the EAL command
line options or from within the application using an EAL API directly.

From the command line using the --vdev EAL option

.. code-block:: console

   --vdev 'baseband_turbo_sw,max_nb_queues=8,socket_id=0'

Our using the rte_vdev_init API within the application code.

.. code-block:: c

    rte_vdev_init("baseband_turbo_sw", "max_nb_queues=2,socket_id=0")

All virtual bbdev devices support the following initialization parameters:

- ``max_nb_queues`` - maximum number of queues supported by the device.

- ``socket_id`` - socket on which to allocate the device resources on.


Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each device, whether virtual or physical is uniquely designated by two
identifiers:

- A unique device index used to designate the bbdev device in all functions
  exported by the bbdev API.

- A device name used to designate the bbdev device in console messages, for
  administration or debugging purposes. For ease of use, the port name includes
  the port index.


Device Configuration
~~~~~~~~~~~~~~~~~~~~

From the application point of view, each instance of a bbdev device consists of
one or more queues identified by queue IDs. While different devices may have
different capabilities (e.g. support different operation types), all queues on
a device support identical configuration possibilities. A queue is configured
for only one type of operation and is configured at initialization time.
When an operation is enqueued to a specific queue ID, the result is dequeued
from the same queue ID.

Configuration of a device has two different levels: configuration that applies
to the whole device, and configuration that applies to a single queue.

Device configuration is applied with
``rte_bbdev_setup_queues(dev_id,num_queues,socket_id)``
and queue configuration is applied with
``rte_bbdev_queue_configure(dev_id,queue_id,conf)``. Note that, although all
queues on a device support same capabilities, they can be configured differently
and will then behave differently.
Devices supporting interrupts can enable them by using
``rte_bbdev_intr_enable(dev_id)``.

The configuration of each bbdev device includes the following operations:

- Allocation of resources, including hardware resources if a physical device.
- Resetting the device into a well-known default state.
- Initialization of statistics counters.

The ``rte_bbdev_setup_queues`` API is used to setup queues for a bbdev device.

.. code-block:: c

   int rte_bbdev_setup_queues(uint16_t dev_id, uint16_t num_queues,
            int socket_id);

- ``num_queues`` argument identifies the total number of queues to setup for
  this device.

- ``socket_id`` specifies which socket will be used to allocate the memory.


The ``rte_bbdev_intr_enable`` API is used to enable interrupts for a bbdev
device, if supported by the driver. Should be called before starting the device.

.. code-block:: c

   int rte_bbdev_intr_enable(uint16_t dev_id);


Queues Configuration
~~~~~~~~~~~~~~~~~~~~

Each bbdev devices queue is individually configured through the
``rte_bbdev_queue_configure()`` API.
Each queue resources may be allocated on a specified socket.

.. code-block:: c

    struct rte_bbdev_queue_conf {
        int socket;
        uint32_t queue_size;
        uint8_t priority;
        bool deferred_start;
        enum rte_bbdev_op_type op_type;
    };

Device & Queues Management
~~~~~~~~~~~~~~~~~~~~~~~~~~

After initialization, devices are in a stopped state, so must be started by the
application. If an application is finished using a device it can close the
device. Once closed, it cannot be restarted.

.. code-block:: c

    int rte_bbdev_start(uint16_t dev_id)
    int rte_bbdev_stop(uint16_t dev_id)
    int rte_bbdev_close(uint16_t dev_id)
    int rte_bbdev_queue_start(uint16_t dev_id, uint16_t queue_id)
    int rte_bbdev_queue_stop(uint16_t dev_id, uint16_t queue_id)


By default, all queues are started when the device is started, but they can be
stopped individually.

.. code-block:: c

    int rte_bbdev_queue_start(uint16_t dev_id, uint16_t queue_id)
    int rte_bbdev_queue_stop(uint16_t dev_id, uint16_t queue_id)


Logical Cores, Memory and Queues Relationships
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The bbdev device Library as the Poll Mode Driver library support NUMA for when
a processor's logical cores and interfaces utilize its local memory. Therefore
baseband operations, the mbuf being operated on should be allocated from memory
pools created in the local memory. The buffers should, if possible, remain on
the local processor to obtain the best performance results and buffer
descriptors should be populated with mbufs allocated from a mempool allocated
from local memory.

The run-to-completion model also performs better, especially in the case of
virtual bbdev devices, if the baseband operation and data buffers are in local
memory instead of a remote processor's memory. This is also true for the
pipe-line model provided all logical cores used are located on the same processor.

Multiple logical cores should never share the same queue for enqueuing
operations or dequeuing operations on the same bbdev device since this would
require global locks and hinder performance. It is however possible to use a
different logical core to dequeue an operation on a queue pair from the logical
core which it was enqueued on. This means that a baseband burst enqueue/dequeue
APIs are a logical place to transition from one logical core to another in a
packet processing pipeline.


Device Operation Capabilities
-----------------------------

Capabilities (in terms of operations supported, max number of queues, etc.)
identify what a bbdev is capable of performing that differs from one device to
another. For the full scope of the bbdev capability see the definition of the
structure in the *DPDK API Reference*.

.. code-block:: c

   struct rte_bbdev_op_cap;

A device reports its capabilities when registering itself in the bbdev framework.
With the aid of this capabilities mechanism, an application can query devices to
discover which operations within the 3GPP physical layer they are capable of
performing. Below is an example of the capabilities for a PMD it supports in
relation to Turbo Encoding and Decoding operations.

.. code-block:: c

    static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
        {
            .type = RTE_BBDEV_OP_TURBO_DEC,
            .cap.turbo_dec = {
                .capability_flags =
                    RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE |
                    RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN |
                    RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN |
                    RTE_BBDEV_TURBO_CRC_TYPE_24B |
                    RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP |
                    RTE_BBDEV_TURBO_EARLY_TERMINATION,
                .max_llr_modulus = 16,
                .num_buffers_src = RTE_BBDEV_MAX_CODE_BLOCKS,
                .num_buffers_hard_out =
                        RTE_BBDEV_MAX_CODE_BLOCKS,
                .num_buffers_soft_out = 0,
            }
        },
        {
            .type   = RTE_BBDEV_OP_TURBO_ENC,
            .cap.turbo_enc = {
                .capability_flags =
                        RTE_BBDEV_TURBO_CRC_24B_ATTACH |
                        RTE_BBDEV_TURBO_CRC_24A_ATTACH |
                        RTE_BBDEV_TURBO_RATE_MATCH |
                        RTE_BBDEV_TURBO_RV_INDEX_BYPASS,
                .num_buffers_src = RTE_BBDEV_MAX_CODE_BLOCKS,
                .num_buffers_dst = RTE_BBDEV_MAX_CODE_BLOCKS,
            }
        },
        RTE_BBDEV_END_OF_CAPABILITIES_LIST()
    };

Capabilities Discovery
~~~~~~~~~~~~~~~~~~~~~~

Discovering the features and capabilities of a bbdev device poll mode driver
is achieved through the ``rte_bbdev_info_get()`` function.

.. code-block:: c

   int rte_bbdev_info_get(uint16_t dev_id, struct rte_bbdev_info *dev_info)

This allows the user to query a specific bbdev PMD and get all the device
capabilities. The ``rte_bbdev_info`` structure provides two levels of
information:

- Device relevant information, like: name and related rte_bus.

- Driver specific information, as defined by the ``struct rte_bbdev_driver_info``
  structure, this is where capabilities reside along with other specifics like:
  maximum queue sizes and priority level.

.. code-block:: c

    struct rte_bbdev_info {
        int socket_id;
        const char *dev_name;
        const struct rte_bus *bus;
        uint16_t num_queues;
        bool started;
        struct rte_bbdev_driver_info drv;
    };

Operation Processing
--------------------

Scheduling of baseband operations on DPDK's application data path is
performed using a burst oriented asynchronous API set. A queue on a bbdev
device accepts a burst of baseband operations using enqueue burst API. On physical
bbdev devices the enqueue burst API will place the operations to be processed
on the device's hardware input queue, for virtual devices the processing of the
baseband operations is usually completed during the enqueue call to the bbdev
device. The dequeue burst API will retrieve any processed operations available
from the queue on the bbdev device, from physical devices this is usually
directly from the device's processed queue, and for virtual device's from a
``rte_ring`` where processed operations are place after being processed on the
enqueue call.


Enqueue / Dequeue Burst APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The burst enqueue API uses a bbdev device identifier and a queue
identifier to specify the bbdev device queue to schedule the processing on.
The ``num_ops`` parameter is the number of operations to process which are
supplied in the ``ops`` array of ``rte_bbdev_*_op`` structures.
The enqueue function returns the number of operations it actually enqueued for
processing, a return value equal to ``num_ops`` means that all packets have been
enqueued.

.. code-block:: c

    uint16_t rte_bbdev_enqueue_enc_ops(uint16_t dev_id, uint16_t queue_id,
            struct rte_bbdev_enc_op **ops, uint16_t num_ops)

    uint16_t rte_bbdev_enqueue_dec_ops(uint16_t dev_id, uint16_t queue_id,
            struct rte_bbdev_dec_op **ops, uint16_t num_ops)

The dequeue API uses the same format as the enqueue API of processed but
the ``num_ops`` and ``ops`` parameters are now used to specify the max processed
operations the user wishes to retrieve and the location in which to store them.
The API call returns the actual number of processed operations returned, this
can never be larger than ``num_ops``.

.. code-block:: c

    uint16_t rte_bbdev_dequeue_enc_ops(uint16_t dev_id, uint16_t queue_id,
            struct rte_bbdev_enc_op **ops, uint16_t num_ops)

    uint16_t rte_bbdev_dequeue_dec_ops(uint16_t dev_id, uint16_t queue_id,
            struct rte_bbdev_dec_op **ops, uint16_t num_ops)

Operation Representation
~~~~~~~~~~~~~~~~~~~~~~~~

An encode bbdev operation is represented by ``rte_bbdev_enc_op`` structure,
and by ``rte_bbdev_dec_op`` for decode. These structures act as metadata
containers for all necessary information required for the bbdev operation to be
processed on a particular bbdev device poll mode driver.

.. code-block:: c

    struct rte_bbdev_enc_op {
        int status;
        struct rte_mempool *mempool;
        void *opaque_data;
        struct rte_bbdev_op_turbo_enc turbo_enc;
    };

    struct rte_bbdev_dec_op {
        int status;
        struct rte_mempool *mempool;
        void *opaque_data;
        struct rte_bbdev_op_turbo_dec turbo_dec;
    };

The operation structure by itself defines the operation type. It includes an
operation status, a reference to the operation specific data, which can vary in
size and content depending on the operation being provisioned. It also contains
the source mempool for the operation, if it is allocated from a mempool.

If bbdev operations are allocated from a bbdev operation mempool, see next
section, there is also the ability to allocate private memory with the
operation for applications purposes.

Application software is responsible for specifying all the operation specific
fields in the ``rte_bbdev_*_op`` structure which are then used by the bbdev PMD
to process the requested operation.


Operation Management and Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The bbdev library provides an API set for managing bbdev operations which
utilize the Mempool Library to allocate operation buffers. Therefore, it ensures
that the bbdev operation is interleaved optimally across the channels and
ranks for optimal processing.

.. code-block:: c

    struct rte_mempool *
    rte_bbdev_op_pool_create(const char *name, enum rte_bbdev_op_type type,
            unsigned int num_elements, unsigned int cache_size,
            int socket_id)

``rte_bbdev_*_op_alloc_bulk()`` and ``rte_bbdev_*_op_free_bulk()`` are used to
allocate bbdev operations of a specific type from a given bbdev operation mempool.

.. code-block:: c

    int rte_bbdev_enc_op_alloc_bulk(struct rte_mempool *mempool,
            struct rte_bbdev_enc_op **ops, uint16_t num_ops)

    int rte_bbdev_dec_op_alloc_bulk(struct rte_mempool *mempool,
            struct rte_bbdev_dec_op **ops, uint16_t num_ops)

``rte_bbdev_*_op_free_bulk()`` is called by the application to return an
operation to its allocating pool.

.. code-block:: c

    void rte_bbdev_dec_op_free_bulk(struct rte_bbdev_dec_op **ops,
            unsigned int num_ops)
    void rte_bbdev_enc_op_free_bulk(struct rte_bbdev_enc_op **ops,
            unsigned int num_ops)

BBDEV Inbound/Outbound Memory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The bbdev operation structure contains all the mutable data relating to
performing Turbo coding on a referenced mbuf data buffer. It is used for either
encode or decode operations.

Turbo Encode operation accepts one input and one output.
Turbo Decode operation accepts one input and two outputs, called *hard-decision*
and *soft-decision* outputs. *Soft-decision* output is optional.

It is expected that the application provides input and output mbuf pointers
allocated and ready to use. The baseband framework supports turbo coding on
Code Blocks (CB) and Transport Blocks (TB).

For the output buffer(s), the application is required to provide an allocated
and free mbuf, so that bbdev write back the resulting output.

The support of split "scattered" buffers is a driver-specific feature, so it is
reported individually by the supporting driver as a capability.

Input and output data buffers are identified by ``rte_bbdev_op_data`` structure,
as follows:

.. code-block:: c

    struct rte_bbdev_op_data {
        struct rte_mbuf *data;
        uint32_t offset;
        uint32_t length;
    };


This structure has three elements:

- ``data``: This is the mbuf data structure representing the data for BBDEV
  operation.

  This mbuf pointer can point to one Code Block (CB) data buffer or multiple CBs
  contiguously located next to each other. A Transport Block (TB) represents a
  whole piece of data that is divided into one or more CBs. Maximum number of
  CBs can be contained in one TB is defined by ``RTE_BBDEV_MAX_CODE_BLOCKS``.

  An mbuf data structure cannot represent more than one TB. The smallest piece
  of data that can be contained in one mbuf is one CB.
  An mbuf can include one contiguous CB, subset of contiguous CBs that are
  belonging to one TB, or all contiguous CBs that are belonging to one TB.

  If a BBDEV PMD supports the extended capability "Scatter-Gather", then it is
  capable of collecting (gathering) non-contiguous (scattered) data from
  multiple locations in the memory.
  This capability is reported by the capability flags:

  - ``RTE_BBDEV_TURBO_ENC_SCATTER_GATHER``, and

  - ``RTE_BBDEV_TURBO_DEC_SCATTER_GATHER``.

  Only if a BBDEV PMD supports this feature, chained mbuf data structures are
  accepted. A chained mbuf can represent one non-contiguous CB or multiple
  non-contiguous CBs.
  The first mbuf segment in the given chained mbuf represents the first piece
  of the CB. Offset is only applicable to the first segment. ``length`` is the
  total length of the CB.

  BBDEV driver is responsible for identifying where the split is and enqueue
  the split data to its internal queues.

  If BBDEV PMD does not support this feature, it will assume inbound mbuf data
  contains one segment.

  The output mbuf data though is always one segment, even if the input was a
  chained mbuf.


- ``offset``: This is the starting point of the BBDEV (encode/decode) operation,
  in bytes.

  BBDEV starts to read data past this offset.
  In case of chained mbuf, this offset applies only to the first mbuf segment.


- ``length``: This is the total data length to be processed in one operation,
  in bytes.

  In case the mbuf data is representing one CB, this is the length of the CB
  undergoing the operation.
  If it is for multiple CBs, this is the total length of those CBs undergoing
  the operation.
  If it is for one TB, this is the total length of the TB under operation.
  In case of chained mbuf, this data length includes the lengths of the
  "scattered" data segments undergoing the operation.


BBDEV Turbo Encode Operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    struct rte_bbdev_op_turbo_enc {
        struct rte_bbdev_op_data input;
        struct rte_bbdev_op_data output;

        uint32_t op_flags;
        uint8_t rv_index;
        uint8_t code_block_mode;
        union {
            struct rte_bbdev_op_enc_cb_params cb_params;
            struct rte_bbdev_op_enc_tb_params tb_params;
        };
    };

The Turbo encode structure is composed of the ``input`` and ``output`` mbuf
data pointers. The provided mbuf pointer of ``input`` needs to be big enough to
stretch for extra CRC trailers.

``op_flags`` parameter holds all operation related flags, like whether CRC24A is
included by the application or not.

``code_block_mode`` flag identifies the mode in which bbdev is operating in.

The encode interface works on both the code block (CB) and the transport block
(TB). An operation executes in "CB-mode" when the CB is standalone. While
"TB-mode" executes when an operation performs on one or multiple CBs that
belong to a TB. Therefore, a given data can be standalone CB, full-size TB or
partial TB. Partial TB means that only a subset of CBs belonging to a bigger TB
are being enqueued.

  **NOTE:** It is assumed that all enqueued ops in one ``rte_bbdev_enqueue_enc_ops()``
  call belong to one mode, either CB-mode or TB-mode.

In case that the CB is smaller than Z (6144 bits), then effectively the TB = CB.
CRC24A is appended to the tail of the CB. The application is responsible for
calculating and appending CRC24A before calling BBDEV in case that the
underlying driver does not support CRC24A generation.

In CB-mode, CRC24A/B is an optional operation.
The input ``k`` is the size of the CB (this maps to K as described in 3GPP TS
36.212 section 5.1.2), this size is inclusive of CRC24A/B.
The ``length`` is inclusive of CRC24A/B and equals to ``k`` in this case.

Not all BBDEV PMDs are capable of CRC24A/B calculation. Flags
``RTE_BBDEV_TURBO_CRC_24A_ATTACH`` and ``RTE_BBDEV_TURBO_CRC_24B_ATTACH``
informs the application with relevant capability. These flags can be set in the
``op_flags`` parameter to indicate BBDEV to calculate and append CRC24A to CB
before going forward with Turbo encoding.

Output format of the CB encode will have the encoded CB in ``e`` size output
(this maps to E described in 3GPP TS 36.212 section 5.1.4.1.2). The output mbuf
buffer size needs to be big enough to hold the encoded buffer of size ``e``.

In TB-mode, CRC24A is assumed to be pre-calculated and appended to the inbound
TB mbuf data buffer.
The output mbuf data structure is expected to be allocated by the application
with enough room for the output data.

The difference between the partial and full-size TB is that we need to know the
index of the first CB in this group and the number of CBs contained within.
The first CB index is given by ``r`` but the number of the remaining CBs is
calculated automatically by BBDEV before passing down to the driver.

The number of remaining CBs should not be confused with ``c``. ``c`` is the
total number of CBs that composes the whole TB (this maps to C as
described in 3GPP TS 36.212 section 5.1.2).

The ``length`` is total size of the CBs inclusive of any CRC24A and CRC24B in
case they were appended by the application.

The case when one CB belongs to TB and is being enqueued individually to BBDEV,
this case is considered as a special case of partial TB where its number of CBs
is 1. Therefore, it requires to get processed in TB-mode.

The figure below visualizes the encoding of CBs using BBDEV interface in
TB-mode. CB-mode is a reduced version, where only one CB exists:

.. _figure_turbo_tb_encode:

.. figure:: img/turbo_tb_encode.*

    Turbo encoding of Code Blocks in mbuf structure


BBDEV Turbo Decode Operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    struct rte_bbdev_op_turbo_dec {
        struct rte_bbdev_op_data input;
        struct rte_bbdev_op_data hard_output;
        struct rte_bbdev_op_data soft_output;

        uint32_t op_flags;
        uint8_t rv_index;
        uint8_t iter_min:4;
        uint8_t iter_max:4;
        uint8_t iter_count;
        uint8_t ext_scale;
        uint8_t num_maps;
        uint8_t code_block_mode;
        union {
            struct rte_bbdev_op_dec_cb_params cb_params;
            struct rte_bbdev_op_dec_tb_params tb_params;
        };
    };

The Turbo decode structure is composed of the ``input`` and ``output`` mbuf
data pointers.

``op_flags`` parameter holds all operation related flags, like whether CRC24B is
retained or not.

``code_block_mode`` flag identifies the mode in which bbdev is operating in.

Similarly, the decode interface works on both the code block (CB) and the
transport block (TB). An operation executes in "CB-mode" when the CB is
standalone. While "TB-mode" executes when an operation performs on one or
multiple CBs that belong to a TB. Therefore, a given data can be standalone CB,
full-size TB or partial TB. Partial TB means that only a subset of CBs belonging
to a bigger TB are being enqueued.

  **NOTE:** It is assumed that all enqueued ops in one ``rte_bbdev_enqueue_dec_ops()``
  call belong to one mode, either CB-mode or TB-mode.

The input ``k`` is the size of the decoded CB (this maps to K as described in
3GPP TS 36.212 section 5.1.2), this size is inclusive of CRC24A/B.
The ``length`` is inclusive of CRC24A/B and equals to ``k`` in this case.

The input encoded CB data is the Virtual Circular Buffer data stream, wk, with
the null padding included as described in 3GPP TS 36.212 section 5.1.4.1.2 and
shown in 3GPP TS 36.212 section 5.1.4.1 Figure 5.1.4-1.
The size of the virtual circular buffer is 3*Kpi, where Kpi is the 32 byte
aligned value of K, as specified in 3GPP TS 36.212 section 5.1.4.1.1.

Each byte in the input circular buffer is the LLR value of each bit of the
original CB.

``hard_output`` is a mandatory capability that all BBDEV PMDs support. This is
the decoded CBs of K sizes (CRC24A/B is the last 24-bit in each decoded CB).
Soft output is an optional capability for BBDEV PMDs. Setting flag
``RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP`` in ``op_flags`` directs BBDEV to retain
CRC24B at the end of each CB. This might be useful for the application in debug
mode.
An LLR rate matched output is computed in the ``soft_output`` buffer structure
for the given ``e`` size (this maps to E described in 3GPP TS 36.212 section
5.1.4.1.2). The output mbuf buffer size needs to be big enough to hold the
encoded buffer of size ``e``.

The first CB Virtual Circular Buffer (VCB) index is given by ``r`` but the
number of the remaining CB VCBs is calculated automatically by BBDEV before
passing down to the driver.

The number of remaining CB VCBs should not be confused with ``c``. ``c`` is the
total number of CBs that composes the whole TB (this maps to C as
described in 3GPP TS 36.212 section 5.1.2).

The ``length`` is total size of the CBs inclusive of any CRC24A and CRC24B in
case they were appended by the application.

The case when one CB belongs to TB and is being enqueued individually to BBDEV,
this case is considered as a special case of partial TB where its number of CBs
is 1. Therefore, it requires to get processed in TB-mode.

The output mbuf data structure is expected to be allocated by the application
with enough room for the output data.

The figure below visualizes the decoding of CBs using BBDEV interface in
TB-mode. CB-mode is a reduced version, where only one CB exists:

.. _figure_turbo_tb_decode:

.. figure:: img/turbo_tb_decode.*

    Turbo decoding of Code Blocks in mbuf structure


Sample code
-----------

The baseband device sample application gives an introduction on how to use the
bbdev framework, by giving a sample code performing a loop-back operation with a
baseband processor capable of transceiving data packets.

The following sample C-like pseudo-code shows the basic steps to encode several
buffers using (**sw_turbo**) bbdev PMD.

.. code-block:: c

    /* EAL Init */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

    /* Get number of available bbdev devices */
    nb_bbdevs = rte_bbdev_count();
    if (nb_bbdevs == 0)
        rte_exit(EXIT_FAILURE, "No bbdevs detected!\n");

    /* Create bbdev op pools */
    bbdev_op_pool[RTE_BBDEV_OP_TURBO_ENC] =
            rte_bbdev_op_pool_create("bbdev_op_pool_enc",
            RTE_BBDEV_OP_TURBO_ENC, NB_MBUF, 128, rte_socket_id());

    /* Get information for this device */
    rte_bbdev_info_get(dev_id, &info);

    /* Setup BBDEV device queues */
    ret = rte_bbdev_setup_queues(dev_id, qs_nb, info.socket_id);
    if (ret < 0)
        rte_exit(EXIT_FAILURE,
                "ERROR(%d): BBDEV %u not configured properly\n",
                ret, dev_id);

    /* setup device queues */
    qconf.socket = info.socket_id;
    qconf.queue_size = info.drv.queue_size_lim;
    qconf.op_type = RTE_BBDEV_OP_TURBO_ENC;

    for (q_id = 0; q_id < qs_nb; q_id++) {
        /* Configure all queues belonging to this bbdev device */
        ret = rte_bbdev_queue_configure(dev_id, q_id, &qconf);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                    "ERROR(%d): BBDEV %u queue %u not configured properly\n",
                    ret, dev_id, q_id);
    }

    /* Start bbdev device */
    ret = rte_bbdev_start(dev_id);

    /* Create the mbuf mempool for pkts */
    mbuf_pool = rte_pktmbuf_pool_create("bbdev_mbuf_pool",
            NB_MBUF, MEMPOOL_CACHE_SIZE, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE,
                "Unable to create '%s' pool\n", pool_name);

    while (!global_exit_flag) {

        /* Allocate burst of op structures in preparation for enqueue */
        if (rte_bbdev_enc_op_alloc_bulk(bbdev_op_pool[RTE_BBDEV_OP_TURBO_ENC],
            ops_burst, op_num) != 0)
            continue;

        /* Allocate input mbuf pkts */
        ret = rte_pktmbuf_alloc_bulk(mbuf_pool, input_pkts_burst, MAX_PKT_BURST);
        if (ret < 0)
            continue;

        /* Allocate output mbuf pkts */
        ret = rte_pktmbuf_alloc_bulk(mbuf_pool, output_pkts_burst, MAX_PKT_BURST);
        if (ret < 0)
            continue;

        for (j = 0; j < op_num; j++) {
            /* Append the size of the ethernet header */
            rte_pktmbuf_append(input_pkts_burst[j],
                    sizeof(struct ether_hdr));

            /* set op */

            ops_burst[j]->turbo_enc.input.offset =
                sizeof(struct ether_hdr);

            ops_burst[j]->turbo_enc->input.length =
                rte_pktmbuf_pkt_len(bbdev_pkts[j]);

            ops_burst[j]->turbo_enc->input.data =
                input_pkts_burst[j];

            ops_burst[j]->turbo_enc->output.offset =
                sizeof(struct ether_hdr);

            ops_burst[j]->turbo_enc->output.data =
                    output_pkts_burst[j];
        }

        /* Enqueue packets on BBDEV device */
        op_num = rte_bbdev_enqueue_enc_ops(qconf->bbdev_id,
                qconf->bbdev_qs[q], ops_burst,
                MAX_PKT_BURST);

        /* Dequeue packets from BBDEV device*/
        op_num = rte_bbdev_dequeue_enc_ops(qconf->bbdev_id,
                qconf->bbdev_qs[q], ops_burst,
                MAX_PKT_BURST);
    }


BBDEV Device API
~~~~~~~~~~~~~~~~

The bbdev Library API is described in the *DPDK API Reference* document.
