..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2024 Arm Limited.

Pointer Compression Library
===========================

Use ``rte_ptr_compress_16_shift()`` and ``rte_ptr_decompress_16_shift()``
to compress and decompress pointers into 16-bit offsets.
Use ``rte_ptr_compress_32_shift()`` and ``rte_ptr_decompress_32_shift()``
to compress and decompress pointers into 32-bit offsets.

Compression takes advantage of the fact that pointers are usually located in a limited memory region (like a mempool).
By converting them to offsets from a base memory address they can be stored in fewer bytes.
How many bytes are needed to store the offset is dictated by the memory region size and alignment of objects the pointers point to.

For example, a pointer which is part of a 4GB memory pool can be stored as 32 bit offset.
If the pointer points to memory that is 8 bytes aligned then 3 bits can be dropped from the offset and
a 32GB memory pool can now fit in 32 bits.

For performance reasons these requirements are not enforced programmatically.
The programmer is responsible for ensuring that the combination of distance from the base pointer and
memory alignment allow for storing of the offset in the number of bits indicated by the function name (16 or 32).
Start of mempool memory would be a good candidate for the base pointer.
Otherwise any pointer that precedes all pointers, is close enough and
has the same alignment as the pointers being compressed will work.

Macros present in the rte_ptr_compress.h header may be used to evaluate whether compression is possible:

*   RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE

*   RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT

*   RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT

*   RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT

These will help you calculate compression parameters and whether these are legal for particular compression function.

If using a mempool you can get the parameters you need to use in the compression macros and functions
by using ``rte_mempool_get_mem_range()`` and ``rte_mempool_get_obj_alignment()``.

.. note::

    Performance gains depend on the batch size of pointers and CPU capabilities such as vector extensions.
    It's important to measure the performance increase on target hardware.
    A test called ``ring_perf_autotest`` in ``dpdk-test`` can provide the measurements.

Example usage
-------------

In this example we send pointers between two cores through a ring.
While this is a realistic use case the code is simplified for demonstration purposes and does not have error handling.

.. code-block:: c

    #include <rte_launch.h>
    #include <rte_ptr_compress.h>
    #include <rte_ring.h>
    #include <rte_ring_elem.h>

    #define ITEMS_ARRAY_SIZE (1024)
    #define BATCH_SIZE (128)
    #define ALIGN_EXPONENT (3)
    #define ITEM_ALIGN (1<<ALIGN_EXPONENT)
    #define CORE_SEND (1)
    #define CORE_RECV (2)

    struct item {
      alignas(ITEM_ALIGN) int a;
    };

    static struct item items[ITEMS_ARRAY_SIZE] = {0};
    static struct rte_ring *ring = NULL;

    static int
    send_compressed(void *args)
    {
      struct item *ptrs_send[BATCH_SIZE] = {0};
      unsigned int n_send = 0;
      struct rte_ring_zc_data zcd = {0};

      /* in this example we only fill the ptrs_send once and reuse */
      for (;n_send < BATCH_SIZE; n_send++)
        ptrs_send[n_send] = &items[n_send];

      for(;;) {
        n_send = rte_ring_enqueue_zc_burst_elem_start(
          ring, sizeof(uint32_t), BATCH_SIZE, &zcd, NULL);

        /* compress ptrs_send into offsets */
        rte_ptr_compress_32_shift(items, /* base pointer */
          ptrs_send, /* source array to be compressed */
          zcd.ptr1, /* destination array to store offsets */
          zcd.n1, /* how many pointers to compress */
          ALIGN_EXPONENT /* how many bits can we drop from the offset */);

        if (zcd.ptr2 != NULL)
          rte_ptr_compress_32_shift(items, ptrs_send + zcd.n1,
            zcd.ptr2, n_send - zcd.n1, ALIGN_EXPONENT);

        rte_ring_enqueue_zc_finish(ring, n_send);
      }
      return 1;
    }

    static int
    recv_compressed(void *args)
    {
      struct item *ptrs_recv[BATCH_SIZE] = {0};
      unsigned int n_recv;
      struct rte_ring_zc_data zcd = {0};

      for(;;) {
        /* receive compressed pointers from the ring */
        n_recv = rte_ring_dequeue_zc_burst_elem_start(
          ring, sizeof(uint32_t), BATCH_SIZE, &zcd, NULL);

        rte_ptr_decompress_32_shift(items, /* base pointer */
          zcd.ptr1, /* source array to decompress */
          ptrs_recv, /* destination array to store pointers */
          zcd.n1, /* how many pointers to decompress */
          ALIGN_EXPONENT /* how many bits were dropped from the offset */);

        /* handle the potential secondary buffer (caused by ring boundary) */
        if (zcd.ptr2 != NULL)
          rte_ptr_decompress_32_shift(items,
            zcd.ptr2,
            ptrs_recv + zcd.n1,
            n_recv - zcd.n1,
            ALIGN_EXPONENT);

        rte_ring_dequeue_zc_finish(ring, n_recv);

        /* ptrs_recv contains what ptrs_send contained in the other thread */
        /* (...) */
      }
      return 1;
    }

    void
    compression_example(void)
    {
      ring = rte_ring_create_elem(
        "COMPR_PTRS", sizeof(uint32_t),
        1024, rte_socket_id(),
        RING_F_SP_ENQ | RING_F_SC_DEQ);

      rte_eal_remote_launch(send_compressed, NULL, CORE_SEND);
      rte_eal_remote_launch(recv_compressed, NULL, CORE_RECV);

      for(;;) {}
    }
