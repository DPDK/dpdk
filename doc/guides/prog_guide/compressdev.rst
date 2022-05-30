..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Cavium Networks.

Compression Device Library
==========================

The compression framework provides a generic set of APIs to perform compression services
as well as to query and configure compression devices both physical(hardware) and virtual(software)
to perform those services. The framework currently only supports lossless compression schemes:
Deflate and LZS.

Device Management
-----------------

Device Creation
~~~~~~~~~~~~~~~

Physical compression devices are discovered during the bus probe of the EAL function
which is executed at DPDK initialization, based on their unique device identifier.
For e.g. PCI devices can be identified using PCI BDF (bus/bridge, device, function).
Specific physical compression devices, like other physical devices in DPDK can be
listed using the EAL command line options.

Virtual devices can be created by two mechanisms, either using the EAL command
line options or from within the application using an EAL API directly.

From the command line using the --vdev EAL option

.. code-block:: console

   --vdev  '<PMD name>,socket_id=0'

.. Note::

   * If a DPDK application requires multiple software compression PMD devices then the
     required number of ``--vdev`` args with appropriate libraries are to be added.

   * An application with multiple compression device instances exposed by the same PMD must
     specify a unique name for each device.

   Example: ``--vdev  'pmd0' --vdev  'pmd1'``

Or, by using the rte_vdev_init API within the application code.

.. code-block:: c

   rte_vdev_init("<pmd_name>","socket_id=0")

All virtual compression devices support the following initialization parameters:

* ``socket_id`` - socket on which to allocate the device resources on.

Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each device, whether virtual or physical, is uniquely designated by two
identifiers:

- A unique device index used to designate the compression device in all functions
  exported by the compressdev API.

- A device name used to designate the compression device in console messages, for
  administration or debugging purposes.

Device Configuration
~~~~~~~~~~~~~~~~~~~~

The configuration of each compression device includes the following operations:

- Allocation of resources, including hardware resources if a physical device.
- Resetting the device into a well-known default state.
- Initialization of statistics counters.

The ``rte_compressdev_configure`` API is used to configure a compression device.

The ``rte_compressdev_config`` structure is used to pass the configuration
parameters.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

Configuration of Queue Pairs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each compression device queue pair is individually configured through the
``rte_compressdev_queue_pair_setup`` API.

The ``max_inflight_ops`` is used to pass maximum number of
``rte_comp_op`` that could be present in a queue at a time.
The PMD can then allocate resources accordingly on a specified socket.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

Logical Cores, Memory and Queue Pair Relationships
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Compressdev library supports NUMA similarly as described in Cryptodev library section.

A queue pair cannot be shared, and should be exclusively used by a single processing
context for enqueuing operations or dequeuing operations on the same compression device,
since sharing would require global locks and hinder performance. It is however possible
to use a different logical core to dequeue an operation on a queue pair from the logical
core on which it was enqueued. This means that for a compression burst, enqueue/dequeue
APIs are a logical place to transition from one logical core to another in a
data processing pipeline.

Device Features and Capabilities
--------------------------------

Compression devices define their functionality through two mechanisms, global device
features and algorithm features. Global device features identify device
wide level features which are applicable to the whole device, such as supported hardware
acceleration and CPU features. List of compression device features can be seen in the
RTE_COMPDEV_FF_XXX macros.

The algorithm features are features which the device supports per-algorithm,
such as a stateful compression/decompression, checksums operation etc.
The list of algorithm features can be seen in the RTE_COMP_FF_XXX macros.

Capabilities
~~~~~~~~~~~~
Each PMD has a list of capabilities, including algorithms listed in
the enum ``rte_comp_algorithm``, its associated feature flag, and
sliding window range in log base 2 value. The sliding window range
defines the minimum and maximum size of a lookup window that an algorithm uses
to find duplicates.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

Each Compression poll mode driver defines its array of capabilities
for each algorithm it supports. See the PMD implementation for capability
initialization.

Capabilities Discovery
~~~~~~~~~~~~~~~~~~~~~~

PMD capability and features are discovered via the ``rte_compressdev_info_get`` function.

