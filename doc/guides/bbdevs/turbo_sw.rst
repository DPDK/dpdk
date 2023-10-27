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

These libraries are available through this `link
<https://github.com/intel/FlexRAN-FEC-SDK-Modules/tree/Branch_FEC_SDK_23.07>`_.

After download is complete, the user needs to unpack and compile on their
system before building DPDK.

To get the FlexRAN FEC SDK user manual, extract this `doxygen
<https://github.com/intel/FlexRAN-FEC-SDK-Modules/blob/Branch_FEC_SDK_23.07/doc/doxygen/html.zip>`_.

The following table maps DPDK versions with past FlexRAN SDK releases:

.. _table_flexran_releases:

.. table:: DPDK and FlexRAN FEC SDK releases compliance

   =====================  ============================
   DPDK version           FlexRAN FEC SDK release
   =====================  ============================
   19.08 to 22.07         19.04
   22.11+                 22.11
   23.11+                 FEC_SDK_23.07
   =====================  ============================

FlexRAN SDK Installation
~~~~~~~~~~~~~~~~~~~~~~~~

Note that the installation of these libraries is optional.

The following are pre-requisites for building FlexRAN SDK Libraries:
 (a) An AVX512 supporting machine.
 (b) Ubuntu Linux release 22.04 operating system is advised.
 (c) Intel ICX 2023.0.0 compiler or more recent and related libraries.
     ICX is available `here <https://docs.o-ran-sc.org/projects/o-ran-sc-o-du-phy/en/latest/build_prerequisite.html#download-and-install-oneapi>`_.
 (d) `FlexRAN SDK Modules <https://github.com/intel/FlexRAN-FEC-SDK-Modules/tree/Branch_FEC_SDK_23.07>`_.
 (e) CMake 3.9.2 (Minimum 2.8.12)
 (f) Google Test 1.7.0 (Required to run the verification and compute performance tests)
 (g) Math Kernel Library 18.0 (Required by some functions in SDK)

The following instructions should be followed in this exact order:

#. Clone the SDK (folder name needs to end in 'sdk')

    .. code-block:: console

        git clone -b Branch_FEC_SDK_23.07 https://github.com/intel/FlexRAN-FEC-SDK-Modules.git flexran_sdk

#. Set the environment variables:

    .. code-block:: console

        source <path-to-workspace>/export_settings.sh -o -avx512

#. Generate makefiles based on system configuration:

    .. code-block:: console

        cd <path-to-workspace>
        ./create-makefiles-linux.sh

#. A build folder is generated in this form ``build-<ISA>-<CC>``, enter that
   folder and install:

    .. code-block:: console

        cd <path-to-workspace>/build-${WIRELESS_SDK_TARGET_ISA}-${WIRELESS_SDK_TOOLCHAIN}/
        make -j$(nproc) && make install

DPDK Initialization
~~~~~~~~~~~~~~~~~~~

In order to enable this virtual bbdev PMD, the user may:

* Build the ``FLEXRAN SDK`` libraries (explained in Installation section).

* Export the environmental variables ``FLEXRAN_SDK`` to the path where the
  FlexRAN SDK libraries were installed. And ``DIR_WIRELESS_SDK`` to the path
  where the libraries were extracted.

* Point pkgconfig towards these libraries so that they can be automatically found by meson.
  If not, DPDK will still compile but the related functionality would be stubbed out.

Example:

.. code-block:: console

    export FLEXRAN_SDK=<path-to-workspace>/build-${WIRELESS_SDK_TARGET_ISA}-${WIRELESS_SDK_TOOLCHAIN}/install
    export DIR_WIRELESS_SDK=<path-to-workspace>/build-${WIRELESS_SDK_TARGET_ISA}-${WIRELESS_SDK_TOOLCHAIN}
    export PKG_CONFIG_PATH=${DIR_WIRELESS_SDK}/pkgcfg:${PKG_CONFIG_PATH}
    cd build
    meson configure

* For AVX512 machines with SDK libraries installed then both 4G and 5G can be enabled for full real time FEC capability.
  For AVX2 machines it is possible to only enable the 4G libraries and the PMD capabilities will be limited to 4G FEC.
  If no library is present then the PMD will still build but its capabilities will be limited accordingly.

SW Turbo PMD Usage
~~~~~~~~~~~~~~~~~~

To use the PMD in an application, user must:

- Call ``rte_vdev_init("baseband_turbo_sw")`` within the application.

- Use ``--vdev="baseband_turbo_sw"`` in the EAL options, which will call ``rte_vdev_init()`` internally.

The following parameters (all optional) can be provided in the previous two calls:

* ``socket_id``: Specify the socket where the memory for the device is going to be allocated
  (by default, *socket_id* will be the socket where the core that is creating the PMD is running on).

* ``max_nb_queues``: Specify the maximum number of queues in the device (default is ``RTE_MAX_LCORE``).

Example:

.. code-block:: console

    ./test-bbdev.py -e="--vdev=baseband_turbo_sw,socket_id=0,max_nb_queues=8" \
    -c validation -v ./turbo_*_default.data
