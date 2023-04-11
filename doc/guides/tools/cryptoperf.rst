..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

dpdk-test-crypto-perf Application
=================================

The ``dpdk-test-crypto-perf`` tool is a Data Plane Development Kit (DPDK)
utility that allows measuring performance parameters of PMDs available in the
crypto tree. There are available two measurement types: throughput and latency.
User can use multiply cores to run tests on but only
one type of crypto PMD can be measured during single application
execution. Cipher parameters, type of device, type of operation and
chain mode have to be specified in the command line as application
parameters. These parameters are checked using device capabilities
structure.

Limitations
-----------
On hardware devices the cycle-count doesn't always represent the actual offload
cost. The cycle-count only represents the offload cost when the hardware
accelerator is not fully loaded, when loaded the cpu cycles freed up by the
offload are still consumed by the test tool and included in the cycle-count.
These cycles are consumed by retries and inefficient API calls enqueuing and
dequeuing smaller bursts than specified by the cmdline parameter. This results
in a larger cycle-count measurement and should not be interpreted as an offload
cost measurement. Using "pmd-cyclecount" mode will give a better idea of
actual costs of hardware acceleration.

On hardware devices the throughput measurement is not necessarily the maximum
possible for the device, e.g. it may be necessary to use multiple cores to keep
the hardware accelerator fully loaded and so measure maximum throughput.


Linearization setting
---------------------

It is possible linearized input segmented packets just before crypto operation
for devices which doesn't support scatter-gather, and allows to measure
performance also for this use case.

To set on the linearization options add below definition to the
``cperf_ops.h`` file::

   #define CPERF_LINEARIZATION_ENABLE


Running the Application
-----------------------

The tool application has a number of command line options:

.. code-block:: console

   dpdk-test-crypto-perf [EAL Options] -- [Application Options]

EAL Options
~~~~~~~~~~~

The following are the EAL command-line options that can be used in conjunction
with the ``dpdk-test-crypto-perf`` application.
See the DPDK Getting Started Guides for more information on these options.

*   ``-c <COREMASK>`` or ``-l <CORELIST>``

        Set the hexadecimal bitmask of the cores to run on. The corelist is a
        list cores to use.

*   ``-a <PCI>``

        Add a PCI device in allow list.

*   ``--vdev <driver><id>``

        Add a virtual device.

Application Options
~~~~~~~~~~~~~~~~~~~

The following are the application command-line options:

* ``--ptest type``

        Set test type, where ``type`` is one of the following::

           throughput
           latency
           verify
           pmd-cyclecount

* ``--silent``

        Disable options dump.

* ``--pool-sz <n>``

        Set the number of mbufs to be allocated in the mbuf pool.

* ``--total-ops <n>``

        Set the number of total operations performed.

* ``--burst-sz <n>``

        Set the number of packets per burst.

        This can be set as:
          * Single value (i.e. ``--burst-sz 16``)
          * Range of values, using the following structure ``min:inc:max``,
            where ``min`` is minimum size, ``inc`` is the increment size and ``max``
            is the maximum size (i.e. ``--burst-sz 16:2:32``)
          * List of values, up to 32 values, separated in commas (i.e. ``--burst-sz 16,24,32``)

* ``--buffer-sz <n>``

        Set the size of single packet (plaintext or ciphertext in it).

        This can be set as:
          * Single value (i.e. ``--buffer-sz 16``)
          * Range of values, using the following structure ``min:inc:max``,
            where ``min`` is minimum size, ``inc`` is the increment size and ``max``
            is the maximum size (i.e. ``--buffer-sz 16:2:32``)
          * List of values, up to 32 values, separated in commas (i.e. ``--buffer-sz 32,64,128``)

* ``--imix <n>``

        Set the distribution of packet sizes.

        A list of weights must be passed, containing the same number of items than buffer-sz,
        so each item in this list will be the weight of the packet size on the same position
        in the buffer-sz parameter (a list have to be passed in that parameter).

        Example:

        To test a distribution of 20% packets of 64 bytes, 40% packets of 100 bytes and 40% packets
        of 256 bytes, the command line would be: ``--buffer-sz 64,100,256 --imix 20,40,40``.
        Note that the weights do not have to be percentages, so using ``--imix 1,2,2`` would result
        in the same distribution

* ``--segment-sz <n>``

        Set the size of the segment to use, for Scatter Gather List testing.
        By default, it is set to the size of the maximum buffer size, including the digest size,
        so a single segment is created.

* ``--devtype <name>``

        Set device type, where ``name`` is one of the following::

           crypto_aesni_gcm
           crypto_aesni_mb
           crypto_armv8
           crypto_cn9k
           crypto_cn10k
           crypto_dpaa_sec
           crypto_dpaa2_sec
           crypto_kasumi
           crypto_mvsam
           crypto_null
           crypto_octeontx
           crypto_openssl
           crypto_qat
           crypto_scheduler
           crypto_snow3g
           crypto_zuc

