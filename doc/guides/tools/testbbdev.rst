..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation

dpdk-test-bbdev Application
===========================

The ``dpdk-test-bbdev`` tool is a Data Plane Development Kit (DPDK) utility that
allows measuring performance parameters of PMDs available in the bbdev framework.
Tests available for execution are: latency, throughput, validation,
bler and sanity tests. Execution of tests can be customized using various
parameters passed to a python running script.


Running the Application
-----------------------

The tool application has a number of command line options:

.. code-block:: console

    test-bbdev.py [-h] [-p TESTAPP_PATH] [-e EAL_PARAMS] [-t TIMEOUT]
                       [-c TEST_CASE [TEST_CASE ...]]
                       [-v TEST_VECTOR [TEST_VECTOR...]] [-n NUM_OPS]
                       [-b BURST_SIZE [BURST_SIZE ...]] [-l NUM_LCORES]
                       [-t MAX_ITERS [MAX_ITERS ...]]
                       [-s SNR [SNR ...]]

command-line Options
~~~~~~~~~~~~~~~~~~~~

The following are the command-line options:

``-h, --help``
 Shows help message and exit.

``-p TESTAPP_PATH, --testapp_path TESTAPP_PATH``
 Indicates the path to the bbdev test app. If not specified path is set based
 on "../.." concatenated with "*/build/app/dpdk-test-bbdev*".

``-e EAL_PARAMS, --eal_params EAL_PARAMS``
 Specifies EAL arguments which are passed to the test app. For more details,
 refer to DPDK documentation at :doc:`../linux_gsg/linux_eal_parameters`.

``-t TIMEOUT, --timeout TIMEOUT``
 Specifies timeout in seconds. If not specified timeout is set to 300 seconds.

``-c TEST_CASE [TEST_CASE ...], --test_cases TEST_CASE [TEST_CASE ...]``
 Defines test cases to run. If not specified all available tests are run.

 **Example usage:**

 ``./test-bbdev.py -c validation``
  Runs validation test suite

 ``./test-bbdev.py -c latency throughput``
  Runs latency and throughput test suites

``-v TEST_VECTOR [TEST_VECTOR ...], --test_vector TEST_VECTOR [TEST_VECTOR ...]``
 Specifies paths to the test vector files. If not specified path is set based
 on "../.." concatenated with "*/app/test-bbdev/test_vectors/bbdev_null.data*"
 and indicates default data file.

 **Example usage:**

 ``./test-bbdev.py -v app/test-bbdev/test_vectors/turbo_dec_test1.data``
  Fills vector based on turbo_dec_test1.data file and runs all tests

 ``./test-bbdev.py -v turbo_dec_test1.data turbo_enc_test2.data``
  The bbdev test app is executed twice. First time vector is filled based on
  *turbo_dec_test1.data* file and second time based on
  *turb_enc_test2.data* file. For both executions all tests are run.

``-n NUM_OPS, --num_ops NUM_OPS``
 Specifies number of operations to process on device. If not specified num_ops
 is set to 32 operations.

``-l NUM_LCORES, --num_lcores NUM_LCORES``
 Specifies number of lcores to run. If not specified num_lcores is set
 according to value from RTE configuration (EAL coremask)

``-b BURST_SIZE [BURST_SIZE ...], --burst-size BURST_SIZE [BURST_SIZE ...]``
 Specifies operations enqueue/dequeue burst size. If not specified burst_size is
 set to 32. Maximum is 512.

``-t MAX_ITERS [MAX_ITERS ...], --iter_max MAX_ITERS [MAX_ITERS ...]``
 Specifies LDPC decoder operations maximum number of iterations for throughput
 and bler tests. If not specified iter_max is set to 6.

``-s SNR [SNR ...], --snr SNR [SNR ...]``
 Specifies for LDPC decoder operations the SNR in dB used when generating LLRs
 for bler tests. If not specified snr is set to 0 dB.

Test Cases
~~~~~~~~~~

There are 7 main test cases that can be executed using testbbdev tool:

* Sanity checks [-c unittest]
    - Performs sanity checks on BBDEV interface, validating basic functionality

* Validation tests [-c validation]
    - Performs full operation of enqueue and dequeue
    - Compares the dequeued data buffer with a expected values in the test
      vector (TV) being used
    - Fails if any dequeued value does not match the data in the TV

* Offload Cost measurement [-c offload]
    - Measures the CPU cycles consumed from the receipt of a user enqueue
      until it is put on the device queue
    - The test measures 4 metrics
        (a) *SW Enq Offload Cost*: Software only enqueue offload cost, the cycle
            counts and time (us) from the point the enqueue API is called until
            the point the operation is put on the accelerator queue.
        (b) *Acc Enq Offload Cost*: The cycle count and time (us) from the
            point the operation is put on the accelerator queue until the return
            from enqueue.
        (c) *SW Deq Offload Cost*: Software dequeue cost, the cycle counts and
            time (us) consumed to dequeue one operation.
        (d) *Empty Queue Enq Offload Cost*: The cycle count and time (us)
            consumed to dequeue from an empty queue.

* Latency measurement [-c latency]
    - Measures the time consumed from the first enqueue until the first
      appearance of a dequeued result
    - This measurement represents the full latency of a bbdev operation
      (encode or decode) to execute

* Poll-mode Throughput measurement [-c throughput]
    - Performs full operation of enqueue and dequeue
    - Executes in poll mode
    - Measures the achieved throughput on a subset or all available CPU cores
    - Dequeued data is not validated against expected values stored in TV
    - Results are printed in million operations per second and million bits
      per second

* BLER measurement [-c bler]
    - Performs full operation of enqueue and dequeue
    - Measures the achieved throughput on a subset or all available CPU cores
    - Computed BLER (Block Error Rate, ratio of blocks not decoded at a given
      SNR) in % based on the total number of operations.

* Interrupt-mode Throughput [-c interrupt]
    - Similar to Throughput test case, but using interrupts. No polling.


Parameter Globbing
~~~~~~~~~~~~~~~~~~

Thanks to the globbing functionality in python test-bbdev.py script allows to
run tests with different set of vector files without giving all of them explicitly.

**Example usage for 4G:**

.. code-block:: console

  ./test-bbdev.py -v app/test-bbdev/test_vectors/turbo_<enc/dec>_c<c>_k<k>_r<r>_e<e>_<extra-info>.data

It runs all tests with following vectors:

- ``bbdev_null.data``

- ``turbo_dec_c1_k6144_r0_e34560_sbd_negllr.data``

- ``turbo_enc_c1_k40_r0_e1196_rm.data``

- ``turbo_enc_c2_k5952_r0_e17868_crc24b.data``

- ``turbo_dec_c1_k40_r0_e17280_sbd_negllr.data``

- ``turbo_dec_c1_k6144_r0_e34560_sbd_posllr.data``

- ``turbo_enc_c1_k40_r0_e272_rm.data``

- ``turbo_enc_c3_k4800_r2_e14412_crc24b.data``

- ``turbo_dec_c1_k6144_r0_e10376_crc24b_sbd_negllr_high_snr.data``

- ``turbo_dec_c2_k3136_r0_e4920_sbd_negllr_crc24b.data``

- ``turbo_enc_c1_k6144_r0_e120_rm_rvidx.data``

- ``turbo_enc_c4_k4800_r2_e14412_crc24b.data``

- ``turbo_dec_c1_k6144_r0_e10376_crc24b_sbd_negllr_low_snr.data``

- ``turbo_dec_c2_k3136_r0_e4920_sbd_negllr.data``

- ``turbo_enc_c1_k6144_r0_e18444.data``

- ``turbo_dec_c1_k6144_r0_e34560_negllr.data``

- ``turbo_enc_c1_k40_r0_e1190_rm.data``

- ``turbo_enc_c1_k6144_r0_e18448_crc24a.data``

- ``turbo_dec_c1_k6144_r0_e34560_posllr.data``

- ``turbo_enc_c1_k40_r0_e1194_rm.data``