The ``rte_compressdev_info`` structure contains all the relevant information for the device.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

Compression Operation
---------------------

DPDK compression supports two types of compression methodologies:

- Stateless - data associated with a compression operation is compressed without any reference
  to another compression operation.

- Stateful - data in each compression operation is compressed with reference to previous compression
  operations in the same data stream i.e. history of data is maintained between the operations.

For more explanation, please refer to the RFC https://www.ietf.org/rfc/rfc1951.txt

Operation Representation
~~~~~~~~~~~~~~~~~~~~~~~~

A compression operation is described via ``struct rte_comp_op``, which contains both input and
output data. The operation structure includes the operation type (stateless or stateful),
the operation status, the priv_xform/stream handle, source, destination and checksum buffer
pointers. It also contains the source mempool from which the operation is allocated.
The PMD updates the consumed field with the amount of data read from the source buffer,
and the produced field with the amount of data written into the destination buffer,
along with status of operation.
See the section :ref:`compressdev_prod_cons_op_status`: for more details.

The compression operations mempool also has the ability to allocate private memory with the
operation for the application's use. The application software is responsible for specifying
all the operation specific fields in the ``rte_comp_op`` structure, which are then used
by the compression PMD to process the requested operation.


Operation Management and Allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The compressdev library provides an API set for managing compression operations which
utilize the Mempool Library to allocate operation buffers. Therefore, it ensures
that the compression operation is interleaved optimally across the channels and
ranks for optimal processing.

A ``rte_comp_op`` contains a field indicating the pool it originated from.

``rte_comp_op_alloc()`` and ``rte_comp_op_bulk_alloc()`` are used to allocate
compression operations from a given compression operation mempool.
The operation gets reset before being returned to a user so that the operation
is always in a good known state before use by the application.

``rte_comp_op_free()`` is called by the application to return an operation to
its allocating pool.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

Passing source data as mbuf-chain
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If input data is scattered across several different buffers, then
the application can either parse through all such buffers and make one
mbuf-chain and enqueue it for processing or, alternatively, it can
make multiple sequential enqueue_burst() calls for each of them,
processing them statefully. See :ref:`compressdev_stateful_op`:
for stateful processing of ops.

Operation Status
~~~~~~~~~~~~~~~~
Each operation carries status information updated by the PMD after it is processed.
The following are currently supported:

- RTE_COMP_OP_STATUS_SUCCESS,
    Operation is successfully completed

- RTE_COMP_OP_STATUS_NOT_PROCESSED,
    Operation has not yet been processed by the device

- RTE_COMP_OP_STATUS_INVALID_ARGS,
    Operation failed due to invalid arguments in request

- RTE_COMP_OP_STATUS_ERROR,
    Operation failed because of internal error

- RTE_COMP_OP_STATUS_INVALID_STATE,
    Operation is invoked in invalid state

- RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED,
    Output buffer ran out of space during processing. Error case,
    PMD cannot continue from here.

- RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE,
    Output buffer ran out of space before operation completed, but this
    is not an error case. Output data up to op.produced can be used and
    the next op in the stream should continue on from op.consumed+1.

Operation status after enqueue / dequeue
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Some of the above values may arise in the op after an
``rte_compressdev_enqueue_burst()``. If the number of ops enqueued < the number of ops requested
then the app should check the op.status of nb_enqd+1.
If the status is RTE_COMP_OP_STATUS_NOT_PROCESSED, it likely indicates a full-queue case for a
hardware device, and a retry after dequeuing some ops is likely to be successful.
If the op holds any other status, e.g. RTE_COMP_OP_STATUS_INVALID_ARGS, a retry with
the same op is unlikely to be successful.


.. _compressdev_prod_cons_op_status:

Produced, Consumed And Operation Status
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- If the status is RTE_COMP_OP_STATUS_SUCCESS,
    consumed = amount of data read from input buffer, and
    produced = amount of data written in destination buffer
- If status is RTE_COMP_OP_STATUS_ERROR,
    consumed = produced = undefined