* ``--optype <name>``

        Set operation type, where ``name`` is one of the following::

           cipher-only
           auth-only
           cipher-then-auth
           auth-then-cipher
           aead
           pdcp
           docsis
           modex

        For GCM/CCM algorithms you should use aead flag.

* ``--sessionless``

        Enable session-less crypto operations mode.

* ``--out-of-place``

        Enable out-of-place crypto operations mode.

* ``--test-file <name>``

        Set test vector file path. See the Test Vector File chapter.

* ``--test-name <name>``

        Set specific test name section in the test vector file.

* ``--cipher-algo <name>``

        Set cipher algorithm name, where ``name`` is one of the following::

           3des-cbc
           3des-ecb
           3des-ctr
           aes-cbc
           aes-ctr
           aes-ecb
           aes-f8
           aes-xts
           arc4
           null
           kasumi-f8
           snow3g-uea2
           zuc-eea3

* ``--cipher-op <mode>``

        Set cipher operation mode, where ``mode`` is one of the following::

           encrypt
           decrypt

* ``--cipher-key-sz <n>``

        Set the size of cipher key.

* ``--cipher-iv-sz <n>``

        Set the size of cipher iv.

* ``--auth-algo <name>``

        Set authentication algorithm name, where ``name`` is one
        of the following::

           aes-cbc-mac
           aes-cmac
           aes-gmac
           aes-xcbc-mac
           md5
           md5-hmac
           sha1
           sha1-hmac
           sha2-224
           sha2-224-hmac
           sha2-256
           sha2-256-hmac
           sha2-384
           sha2-384-hmac
           sha2-512
           sha2-512-hmac
           kasumi-f9
           snow3g-uia2
           zuc-eia3

* ``--auth-op <mode>``

        Set authentication operation mode, where ``mode`` is one of
        the following::

           verify
           generate

* ``--auth-key-sz <n>``

        Set the size of authentication key.

* ``--auth-iv-sz <n>``

        Set the size of auth iv.

* ``--aead-algo <name>``

        Set AEAD algorithm name, where ``name`` is one
        of the following::

           aes-ccm
           aes-gcm

* ``--aead-op <mode>``

        Set AEAD operation mode, where ``mode`` is one of
        the following::

           encrypt
           decrypt

* ``--aead-key-sz <n>``

        Set the size of AEAD key.

* ``--aead-iv-sz <n>``

        Set the size of AEAD iv.

* ``--aead-aad-sz <n>``

        Set the size of AEAD aad.

* ``--digest-sz <n>``

        Set the size of digest.

* ``--desc-nb <n>``

        Set number of descriptors for each crypto device.

* ``--pmd-cyclecount-delay-ms <n>``

        Add a delay (in milliseconds) between enqueue and dequeue in
        pmd-cyclecount benchmarking mode (useful when benchmarking
        hardware acceleration).

* ``--csv-friendly``

        Enable test result output CSV friendly rather than human friendly.

* ``--pdcp-sn-sz <n>``

        Set PDCP sequence number size(n) in bits. Valid values of n will
        be 5/7/12/15/18.

* ``--pdcp-domain <control/user>``

        Set PDCP domain to specify short_mac/control/user plane.

* ``--docsis-hdr-sz <n>``

        Set DOCSIS header size(n) in bytes.

* ``--pdcp-ses-hfn-en``

        Enable fixed session based HFN instead of per packet HFN.

* ``--enable-sdap``

        Enable Service Data Adaptation Protocol.

* ``--modex-len <n>``

        Set modex length for asymmetric crypto perf test.
        Supported lengths are 60, 128, 255, 448. Default length is 128.

Test Vector File
~~~~~~~~~~~~~~~~

The test vector file is a text file contain information about test vectors.
The file is made of the sections. The first section doesn't have header.
It contain global information used in each test variant vectors -
typically information about plaintext, ciphertext, cipher key, auth key,
initial vector. All other sections begin header.
The sections contain particular information typically digest.

**Format of the file:**

Each line beginning with sign '#' contain comment and it is ignored by parser::

   # <comment>

Header line is just name in square bracket::

   [<section name>]

Data line contain information token then sign '=' and
a string of bytes in C byte array format::

   <token> = <C byte array>

**Tokens list:**

* ``plaintext``

        Original plaintext to be encrypted.

* ``ciphertext``

        Encrypted plaintext string.

* ``cipher_key``

        Key used in cipher operation.

* ``auth_key``

        Key used in auth operation.

* ``cipher_iv``

        Cipher Initial Vector.

* ``auth_iv``

        Auth Initial Vector.

* ``aad``

        Additional data.

* ``digest``

        Digest string.

Examples
--------