- ``turbo_enc_c1_k6144_r0_e32256_crc24b_rm.data``

.. code-block:: console

  ./test-bbdev.py -v app/test-bbdev/turbo_*_default.data

It runs all tests with "default" vectors.

* ``turbo_dec_default.data`` is a soft link to
  ``turbo_dec_c1_k6144_r0_e10376_crc24b_sbd_negllr_high_snr.data``

* ``turbo_enc_default.data`` is a soft link to
  ``turbo_enc_c1_k6144_r0_e32256_crc24b_rm.data``

* ``ldpc_dec_default.data`` is a soft link to
  ``ldpc_dec_v6563.data``

* ``ldpc_enc_default.data`` is a soft link to
  ``ldpc_enc_c1_k8148_r0_e9372_rm.data``

Running Tests
-------------

All default reference test-vectors are stored in the test_vector
directory below.
The prefix trivially defines which type of operation is included :
turbo_enc, turbo_dec, ldpc_enc, ldpc_dec.
The details of the configuration are captured in the file but some
vector name refer more explicitly processing specificity such as
'HARQ' when HARQ retransmission is used, 'loopback' when the data
is purely read/written for external DDR, lbrm when limited buffer
rate matching is expected, or crc_fail when a CRC failure is expected.
They are chosen to have a good coverage across sizes and processing
parameters while still keeping their number limited as part of sanity
regression.

Shortened tree of isg_cid-wireless_dpdk_ae with dpdk compiled and output
to the build directory:

::

 |-- app
     |-- test-bbdev
         |-- test_vectors

 |-- build
     |-- app
         |-- dpdk-test-bbdev

All bbdev devices
~~~~~~~~~~~~~~~~~

.. code-block:: console

  ./test-bbdev.py -p ../../build/app/dpdk-test-bbdev
  -v turbo_dec_default.data

It runs all available tests using the test vector filled based on
*turbo_dec_default.data* file.
By default number of operations to process on device is set to 32, timeout is
set to 300s and operations enqueue/dequeue burst size is set to 32.
Moreover a bbdev (*baseband_null*) device will be created.