- If status is RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED,
    consumed = 0 and
    produced = usually 0, but in decompression cases a PMD may return > 0
    i.e. amount of data successfully produced until out of space condition
    hit. Application can consume output data in this case, if required.
- If status is RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE,
    consumed = amount of data read, and
    produced = amount of data successfully produced until
    out of space condition hit. The PMD has ability to recover
    from here, so an application can submit the next op from
    consumed+1, and a destination buffer with available space.

Transforms
----------

Compression transforms (``rte_comp_xform``) are the mechanism
to specify the details of the compression operation such as algorithm,
window size, and checksum.

Compression API Hash support
----------------------------

The compression API allows an application to enable digest calculation
alongside compression and decompression of data. A PMD reflects its
support for hash algorithms via capability algo feature flags.
If supported, the PMD always calculates the digest on plaintext i.e.
before compression and after decompression.

Currently supported list of hash algos are SHA-1 and SHA2 family
SHA256.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

If required, the application should set the valid hash algo in compress
or decompress xforms during ``rte_compressdev_stream_create()``
or ``rte_compressdev_private_xform_create()``, and pass a valid
output buffer in ``rte_comp_op`` hash field struct to store the
resulting digest. The buffer passed should be contiguous and large
enough to store digest, which is 20 bytes for SHA-1 and
32 bytes for SHA2-256.

Compression API Stateless operation
------------------------------------

An op is processed stateless if it has
- op_type set to RTE_COMP_OP_STATELESS
- flush value set to RTE_COMP_FLUSH_FULL or RTE_COMP_FLUSH_FINAL
(required only on compression side),
- All required input in source buffer

When all of the above conditions are met, the PMD initiates stateless processing
and releases acquired resources after processing of current operation is
complete. The application can enqueue multiple stateless ops in a single burst
and must attach priv_xform handle to such ops.

priv_xform in Stateless operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A priv_xform is private data managed internally by the PMD to do stateless processing.
A priv_xform is initialized by an application providing a generic xform structure
to ``rte_compressdev_private_xform_create``, which returns an opaque priv_xform reference.
If the PMD supports SHAREABLE priv_xform, indicated via algorithm feature flag,
then the application can attach the same priv_xform with many stateless ops at a time.
If not, then the application needs to create as many priv_xforms as it expects to have
stateless operations in-flight.

.. figure:: img/stateless-op.*

   Stateless Ops using Non-Shareable priv_xform


.. figure:: img/stateless-op-shared.*

   Stateless Ops using Shareable priv_xform


The application should call ``rte_compressdev_private_xform_create()`` and attach it to a stateless
op before enqueuing them for processing and free via ``rte_compressdev_private_xform_free()``
during termination.

An example pseudocode to setup and process NUM_OPS stateless ops with each of length OP_LEN
using priv_xform would look like:

