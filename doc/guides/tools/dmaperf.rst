..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2023 Intel Corporation.

dpdk-test-dma-perf Application
==============================

The ``dpdk-test-dma-perf`` tool is a Data Plane Development Kit (DPDK) application
that enables testing the performance of DMA (Direct Memory Access) devices available within DPDK.
It provides a test framework to assess the performance of CPU and DMA devices
under various scenarios, such as varying buffer lengths.
Doing so provides insight into the potential performance
when using these DMA devices for acceleration in DPDK applications.

It supports memory copy performance tests for now,
comparing the performance of CPU and DMA automatically in various conditions
with the help of a pre-set configuration file.


Configuration
-------------

This application uses inherent DPDK EAL command-line options
as well as custom command-line options in the application.
An example configuration file for the application is provided
and gives the meanings for each parameter.

Here is an extracted sample from the configuration file
(the complete sample can be found in the application source directory):

.. code-block:: ini

   [case1]
   type=DMA_MEM_COPY
   mem_size=10
   buf_size=64,8192,2,MUL
   dma_ring_size=1024
   kick_batch=32
   src_numa_node=0
   dst_numa_node=0
   cache_flush=0
   test_seconds=2
   lcore_dma=lcore10@0000:00:04.2, lcore11@0000:00:04.3
   eal_args=--in-memory --file-prefix=test

   [case2]
   type=CPU_MEM_COPY
   mem_size=10
   buf_size=64,8192,2,MUL
   src_numa_node=0
   dst_numa_node=1
   cache_flush=0
   test_seconds=2
   lcore = 3, 4
   eal_args=--in-memory --no-pci

The configuration file is divided into multiple sections, each section represents a test case.
The four variables ``mem_size``, ``buf_size``, ``dma_ring_size``, and ``kick_batch``
can vary in each test case.
The format for this is ``variable=first,last,increment,ADD|MUL``.
This means that the first value of the variable is 'first',
the last value is 'last',
'increment' is the step size,
and 'ADD|MUL' indicates whether the change is by addition or multiplication.

Each case can only have one variable change,
and each change will generate a scenario, so each case can have multiple scenarios.


Configuration Parameters
~~~~~~~~~~~~~~~~~~~~~~~~

``type``
  The type of the test.
  Currently supported types are ``DMA_MEM_COPY`` and ``CPU_MEM_COPY``.

``mem_size``
  The size of the memory footprint.

``buf_size``
  The memory size of a single operation.

``dma_ring_size``
  The DMA ring buffer size. Must be a power of two, and between ``64`` and ``4096``.

``kick_batch``
  The DMA operation batch size, should be greater than ``1`` normally.

``src_numa_node``
  Controls the NUMA node where the source memory is allocated.

``dst_numa_node``
  Controls the NUMA node where the destination memory is allocated.

``cache_flush``
  Determines whether the cache should be flushed.
  ``1`` indicates to flush and ``0`` to not flush.

``test_seconds``
  Controls the test time for each scenario.

``lcore_dma``
  Specifies the lcore/DMA mapping.

.. note::

   The mapping of lcore to DMA must be one-to-one and cannot be duplicated.

``lcore``
  Specifies the lcore for CPU testing.

``eal_args``
  Specifies the EAL arguments.


Running the Application
-----------------------

Typical command-line invocation to execute the application:

.. code-block:: console

   dpdk-test-dma-perf --config=./config_dma.ini --result=./res_dma.csv

Where ``config_dma.ini`` is the configuration file,
and ``res_dma.csv`` will be the generated result file.

If no result file is specified, the test results are found in a file
with the same name as the configuration file with the addition of ``_result.csv`` at the end.


Limitations
-----------

Currently, this tool only supports memory copy performance tests.
Additional enhancements are possible in the future
to support more types of tests for DMA devices and CPUs.
