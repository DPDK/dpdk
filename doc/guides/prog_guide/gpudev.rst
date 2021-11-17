.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2021 NVIDIA Corporation & Affiliates

General-Purpose Graphics Processing Unit Library
================================================

When mixing networking activity with task processing on a GPU device,
there may be the need to put in communication the CPU with the device
in order to manage the memory, synchronize operations, exchange info, etc..

By means of the generic GPU interface provided by this library,
it is possible to allocate a chunk of GPU memory and use it
to create a DPDK mempool with external mbufs having the payload
on the GPU memory, enabling any network interface card
(which support this feature like Mellanox NIC)
to directly transmit and receive packets using GPU memory.

Additionally, this library provides a number of functions
to enhance the dialog between CPU and GPU.

Out of scope of this library is to provide a wrapper for GPU specific libraries
(e.g. CUDA Toolkit or OpenCL), thus it is not possible to launch workload
on the device or create GPU specific objects
(e.g. CUDA Driver context or CUDA Streams in case of NVIDIA GPUs).

This library is optional in DPDK
and can be disabled with ``-Ddisable_libs=gpudev``.


Features
--------

This library provides a number of features:

- Interoperability with device-specific library through generic handlers.
- Allocate and free memory on the device.
- Register CPU memory to make it visible from the device.
- Communication between the CPU and the device.

The whole CPU - GPU communication is implemented
using CPU memory visible from the GPU.


API Overview
------------

Child Device
~~~~~~~~~~~~

By default, DPDK PCIe module detects and registers physical GPU devices
in the system.
With the gpudev library is also possible to add additional non-physical devices
through an ``uint64_t`` generic handler (e.g. CUDA Driver context)
that will be registered internally by the driver as an additional device (child)
connected to a physical device (parent).
Each device (parent or child) is represented through a ID
required to indicate which device a given operation should be executed on.

Memory Allocation
~~~~~~~~~~~~~~~~~

gpudev can allocate on an input given GPU device a memory area
returning the pointer to that memory.
Later, it's also possible to free that memory with gpudev.
GPU memory allocated outside of the gpudev library
(e.g. with GPU-specific library) cannot be freed by the gpudev library.

Memory Registration
~~~~~~~~~~~~~~~~~~~

gpudev can register a CPU memory area to make it visible from a GPU device.
Later, it's also possible to unregister that memory with gpudev.
CPU memory registered outside of the gpudev library
(e.g. with GPU specific library) cannot be unregistered by the gpudev library.

Memory Barrier
~~~~~~~~~~~~~~

Some GPU drivers may need, under certain conditions,
to enforce the coherency of external devices writes (e.g. NIC receiving packets)
into the GPU memory.
gpudev abstracts and exposes this capability.

Communication Flag
~~~~~~~~~~~~~~~~~~

Considering an application with some GPU task
that's waiting to receive a signal from the CPU
to move forward with the execution.
The communication flag allocates a CPU memory GPU-visible ``uint32_t`` flag
that can be used by the CPU to communicate with a GPU task.

Communication list
~~~~~~~~~~~~~~~~~~

By default, DPDK pulls free mbufs from a mempool to receive packets.
Best practice, especially in a multithreaded application,
is to no make any assumption on which mbufs will be used
to receive the next bursts of packets.
Considering an application with a GPU memory mempool
attached to a receive queue having some task waiting on the GPU
to receive a new burst of packets to be processed,
there is the need to communicate from the CPU
the list of mbuf payload addresses where received packet have been stored.
The ``rte_gpu_comm_*()`` functions are responsible to create a list of packets
that can be populated with receive mbuf payload addresses
and communicated to the task running on the GPU.


CUDA Example
------------

In the example below, there is a pseudo-code to give an example
about how to use functions in this library in case of a CUDA application.