.. code-block:: c

    /*
     * pseudocode for stateless compression
     */

    uint8_t cdev_id = rte_compressdev_get_dev_id(<PMD name>);

    /* configure the device. */
    if (rte_compressdev_configure(cdev_id, &conf) < 0)
        rte_exit(EXIT_FAILURE, "Failed to configure compressdev %u", cdev_id);

    if (rte_compressdev_queue_pair_setup(cdev_id, 0, NUM_MAX_INFLIGHT_OPS,
                            socket_id()) < 0)
        rte_exit(EXIT_FAILURE, "Failed to setup queue pair\n");

    if (rte_compressdev_start(cdev_id) < 0)
        rte_exit(EXIT_FAILURE, "Failed to start device\n");

    /* setup compress transform */
    struct rte_comp_xform compress_xform = {
        .type = RTE_COMP_COMPRESS,
        .compress = {
            .algo = RTE_COMP_ALGO_DEFLATE,
            .deflate = {
                .huffman = RTE_COMP_HUFFMAN_DEFAULT
            },
            .level = RTE_COMP_LEVEL_PMD_DEFAULT,
            .chksum = RTE_COMP_CHECKSUM_NONE,
            .window_size = DEFAULT_WINDOW_SIZE,
            .hash_algo = RTE_COMP_HASH_ALGO_NONE
        }
    };

    /* create priv_xform and initialize it for the compression device. */
    rte_compressdev_info dev_info;
    void *priv_xform = NULL;
    int shareable = 1;
    rte_compressdev_info_get(cdev_id, &dev_info);
    if (dev_info.capabilities->comp_feature_flags & RTE_COMP_FF_SHAREABLE_PRIV_XFORM) {
        rte_compressdev_private_xform_create(cdev_id, &compress_xform, &priv_xform);
    } else {
        shareable = 0;
    }

    /* create operation pool via call to rte_comp_op_pool_create and alloc ops */
    struct rte_comp_op *comp_ops[NUM_OPS];
    rte_comp_op_bulk_alloc(op_pool, comp_ops, NUM_OPS);

    /* prepare ops for compression operations */
    for (i = 0; i < NUM_OPS; i++) {
        struct rte_comp_op *op = comp_ops[i];
        if (!shareable)
            rte_compressdev_private_xform_create(cdev_id, &compress_xform, &op->priv_xform)
        else
            op->private_xform = priv_xform;
        op->op_type = RTE_COMP_OP_STATELESS;
        op->flush_flag = RTE_COMP_FLUSH_FINAL;

        op->src.offset = 0;
        op->dst.offset = 0;
        op->src.length = OP_LEN;
        op->input_chksum = 0;
        setup op->m_src and op->m_dst;
    }
    num_enqd = rte_compressdev_enqueue_burst(cdev_id, 0, comp_ops, NUM_OPS);
    /* wait for this to complete before enqueuing next*/
    do {
        num_deque = rte_compressdev_dequeue_burst(cdev_id, 0 , &processed_ops, NUM_OPS);
    } while (num_dqud < num_enqd);


Stateless and OUT_OF_SPACE
~~~~~~~~~~~~~~~~~~~~~~~~~~

OUT_OF_SPACE is a condition when the output buffer runs out of space and where the PMD
still has more data to produce. If the PMD runs into such condition, then the PMD returns
RTE_COMP_OP_OUT_OF_SPACE_TERMINATED error. In such case, the PMD resets itself and can set
consumed=0 and produced=amount of output it could produce before hitting out_of_space.
The application would need to resubmit the whole input with a larger output buffer, if it
wants the operation to be completed.

Hash in Stateless
~~~~~~~~~~~~~~~~~
If hash is enabled, the digest buffer will contain valid data after an op is successfully
processed i.e. dequeued with status = RTE_COMP_OP_STATUS_SUCCESS.

Checksum in Stateless
~~~~~~~~~~~~~~~~~~~~~
If checksum is enabled, checksum will only be available after an op is successfully
processed i.e. dequeued with status = RTE_COMP_OP_STATUS_SUCCESS.

.. _compressdev_stateful_op:

Compression API Stateful operation
-----------------------------------

The compression API provides RTE_COMP_FF_STATEFUL_COMPRESSION and
RTE_COMP_FF_STATEFUL_DECOMPRESSION feature flag for the PMD to reflect
its support for Stateful operations.

A Stateful operation in DPDK compression means the application invokes enqueue
burst() multiple times to process a related chunk of data because the
application broke the data into several ops.

In such cases
- ops are setup with op_type RTE_COMP_OP_STATEFUL,
- all ops except the last are set with flush value = RTE_COMP_FLUSH_NONE/SYNC
and the last is set with flush value RTE_COMP_FLUSH_FULL/FINAL.

In case of either one or all of the above conditions, the PMD initiates
stateful processing and releases acquired resources after processing the
operation with flush value = RTE_COMP_FLUSH_FULL/FINAL is complete.
Unlike stateless, the application can enqueue only one stateful op from
a particular stream at a time and must attach a stream handle
to each op.

Stream in Stateful operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A stream in DPDK compression is a logical entity which identifies a related set of ops.
For example, one large file broken into multiple chunks, then the file is represented by a stream,
and each chunk of that file is represented by a compression op ``rte_comp_op``.
Whenever an application wants stateful processing of such data, then it must get a stream handle
via making call to ``rte_compressdev_stream_create()`` with an xform, which will return an opaque
stream handle to attach to all of the ops carrying data of that stream.
In stateful processing, every op requires previous op data for compression/decompression.
A PMD allocates and sets up resources such as history, states, etc. within a stream,
which are maintained during the processing of related ops.

