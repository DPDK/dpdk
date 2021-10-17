..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2021 NXP

NXP LA12xx Poll Mode Driver
===========================

The BBDEV LA12xx poll mode driver (PMD) supports an implementation for
offloading High Phy processing functions like LDPC Encode / Decode 5GNR wireless
acceleration function, using PCI based LA12xx Software defined radio.

More information can be found at `NXP Official Website
<https://www.nxp.com/products/processors-and-microcontrollers/arm-processors/layerscape-processors/layerscape-access-la1200-programmable-baseband-processor:LA1200>`_.

Features
--------

LA12xx PMD supports the following features:

- Maximum of 8 LDPC decode (UL) queues
- Maximum of 8 LDPC encode (DL) queues
- PCIe Gen-3 x8 Interface

Installation
------------

Section 3 of the DPDK manual provides instructions on installing and compiling DPDK.

DPDK requires hugepages to be configured as detailed in section 2 of the DPDK manual.

Initialization
--------------

The device can be listed on the host console with:


Use the following lspci command to get the multiple LA12xx processor ids. The
device ID of the LA12xx baseband processor is "1c30".

.. code-block:: console

  sudo lspci -nn

...
0001:01:00.0 Power PC [0b20]: Freescale Semiconductor Inc Device [1957:1c30] (
rev 10)
...
0002:01:00.0 Power PC [0b20]: Freescale Semiconductor Inc Device [1957:1c30] (
rev 10)


Prerequisites
-------------

Currently supported by DPDK:

- NXP LA1224 BSP **1.0+**.
- NXP LA1224 PCIe Modem card connected to ARM host.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

* Use dev arg option ``modem=0`` to identify the modem instance for a given
  device. This is required only if more than 1 modem cards are attached to host.
  this is optional and the default value is 0.
  e.g. ``--vdev=baseband_la12xx,modem=0``

* Use dev arg option ``max_nb_queues=x`` to specify the maximum number of queues
  to be used for communication with offload device i.e. modem. default is 16.
  e.g. ``--vdev=baseband_la12xx,max_nb_queues=4``

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_bbdev_application <EAL args> --log-level=la12xx:<level>

Using ``bb.la12xx`` as log matching criteria, all Baseband PMD logs can be
enabled which are lower than logging ``level``.

Test Application
----------------

BBDEV provides a test application, ``test-bbdev.py`` and range of test data for testing
the functionality of LA12xx for FEC encode and decode, depending on the device
capabilities. The test application is located under app->test-bbdev folder and has the
following options:

.. code-block:: console

  "-p", "--testapp-path": specifies path to the bbdev test app.
  "-e", "--eal-params"	: EAL arguments which are passed to the test app.
  "-t", "--timeout"	: Timeout in seconds (default=300).
  "-c", "--test-cases"	: Defines test cases to run. Run all if not specified.
  "-v", "--test-vector"	: Test vector path (default=dpdk_path+/app/test-bbdev/test_vectors/bbdev_null.data).
  "-n", "--num-ops"	: Number of operations to process on device (default=32).
  "-b", "--burst-size"	: Operations enqueue/dequeue burst size (default=32).
  "-s", "--snr"		: SNR in dB used when generating LLRs for bler tests.
  "-s", "--iter_max"	: Number of iterations for LDPC decoder.
  "-l", "--num-lcores"	: Number of lcores to run (default=16).
  "-i", "--init-device" : Initialise PF device with default values.


To execute the test application tool using simple decode or encode data,
type one of the following:

.. code-block:: console

  ./test-bbdev.py -e="--vdev=baseband_la12xx,socket_id=0,max_nb_queues=8" -c validation -n 64 -b 1 -v ./ldpc_dec_default.data
  ./test-bbdev.py -e="--vdev=baseband_la12xx,socket_id=0,max_nb_queues=8" -c validation -n 64 -b 1 -v ./ldpc_enc_default.data

The test application ``test-bbdev.py``, supports the ability to configure the PF device with
a default set of values, if the "-i" or "- -init-device" option is included. The default values
are defined in test_bbdev_perf.c.


Test Vectors
~~~~~~~~~~~~

In addition to the simple LDPC decoder and LDPC encoder tests, bbdev also provides
a range of additional tests under the test_vectors folder, which may be useful. The results
of these tests will depend on the LA12xx FEC capabilities which may cause some
testcases to be skipped, but no failure should be reported.
