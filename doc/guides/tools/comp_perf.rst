..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

dpdk-test-compress-perf Tool
============================

The ``dpdk-test-compress-perf`` tool is a Data Plane Development Kit (DPDK)
utility that allows measuring performance parameters of PMDs available in the
compress tree. The tool reads the data from a file (--input-file),
dumps all the file into a buffer and fills out the data of input mbufs,
which are passed to compress device with compression operations.
Then, the output buffers are fed into the decompression stage, and the resulting
data is compared against the original data (verification phase). After that,
a number of iterations are performed, compressing first and decompressing later,
to check the throughput rate
(showing cycles/iteration, cycles/Byte and Gbps, for compression and decompression).

.. Note::

	if the max-num-sgl-segs x seg_sz > input size then segments number in
	the chain will be lower than value passed into max-num-sgl-segs.


Limitations
~~~~~~~~~~~

* Stateful operation is not supported in this version.


Command line options
--------------------

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

 ``-h``: prints this help


Compiling the Tool
------------------

**Step 1: PMD setting**

The ``dpdk-test-compress-perf`` tool depends on compression device drivers PMD which
can be disabled by default in the build configuration file ``common_base``.
The compression device drivers PMD which should be tested can be enabled by setting e.g.::

   CONFIG_RTE_LIBRTE_PMD_ISAL=y


Running the Tool
----------------

The tool has a number of command line options. Here is the sample command line:

.. code-block:: console

   ./build/app/dpdk-test-compress-perf  -l 4 -- --driver-name compress_qat --input-file test.txt --seg-sz 8192
    --compress-level 1:1:9 --num-iter 10 --extended-input-sz 1048576  --max-num-sgl-segs 16 --huffman-enc fixed