.. code-block:: c

   //////////////////////////////////////////////////////////////////////////
   ///// gpudev library + CUDA functions
   //////////////////////////////////////////////////////////////////////////
   #define GPU_PAGE_SHIFT 16
   #define GPU_PAGE_SIZE (1UL << GPU_PAGE_SHIFT)

   int main()
   {
       struct rte_gpu_flag quit_flag;
       struct rte_gpu_comm_list *comm_list;
       int nb_rx = 0;
       int comm_list_entry = 0;
       struct rte_mbuf *rx_mbufs[max_rx_mbufs];
       cudaStream_t cstream;
       struct rte_mempool *mpool_payload, *mpool_header;
       struct rte_pktmbuf_extmem ext_mem;
       int16_t dev_id;
       int16_t port_id = 0;

       /* Initialize CUDA objects (cstream, context, etc..). */
       /* Use gpudev library to register a new CUDA context if any. */

       /* Let's assume the application wants to use the default context of the GPU device 0. */
       dev_id = 0;

       /* Create an external memory mempool using memory allocated on the GPU. */
       ext_mem.elt_size = mbufs_headroom_size;
       ext_mem.buf_len = RTE_ALIGN_CEIL(mbufs_num * ext_mem.elt_size, GPU_PAGE_SIZE);
       ext_mem.buf_iova = RTE_BAD_IOVA;
       ext_mem.buf_ptr = rte_gpu_mem_alloc(dev_id, ext_mem.buf_len, 0);
       rte_extmem_register(ext_mem.buf_ptr, ext_mem.buf_len, NULL, ext_mem.buf_iova, GPU_PAGE_SIZE);
       rte_dev_dma_map(rte_eth_devices[port_id].device,
               ext_mem.buf_ptr, ext_mem.buf_iova, ext_mem.buf_len);
       mpool_payload = rte_pktmbuf_pool_create_extbuf("gpu_mempool", mbufs_num,
                                                      0, 0, ext_mem.elt_size,
                                                      rte_socket_id(), &ext_mem, 1);

       /*
        * Create CPU - device communication flag.
        * With this flag, the CPU can tell to the CUDA kernel to exit from the main loop.
        */
       rte_gpu_comm_create_flag(dev_id, &quit_flag, RTE_GPU_COMM_FLAG_CPU);
       rte_gpu_comm_set_flag(&quit_flag , 0);

       /*
        * Create CPU - device communication list.
        * Each entry of this list will be populated by the CPU
        * with a new set of received mbufs that the CUDA kernel has to process.
        */
       comm_list = rte_gpu_comm_create_list(dev_id, num_entries);

       /* A very simple CUDA kernel with just 1 CUDA block and RTE_GPU_COMM_LIST_PKTS_MAX CUDA threads. */
       cuda_kernel_packet_processing<<<1, RTE_GPU_COMM_LIST_PKTS_MAX, 0, cstream>>>(quit_flag->ptr, comm_list, num_entries, ...);

       /*
        * For simplicity, the CPU here receives only 2 bursts of mbufs.
        * In a real application, network activity and device processing should overlap.
        */
       nb_rx = rte_eth_rx_burst(port_id, queue_id, &(rx_mbufs[0]), max_rx_mbufs);
       rte_gpu_comm_populate_list_pkts(comm_list[0], rx_mbufs, nb_rx);
       nb_rx = rte_eth_rx_burst(port_id, queue_id, &(rx_mbufs[0]), max_rx_mbufs);
       rte_gpu_comm_populate_list_pkts(comm_list[1], rx_mbufs, nb_rx);

       /*
        * CPU waits for the completion of the packets' processing on the CUDA kernel
        * and then it does a cleanup of the received mbufs.
        */
       while (rte_gpu_comm_cleanup_list(comm_list[0]));
       while (rte_gpu_comm_cleanup_list(comm_list[1]));

       /* CPU notifies the CUDA kernel that it has to terminate. */
       rte_gpu_comm_set_flag(&quit_flag, 1);

       /* gpudev objects cleanup/destruction */
       rte_gpu_mem_free(dev_id, ext_mem.buf_len);

       return 0;
   }

   //////////////////////////////////////////////////////////////////////////
   ///// CUDA kernel
   //////////////////////////////////////////////////////////////////////////

   void cuda_kernel(uint32_t * quit_flag_ptr, struct rte_gpu_comm_list *comm_list, int comm_list_entries)
   {
       int comm_list_index = 0;
       struct rte_gpu_comm_pkt *pkt_list = NULL;

       /* Do some pre-processing operations. */

       /* GPU kernel keeps checking this flag to know if it has to quit or wait for more packets. */
       while (*quit_flag_ptr == 0) {
           if (comm_list[comm_list_index]->status != RTE_GPU_COMM_LIST_READY)
               continue;

           if (threadIdx.x < comm_list[comm_list_index]->num_pkts)
           {
               /* Each CUDA thread processes a different packet. */
               packet_processing(comm_list[comm_list_index]->addr, comm_list[comm_list_index]->size, ..);
           }
           __threadfence();
           __syncthreads();

           /* Wait for new packets on the next communication list entry. */
           comm_list_index = (comm_list_index+1) % comm_list_entries;
       }

       /* Do some post-processing operations. */
   }