Call application for performance throughput test of single Aesni MB PMD
for cipher encryption aes-cbc and auth generation sha1-hmac,
one million operations, burst size 32, packet size 64::

   dpdk-test-crypto-perf -l 6-7 --vdev crypto_aesni_mb -a 0000:00:00.0 --
   --ptest throughput --devtype crypto_aesni_mb --optype cipher-then-auth
   --cipher-algo aes-cbc --cipher-op encrypt --cipher-key-sz 16 --auth-algo
   sha1-hmac --auth-op generate --auth-key-sz 64 --digest-sz 12
   --total-ops 10000000 --burst-sz 32 --buffer-sz 64

Call application for performance latency test of two Aesni MB PMD executed
on two cores for cipher encryption aes-cbc, ten operations in silent mode::

   dpdk-test-crypto-perf -l 4-7 --vdev crypto_aesni_mb1
   --vdev crypto_aesni_mb2 -a 0000:00:00.0 -- --devtype crypto_aesni_mb
   --cipher-algo aes-cbc --cipher-key-sz 16 --cipher-iv-sz 16
   --cipher-op encrypt --optype cipher-only --silent
   --ptest latency --total-ops 10

Call application for verification test of single open ssl PMD
for cipher encryption aes-gcm and auth generation aes-gcm,ten operations
in silent mode, test vector provide in file "test_aes_gcm.data"
with packet verification::

   dpdk-test-crypto-perf -l 4-7 --vdev crypto_openssl -a 0000:00:00.0 --
   --devtype crypto_openssl --aead-algo aes-gcm --aead-key-sz 16
   --aead-iv-sz 16 --aead-op encrypt --aead-aad-sz 16 --digest-sz 16
   --optype aead --silent --ptest verify --total-ops 10
   --test-file test_aes_gcm.data

Test vector file for cipher algorithm aes cbc 256 with authorization sha::

   # Global Section
   plaintext =
   0xff, 0xca, 0xfb, 0xf1, 0x38, 0x20, 0x2f, 0x7b, 0x24, 0x98, 0x26, 0x7d, 0x1d, 0x9f, 0xb3, 0x93,
   0xd9, 0xef, 0xbd, 0xad, 0x4e, 0x40, 0xbd, 0x60, 0xe9, 0x48, 0x59, 0x90, 0x67, 0xd7, 0x2b, 0x7b,
   0x8a, 0xe0, 0x4d, 0xb0, 0x70, 0x38, 0xcc, 0x48, 0x61, 0x7d, 0xee, 0xd6, 0x35, 0x49, 0xae, 0xb4,
   0xaf, 0x6b, 0xdd, 0xe6, 0x21, 0xc0, 0x60, 0xce, 0x0a, 0xf4, 0x1c, 0x2e, 0x1c, 0x8d, 0xe8, 0x7b
   ciphertext =
   0x77, 0xF9, 0xF7, 0x7A, 0xA3, 0xCB, 0x68, 0x1A, 0x11, 0x70, 0xD8, 0x7A, 0xB6, 0xE2, 0x37, 0x7E,
   0xD1, 0x57, 0x1C, 0x8E, 0x85, 0xD8, 0x08, 0xBF, 0x57, 0x1F, 0x21, 0x6C, 0xAD, 0xAD, 0x47, 0x1E,
   0x0D, 0x6B, 0x79, 0x39, 0x15, 0x4E, 0x5B, 0x59, 0x2D, 0x76, 0x87, 0xA6, 0xD6, 0x47, 0x8F, 0x82,
   0xB8, 0x51, 0x91, 0x32, 0x60, 0xCB, 0x97, 0xDE, 0xBE, 0xF0, 0xAD, 0xFC, 0x23, 0x2E, 0x22, 0x02
   cipher_key =
   0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2, 0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A,
   0xd0, 0xe7, 0x4b, 0xfb, 0x5d, 0xe5, 0x0c, 0xe7, 0x6f, 0x21, 0xb5, 0x52, 0x2a, 0xbb, 0xc7, 0xf7
   auth_key =
   0xaf, 0x96, 0x42, 0xf1, 0x8c, 0x50, 0xdc, 0x67, 0x1a, 0x43, 0x47, 0x62, 0xc7, 0x04, 0xab, 0x05,
   0xf5, 0x0c, 0xe7, 0xa2, 0xa6, 0x23, 0xd5, 0x3d, 0x95, 0xd8, 0xcd, 0x86, 0x79, 0xf5, 0x01, 0x47,
   0x4f, 0xf9, 0x1d, 0x9d, 0x36, 0xf7, 0x68, 0x1a, 0x64, 0x44, 0x58, 0x5d, 0xe5, 0x81, 0x15, 0x2a,
   0x41, 0xe4, 0x0e, 0xaa, 0x1f, 0x04, 0x21, 0xff, 0x2c, 0xf3, 0x73, 0x2b, 0x48, 0x1e, 0xd2, 0xf7
   cipher_iv =
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
   # Section sha 1 hmac buff 32
   [sha1_hmac_buff_32]
   digest =
   0x36, 0xCA, 0x49, 0x6A, 0xE3, 0x54, 0xD8, 0x4F, 0x0B, 0x76, 0xD8, 0xAA, 0x78, 0xEB, 0x9D, 0x65,
   0x2C, 0xCA, 0x1F, 0x97
   # Section sha 256 hmac buff 32
   [sha256_hmac_buff_32]
   digest =
   0x1C, 0xB2, 0x3D, 0xD1, 0xF9, 0xC7, 0x6C, 0x49, 0x2E, 0xDA, 0x94, 0x8B, 0xF1, 0xCF, 0x96, 0x43,
   0x67, 0x50, 0x39, 0x76, 0xB5, 0xA1, 0xCE, 0xA1, 0xD7, 0x77, 0x10, 0x07, 0x43, 0x37, 0x05, 0xB4