baseband turbo_sw device
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

  ./test-bbdev.py -p ../../build/app/dpdk-test-bbdev
  -e="--vdev=baseband_turbo_sw" -t 120 -c validation
  -v ./test_vectors/* -n 64 -b 8 32

It runs **validation** test for each vector file that matches the given pattern.
Number of operations to process on device is set to 64 and operations timeout is
set to 120s and enqueue/dequeue burst size is set to 8 and to 32.
Moreover a bbdev (*baseband_turbo_sw*) device will be created.


bbdev null device
~~~~~~~~~~~~~~~~~

Executing bbdev null device with *bbdev_null.data* helps in measuring the
overhead introduced by the bbdev framework.

.. code-block:: console

  ./test-bbdev.py -e="--vdev=baseband_null0"
  -v ./test_vectors/bbdev_null.data

**Note:**

baseband_null device does not have to be defined explicitly as it is created by default.



Test Vector files
-----------------

Test Vector files contain the data which is used to set turbo decoder/encoder
parameters and buffers for validation purpose. New test vector files should be
stored in ``app/test-bbdev/test_vectors/`` directory. Detailed description of
the syntax of the test vector files is in the following section.


Basic principles for test vector files
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Line started with ``#`` is treated as a comment and is ignored.

If variable is a chain of values, values should be separated by a comma. If
assignment is split into several lines, each line (except the last one) has to
be ended with a comma.
There is no comma after last value in last line. Correct assignment should
look like the following:

.. parsed-literal::

 variable =
 value, value, value, value,
 value, value

In case where variable is a single value correct assignment looks like the
following:

.. parsed-literal::

 variable =
 value

Length of chain variable is calculated by parser. Can not be defined
explicitly.

Variable op_type has to be defined as a first variable in file. It specifies
what type of operations will be executed. For 4G decoder op_type has to be set to
``RTE_BBDEV_OP_TURBO_DEC`` and for 4G encoder to ``RTE_BBDEV_OP_TURBO_ENC``.

Full details of the meaning and valid values for the below fields are
documented in *rte_bbdev_op.h*


Turbo decoder test vectors template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For turbo decoder it has to be always set to ``RTE_BBDEV_OP_TURBO_DEC``

.. parsed-literal::

    op_type =
    RTE_BBDEV_OP_TURBO_DEC

Chain of uint32_t values. Note that it is possible to define more than one
input/output entries which will result in chaining two or more data structures
for *segmented Transport Blocks*

.. parsed-literal::

    input0 =
    0x00000000, 0x7f817f00, 0x7f7f8100, 0x817f8100, 0x81008100, 0x7f818100, 0x81817f00, 0x7f818100,
    0x81007f00, 0x7f818100, 0x817f8100, 0x81817f00, 0x81008100, 0x817f7f00, 0x7f7f8100, 0x81817f00

Chain of uint32_t values

.. parsed-literal::

    input1 =
    0x7f7f0000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000

Chain of uint32_t values

.. parsed-literal::

    input2 =
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000

Chain of uint32_t values

.. parsed-literal::

    hard_output0 =
    0xa7d6732e

Chain of uint32_t values

.. parsed-literal::

    hard_output1 =
    0xa61

Chain of uint32_t values

.. parsed-literal::

    soft_output0 =
    0x817f817f, 0x7f817f7f, 0x81818181, 0x817f7f81, 0x7f818181, 0x8181817f, 0x817f817f, 0x8181817f

Chain of uint32_t values

.. parsed-literal::

    soft_output1 =
    0x817f7f81, 0x7f7f7f81, 0x7f7f8181

uint32_t value

.. parsed-literal::

    e =
    44

uint16_t value

.. parsed-literal::

    k =
    40

uint8_t value

.. parsed-literal::

    rv_index =
    0

uint8_t value

.. parsed-literal::

    iter_max =
    8

uint8_t value

.. parsed-literal::

    iter_min =
    4

uint8_t value

.. parsed-literal::

    expected_iter_count =
    8

uint8_t value

.. parsed-literal::

    ext_scale =
    15

uint8_t value

.. parsed-literal::

    num_maps =
    0

Chain of flags for LDPC decoder operation based on the rte_bbdev_op_td_flag_bitmasks:

Example:

    .. parsed-literal::

        op_flags =
        RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE, RTE_BBDEV_TURBO_EQUALIZER,
        RTE_BBDEV_TURBO_SOFT_OUTPUT

Chain of operation statuses that are expected after operation is performed.
Following statuses can be used:

- ``DMA``

- ``FCW``

- ``CRC``

- ``OK``

``OK`` means no errors are expected. Cannot be used with other values.

.. parsed-literal::

    expected_status =
    FCW, CRC


Turbo encoder test vectors template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For turbo encoder it has to be always set to ``RTE_BBDEV_OP_TURBO_ENC``

.. parsed-literal::

    op_type =
    RTE_BBDEV_OP_TURBO_ENC

Chain of uint32_t values

.. parsed-literal::

    input0 =
    0x11d2bcac, 0x4d

Chain of uint32_t values

.. parsed-literal::

    output0 =
    0xd2399179, 0x640eb999, 0x2cbaf577, 0xaf224ae2, 0x9d139927, 0xe6909b29,
    0xa25b7f47, 0x2aa224ce, 0x79f2

uint32_t value

.. parsed-literal::

    e =
    272

uint16_t value

.. parsed-literal::

    k =
    40

uint16_t value

.. parsed-literal::

    ncb =
    192

uint8_t value

.. parsed-literal::

    rv_index =
    0

Chain of flags for LDPC decoder operation based on the rte_bbdev_op_te_flag_bitmasks:

``RTE_BBDEV_TURBO_ENC_SCATTER_GATHER`` is used to indicate the parser to
force the input data to be memory split and formed as a segmented mbuf.


.. parsed-literal::

    op_flags =
    RTE_BBDEV_TURBO_RATE_MATCH

Chain of operation statuses that are expected after operation is performed.
Following statuses can be used:

- ``DMA``

- ``FCW``

- ``OK``

``OK`` means no errors are expected. Cannot be used with other values.

.. parsed-literal::

    expected_status =
    OK

LDPC decoder test vectors template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For LDPC decoder it has to be always set to ``RTE_BBDEV_OP_LDPC_DEC``

.. parsed-literal::

    op_type =
    RTE_BBDEV_OP_LDPC_DEC

Chain of uint32_t values. Note that it is possible to define more than one
input/output entries which will result in chaining two or more data structures
for *segmented Transport Blocks*

.. parsed-literal::

    input0 =
    0x00000000, 0x7f817f00, 0x7f7f8100, 0x817f8100, 0x81008100, 0x7f818100, 0x81817f00, 0x7f818100,
    0x81007f00, 0x7f818100, 0x817f8100, 0x81817f00, 0x81008100, 0x817f7f00, 0x7f7f8100, 0x81817f00

.. parsed-literal::

    output0 =
    0xa7d6732e

uint8_t value

.. parsed-literal::

    basegraph=
    1

uint16_t value

.. parsed-literal::

    z_c=
    224

uint16_t value

.. parsed-literal::

    n_cb=
    14784

uint8_t value

.. parsed-literal::

    q_m=
    1

uint16_t value

.. parsed-literal::

    n_filler=
    40

uint32_t value

.. parsed-literal::

    e=
    13072

uint8_t value

.. parsed-literal::

    rv_index=
    2

uint8_t value

.. parsed-literal::
    code_block_mode=
    1

uint8_t value

.. parsed-literal::

    iter_max=
    20

uint8_t value

.. parsed-literal::

    expected_iter_count=
    8


Chain of flags for LDPC decoder operation based on the rte_bbdev_op_ldpcdec_flag_bitmasks:

Example:

    .. parsed-literal::

        op_flags =
        RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE, RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE,
        RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE, RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION

Chain of operation statuses that are expected after operation is performed.
Following statuses can be used:

- ``OK`` : No error reported.

- ``SYN`` : LDPC syndrome parity check is failing.

- ``CRC`` : CRC parity check is failing when CRC check operation is included.

- ``SYNCRC`` : Both CRC and LDPC syndromes parity checks are failing.

``OK`` means no errors are expected. Cannot be used with other values.

.. parsed-literal::

    expected_status =
    CRC


LDPC encoder test vectors template
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For turbo encoder it has to be always set to ``RTE_BBDEV_OP_LDPC_ENC``

.. parsed-literal::

    op_type =
    RTE_BBDEV_OP_LDPC_ENC

Chain of uint32_t values

.. parsed-literal::

    input0 =
    0x11d2bcac, 0x4d

Chain of uint32_t values

.. parsed-literal::

    output0 =
    0xd2399179, 0x640eb999, 0x2cbaf577, 0xaf224ae2, 0x9d139927, 0xe6909b29,
    0xa25b7f47, 0x2aa224ce, 0x79f2


uint8_t value

.. parsed-literal::

    basegraph=
    1

uint16_t value

.. parsed-literal::

    z_c=
    52

uint16_t value

.. parsed-literal::

    n_cb=
    3432

uint8_t value

.. parsed-literal::

    q_m=
    6

uint16_t value

.. parsed-literal::

    n_filler=
    0

uint32_t value

.. parsed-literal::

    e =
    1380

uint8_t value

.. parsed-literal::

    rv_index =
    1

uint8_t value

.. parsed-literal::

    code_block_mode =
    1


Chain of flags for LDPC encoder operation based on the
rte_bbdev_op_ldpcenc_flag_bitmasks:

.. parsed-literal::

    op_flags =
    RTE_BBDEV_LDPC_RATE_MATCH

Chain of operation statuses that are expected after operation is performed.
Following statuses can be used:

- ``DMA``

- ``FCW``

- ``OK``

``OK`` means no errors are expected. Cannot be used with other values.

.. parsed-literal::

    expected_status =
    OK
