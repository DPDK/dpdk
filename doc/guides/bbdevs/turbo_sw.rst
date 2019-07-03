..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation

SW Turbo Poll Mode Driver
=========================

The SW Turbo PMD (**baseband_turbo_sw**) provides a software only poll mode bbdev
driver that can optionally utilize Intel optimized libraries for LTE and 5GNR
Layer 1 workloads acceleration.

Note that the driver can also be built without any dependency with reduced
functionality for maintenance purpose.

To enable linking to the SDK libraries see detailed installation section below.
Two flags can be enabled depending on whether the target machine can support
AVX2 and AVX512 instructions sets and the related SDK libraries for vectorized
signal processing functions are installed :
- CONFIG_RTE_BBDEV_SDK_AVX2
- CONFIG_RTE_BBDEV_SDK_AVX512
By default these 2 flags are disabled by default. For AVX2 machine and SDK
library installed then the first flag can be enabled. For AVX512 machine and
SDK library installed then both flags can be enabled for full real time capability.

This PMD supports the functions: FEC, Rate Matching and CRC functions detailed
in the Features section.

Features
--------

SW Turbo PMD can support for the following capabilities when the SDK libraries
are used:

For the LTE encode operation:

* ``RTE_BBDEV_TURBO_CRC_24A_ATTACH``
* ``RTE_BBDEV_TURBO_CRC_24B_ATTACH``
* ``RTE_BBDEV_TURBO_RATE_MATCH``
* ``RTE_BBDEV_TURBO_RV_INDEX_BYPASS``

For the LTE decode operation:

* ``RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE``
* ``RTE_BBDEV_TURBO_CRC_TYPE_24B``
* ``RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN``
* ``RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN``
* ``RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP``
* ``RTE_BBDEV_TURBO_EARLY_TERMINATION``

For the 5G NR LDPC encode operation:

* ``RTE_BBDEV_LDPC_RATE_MATCH``
* ``RTE_BBDEV_LDPC_CRC_24A_ATTACH``
* ``RTE_BBDEV_LDPC_CRC_24B_ATTACH``

For the 5G NR LDPC decode operation:

* ``RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK``
* ``RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK``
* ``RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP``
* ``RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE``
* ``RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE``
* ``RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE``

Limitations
-----------

* In-place operations for encode and decode are not supported

Installation
------------

FlexRAN SDK Download
~~~~~~~~~~~~~~~~~~~~

As an option it is possible to link this driver with FleXRAN SDK libraries
which can enable real time signal processing using AVX instructions.

These libraries are available through this `link <https://software.intel.com/en-us/articles/flexran-lte-and-5g-nr-fec-software-development-kit-modules>`_.

After download is complete, the user needs to unpack and compile on their
system before building DPDK.

The following table maps DPDK versions with past FlexRAN SDK releases:

.. _table_flexran_releases:

.. table:: DPDK and FlexRAN FEC SDK releases compliance

   =====================  ============================
   DPDK version           FlexRAN FEC SDK release
   =====================  ============================
   19.08                  19.04
   =====================  ============================

FlexRAN SDK Installation
~~~~~~~~~~~~~~~~~~~~~~~~

Note that the installation of these libraries is optional.

The following are pre-requisites for building FlexRAN SDK Libraries:
 (a) An AVX2 or AVX512 supporting machine
 (b) CentOS Linux release 7.2.1511 (Core) operating system is advised
 (c) Intel ICC 18.0.1 20171018 compiler or more recent and related libraries
     ICC is `available with a free community license <https://software.intel.com/en-us/system-studio/choose-download#technical>`_.

The following instructions should be followed in this exact order:

#. Set the environment variables:

    .. code-block:: console

        source <path-to-icc-compiler-install-folder>/linux/bin/compilervars.sh intel64 -platform linux

#. Run the SDK extractor script and accept the license:

    .. code-block:: console

        cd <path-to-workspace>
        ./FlexRAN-FEC-SDK-19-04.sh

#. Generate makefiles based on system configuration:

    .. code-block:: console

        cd <path-to-workspace>/FlexRAN-FEC-SDK-19-04/sdk/
        ./create-makefiles-linux.sh

#. A build folder is generated in this form ``build-<ISA>-<CC>``, enter that
   folder and install:

    .. code-block:: console

        cd build-avx512-icc/
        make && make install

Initialization
--------------

In order to enable this virtual bbdev PMD, the user may:

* Build the ``FLEXRAN SDK`` libraries (explained in Installation section).

* Export the environmental variables ``FLEXRAN_SDK`` to the path where the
  FlexRAN SDK libraries were installed. And ``DIR_WIRELESS_SDK`` to the path
  where the libraries were extracted.

Example:

.. code-block:: console

    export FLEXRAN_SDK=<path-to-workspace>/FlexRAN-FEC-SDK-19-04/sdk/build-avx2-icc/install
    export DIR_WIRELESS_SDK=<path-to-workspace>/FlexRAN-FEC-SDK-19-04/sdk/build-avx2-icc/

* Set ``CONFIG_RTE_BBDEV_SDK_AVX2=y`` and ``CONFIG_RTE_BBDEV_SDK_AVX512=y``
  in DPDK common configuration file ``config/common_base`` to be able to use
  the SDK libraries as mentioned above.
  For AVX2 machine it is possible to only enable CONFIG_RTE_BBDEV_SDK_AVX2
  for limited 4G functionality.
  If no flag are set the PMD driver will still build but its capabilities
  will be limited accordingly.

To use the PMD in an application, user must:

- Call ``rte_vdev_init("baseband_turbo_sw")`` within the application.

- Use ``--vdev="baseband_turbo_sw"`` in the EAL options, which will call ``rte_vdev_init()`` internally.

The following parameters (all optional) can be provided in the previous two calls:

* ``socket_id``: Specify the socket where the memory for the device is going to be allocated
  (by default, *socket_id* will be the socket where the core that is creating the PMD is running on).

* ``max_nb_queues``: Specify the maximum number of queues in the device (default is ``RTE_MAX_LCORE``).

Example:
~~~~~~~~

.. code-block:: console

    ./test-bbdev.py -e="--vdev=baseband_turbo_sw,socket_id=0,max_nb_queues=8" \
    -c validation -v ./turbo_*_default.data