Unlike priv_xforms, a stream is always a NON_SHAREABLE entity. One stream handle must be attached
to only one set of related ops and cannot be reused until all of them are processed with a
success/failure status.

.. figure:: img/stateful-op.*

   Stateful Ops


An application should call ``rte_compressdev_stream_create()`` and attach it to the op before
enqueuing them for processing and free via ``rte_compressdev_stream_free()`` during
termination. All ops that are to be processed statefully should carry the *same* stream.

See the `DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_ for details.

An example pseudocode to set up and process a stream having NUM_CHUNKS,
with each chunk size of CHUNK_LEN, would look like:

.. code-block:: c

    /*
     * pseudocode for stateful compression
     */

    uint8_t cdev_id = rte_compressdev_get_dev_id(<PMD name>);

    /* configure the  device. */
    if (rte_compressdev_configure(cdev_id, &conf) < 0)
        rte_exit(EXIT_FAILURE, "Failed to configure compressdev %u", cdev_id);

    if (rte_compressdev_queue_pair_setup(cdev_id, 0, NUM_MAX_INFLIGHT_OPS,
                                    socket_id()) < 0)
        rte_exit(EXIT_FAILURE, "Failed to setup queue pair\n");

    if (rte_compressdev_start(cdev_id) < 0)
        rte_exit(EXIT_FAILURE, "Failed to start device\n");

    /* setup compress transform. */
    struct rte_comp_xform compress_xform = {
        .type = RTE_COMP_COMPRESS,
        .compress = {
            .algo = RTE_COMP_ALGO_DEFLATE,
            .deflate = {
                .huffman = RTE_COMP_HUFFMAN_DEFAULT
            },
            .level = RTE_COMP_LEVEL_PMD_DEFAULT,
            .chksum = RTE_COMP_CHECKSUM_NONE,
            .window_size = DEFAULT_WINDOW_SIZE,
            .hash_algo = RTE_COMP_HASH_ALGO_NONE
        }
    };

    /* create stream */
    void *stream;
    rte_compressdev_stream_create(cdev_id, &compress_xform, &stream);

    /* create an op pool and allocate ops */
    rte_comp_op_bulk_alloc(op_pool, comp_ops, NUM_CHUNKS);

    /* Prepare source and destination mbufs for compression operations */
    unsigned int i;
    for (i = 0; i < NUM_CHUNKS; i++) {
        if (rte_pktmbuf_append(mbufs[i], CHUNK_LEN) == NULL)
            rte_exit(EXIT_FAILURE, "Not enough room in the mbuf\n");
        comp_ops[i]->m_src = mbufs[i];
        if (rte_pktmbuf_append(dst_mbufs[i], CHUNK_LEN) == NULL)
            rte_exit(EXIT_FAILURE, "Not enough room in the mbuf\n");
        comp_ops[i]->m_dst = dst_mbufs[i];
    }

    /* Set up the compress operations. */
    for (i = 0; i < NUM_CHUNKS; i++) {
        struct rte_comp_op *op = comp_ops[i];
        op->stream = stream;
        op->m_src = src_buf[i];
        op->m_dst = dst_buf[i];
        op->op_type = RTE_COMP_OP_STATEFUL;
        if (i == NUM_CHUNKS-1) {
            /* set to final, if last chunk*/
            op->flush_flag = RTE_COMP_FLUSH_FINAL;
        } else {
            /* set to NONE, for all intermediary ops */
            op->flush_flag = RTE_COMP_FLUSH_NONE;
        }
        op->src.offset = 0;
        op->dst.offset = 0;
        op->src.length = CHUNK_LEN;
        op->input_chksum = 0;
        num_enqd = rte_compressdev_enqueue_burst(cdev_id, 0, &op[i], 1);
        /* wait for this to complete before enqueuing next*/
        do {
            num_deqd = rte_compressdev_dequeue_burst(cdev_id, 0 , &processed_ops, 1);
        } while (num_deqd < num_enqd);
        /* analyze the amount of consumed and produced data before pushing next op*/
    }


Stateful and OUT_OF_SPACE
~~~~~~~~~~~~~~~~~~~~~~~~~

If a PMD supports stateful operation, then an OUT_OF_SPACE status is not an actual
error for the PMD. In such a case, the PMD returns with status
RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE with consumed = number of input bytes
read, and produced = length of complete output buffer.
The application should enqueue the next op with source starting at consumed+1, and an
output buffer with available space.

Hash in Stateful
~~~~~~~~~~~~~~~~
If enabled, the digest buffer will contain valid digest after the last op in a stream
(having flush = RTE_COMP_FLUSH_FINAL) is successfully processed i.e. dequeued
with status = RTE_COMP_OP_STATUS_SUCCESS.

Checksum in Stateful
~~~~~~~~~~~~~~~~~~~~
If enabled, the checksum will only be available after the last op in a stream
(having flush = RTE_COMP_FLUSH_FINAL) is successfully processed i.e. dequeued
with status = RTE_COMP_OP_STATUS_SUCCESS.

Burst in compression API
------------------------

Scheduling of compression operations on DPDK's application data path is
performed using a burst oriented asynchronous API set. A queue pair on a compression
device accepts a burst of compression operations using the enqueue burst API.
On physical devices the enqueue burst API will place the operations to be processed
on the device's hardware input queue, for virtual devices the processing of the
operations is usually completed during the enqueue call to the compression
device. The dequeue burst API will retrieve any processed operations available
from the queue pair on the compression device, from physical devices this is usually
directly from the devices processed queue, and for virtual device's from an
``rte_ring`` where processed operations are placed after being processed on the
enqueue call.

A burst in DPDK compression can be a combination of stateless and stateful operations with a
condition that for stateful ops only one op at a time should be enqueued from a particular stream
i.e. two ops should never belong to the same stream in a single burst.
However, a burst may contain multiple stateful ops, as long as each op is attached to a different
stream, i.e. a burst can look like:

+---------------+--------------+--------------+-----------------+--------------+--------------+
| enqueue_burst | op1.no_flush | op2.no_flush | op3.flush_final | op4.no_flush | op5.no_flush |
+---------------+--------------+--------------+-----------------+--------------+--------------+

Where, op1 .. op5 all belong to different independent data units. op1, op2, op4, op5 must be
stateful as stateless ops can only use flush full or final and op3 can be of type stateless or
stateful. Every op with type set to RTE_COMP_OP_STATELESS must be attached to priv_xform and
every op with type set to RTE_COMP_OP_STATEFUL *must* be attached to stream.

Since each operation in a burst is independent and thus can be completed
out of order, applications which need ordering should setup a per-op user data
area, with reordering information so that it can determine enqueue order at
dequeue.

Also, if multiple threads calls enqueue_burst() on the same queue pair then it's
the application's responsibility to use a proper locking mechanism to ensure
exclusive enqueuing of operations.

Enqueue / Dequeue Burst APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The burst enqueue API uses a compression device identifier and a queue pair
identifier to specify the compression device queue pair to schedule the processing on.
The ``nb_ops`` parameter is the number of operations to process which are
supplied in the ``ops`` array of ``rte_comp_op`` structures.
The enqueue function returns the number of operations it actually enqueued for
processing, a return value equal to ``nb_ops`` means that all packets have been
enqueued.

The dequeue API uses the same format as the enqueue API but
the ``nb_ops`` and ``ops`` parameters are now used to specify the max processed
operations the user wishes to retrieve and the location in which to store them.
The API call returns the actual number of processed operations returned, this
can never be larger than ``nb_ops``.

Sample code
-----------

There are unit test applications that show how to use the compressdev library inside
``app/test/test_compressdev.c``

Compression Device API
~~~~~~~~~~~~~~~~~~~~~~

The compressdev Library API is described in the
`DPDK API Reference <https://doc.dpdk.org/api/rte__compressdev_8h.html>`_.