Graph Crypto Perf Results
-------------------------

The ``dpdk-graph-crypto-perf.py`` tool is a simple script to automate
running crypto performance tests, and graphing the results.
It can be found in the ``app/test-crypto-perf/`` directory.
The output graphs include various grouped barcharts for throughput
tests, and histogram and boxplot graphs for latency tests.
These are output to PDF files, with one PDF per test suite graph type.


Dependencies
~~~~~~~~~~~~

The following python modules must be installed to run the script:

.. code-block:: console

   pip3 install img2pdf plotly pandas psutil kaleido


Test Configuration
~~~~~~~~~~~~~~~~~~

The test cases run by the script are defined by a JSON config file.
Some config files can be found in ``app/test-crypto-perf/configs/``,
or the user may create a new one following the same format as the config files provided.

An example of this format is shown below for one test suite in the ``crypto-perf-aesni-mb.json`` file.
This shows the required default config for the test suite, and one test case.
The test case has additional app config that will be combined with
the default config when running the test case.

.. code-block:: c

   "throughput": {
       "default": {
           "eal": {
               "l": "1,2",
               "vdev": "crypto_aesni_mb"
           },
           "app": {
               "csv-friendly": true,
               "buffer-sz": "64,128,256,512,768,1024,1408,2048",
               "burst-sz": "1,4,8,16,32",
               "ptest": "throughput",
               "devtype": "crypto_aesni_mb"
           }
        },
       "AES-CBC-128 SHA1-HMAC auth-then-cipher decrypt": {
               "cipher-algo": "aes-cbc",
               "cipher-key-sz": "16",
               "auth-algo": "sha1-hmac",
               "optype": "auth-then-cipher",
               "cipher-op": "decrypt"
        }
   }

.. note::
   The specific test cases only allow modification of app parameters,
   and not EAL parameters.
   The default case is required for each test suite in the config file,
   to specify EAL parameters.

Currently, crypto_qat, crypto_aesni_mb, and crypto_aesni_gcm devices for
both throughput and latency ptests are supported.


Usage
~~~~~

.. code-block:: console

   ./dpdk-graph-crypto-perf <config_file>

The ``config_file`` positional argument is required to run the script.
This points to a valid JSON config file containing test suites.

.. code-block:: console

   ./dpdk-graph-crypto-perf configs/crypto-perf-aesni-mb.json

The following are the application optional command-line options:

* ``-h, --help``

  Display usage information and quit.

* ``-f <file_path>, --file-path <file_path>``

  Provide path to ``dpdk-test-crypto-perf`` application.
  The script uses the installed app by default.

  .. code-block:: console

     ./dpdk-graph-crypto-perf <config_file> \
         -f <build_dir>/app/dpdk-test-crypto-perf

* ``-t <test_suite_list>, --test-suites <test_suite_list>``

  Specify test suites to run. All test suites are run by default.

  To run crypto-perf-qat latency test suite only:

  .. code-block:: console

     ./dpdk-graph-crypto-perf configs/crypto-perf-qat -t latency

  To run both crypto-perf-aesni-mb throughput and latency test suites

  .. code-block:: console

     ./dpdk-graph-crypto-perf configs/crypto-perf-aesni-mb -t throughput latency

* ``-o <output_path>, --output-path <output_path>``

  Specify directory to use for output files.
  The default is to use the script's directory.

  .. code-block:: console

     ./dpdk-graph-crypto-perf <config_file> -o <output_dir>

* ``-v, --verbose``

  Enable verbose output. This displays ``dpdk-test-crypto-perf`` app output in real-time.

  .. code-block:: console

     ./dpdk-graph-crypto-perf <config_file> -v

  .. warning::
     Latency performance tests have a large amount of output.
     It is not recommended to use the verbose option for latency tests.
