..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

dpdk-test-compress-perf Tool
============================

The ``dpdk-test-compress-perf`` tool is a Data Plane Development Kit (DPDK)
utility that allows measuring performance parameters of PMDs available in the
compress tree. User can use multiple cores to run tests on but only
one type of compression PMD can be measured during single application
execution. The tool reads the data from a file (--input-file),
dumps all the file into a buffer and fills out the data of input mbufs,
which are passed to compress device with compression operations.
Then, the output buffers are fed into the decompression stage, and the resulting
data is compared against the original data (verification phase). After that,
a number of iterations are performed, compressing first and decompressing later,
to check the throughput rate (showing cycles/iteration, cycles/Byte and Gbps,
for compression and decompression).
Another option: ``pmd-cyclecount``, gives the user the opportunity to measure
the number of cycles per operation for the 3 phases: setup, enqueue_burst and
dequeue_burst, for both compression and decompression. An optional delay can be
inserted between enqueue and dequeue so no cycles are wasted in retries while
waiting for a hardware device to finish. Although artificial, this allows
to measure the minimum offload cost which could be achieved in a perfectly
tuned system. Comparing the results of the two tests gives information about
the trade-off between throughput and cycle-count.

.. Note::

	if the max-num-sgl-segs x seg_sz > input size then segments number in
	the chain will be lower than value passed into max-num-sgl-segs.


Limitations
~~~~~~~~~~~

* Stateful operation is not supported in this version.

EAL Options
~~~~~~~~~~~

The following are the EAL command-line options that can be used in conjunction
with the ``dpdk-test-compress-perf`` application.
See the DPDK Getting Started Guides for more information on these options.

*   ``-c <COREMASK>`` or ``-l <CORELIST>``

	Set the hexadecimal bitmask of the cores to run on. The corelist is a
	list cores to use.

.. Note::

	One lcore is needed for process admin, tests are run on all other cores.
	To run tests on two lcores, three lcores must be passed to the tool.

*   ``-a <PCI>``

	Add a PCI device in allow list.

*   ``--vdev <driver><id>``

	Add a virtual device.

Application Options
~~~~~~~~~~~~~~~~~~~

 ``--ptest [throughput/verify/pmd-cyclecount]``: set test type (default: throughput)

 ``--driver-name NAME``: compress driver to use

 ``--input-file NAME``: file to compress and decompress

 ``--extended-input-sz N``: extend file data up to this size (default: no extension)

 ``--seg-sz N``: size of segment to store the data (default: 2048)

 ``--burst-sz N``: compress operation burst size

 ``--pool-sz N``: mempool size for compress operations/mbufs (default: 8192)

 ``--max-num-sgl-segs N``: maximum number of segments for each mbuf (default: 16)

 ``--num-iter N``: number of times the file will be compressed/decompressed (default: 10000)

 ``--operation [comp/decomp/comp_and_decomp]``: perform test on compression, decompression or both operations

 ``--huffman-enc [fixed/dynamic/default]``: Huffman encoding (default: dynamic)

 ``--compress-level N``: compression level, which could be a single value, list or range (default: range between 1 and 9)

 ``--window-sz N``: base two log value of compression window size (default: max supported by PMD)

 ``--external-mbufs``: allocate and use memzones as external buffers instead of keeping the data directly in mbuf areas

 ``--cc-delay-us N``: delay between enqueue and dequeue operations in microseconds, valid only for the cyclecount test (default: 500 us)

 ``-h``: prints this help


Running the Tool
----------------

The tool has a number of command line options. Here is the sample command line:

.. code-block:: console

   ./<build_dir>/app/dpdk-test-compress-perf  -l 4 -- --driver-name compress_qat --input-file test.txt --seg-sz 8192
    --compress-level 1:1:9 --num-iter 10 --extended-input-sz 1048576  --max-num-sgl-segs 16 --huffman-enc fixed
