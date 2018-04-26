..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation

SW Turbo Poll Mode Driver
=========================

The SW Turbo PMD (**baseband_turbo_sw**) provides a poll mode bbdev driver that utilizes
Intel optimized libraries for LTE Layer 1 workloads acceleration. This PMD
supports the functions: Turbo FEC, Rate Matching and CRC functions.

Features
--------

SW Turbo PMD has support for the following capabilities:

For the encode operation:

* ``RTE_BBDEV_TURBO_CRC_24A_ATTACH``
* ``RTE_BBDEV_TURBO_CRC_24B_ATTACH``
* ``RTE_BBDEV_TURBO_RATE_MATCH``
* ``RTE_BBDEV_TURBO_RV_INDEX_BYPASS``

For the decode operation:

* ``RTE_BBDEV_TURBO_SUBBLOCK_DEINTERLEAVE``
* ``RTE_BBDEV_TURBO_CRC_TYPE_24B``
* ``RTE_BBDEV_TURBO_POS_LLR_1_BIT_IN``
* ``RTE_BBDEV_TURBO_NEG_LLR_1_BIT_IN``
* ``RTE_BBDEV_TURBO_DEC_TB_CRC_24B_KEEP``
* ``RTE_BBDEV_TURBO_EARLY_TERMINATION``


Limitations
-----------

* In-place operations for Turbo encode and decode are not supported

Installation
------------

FlexRAN SDK Download
~~~~~~~~~~~~~~~~~~~~

To build DPDK with the *baseband_turbo_sw* PMD the user is required to download
the export controlled ``FlexRAN SDK`` Libraries. An account at `Intel Resource
Design Center <https://www.intel.com/content/www/us/en/design/resource-design-center.html>`_
needs to be registered.

Once registered, the user needs to log in, and look for
*Intel FlexRAN Software Release Package 1_4_0* to download or directly through
this `link <https://cdrdv2.intel.com/v1/dl/getContent/576288>`_.

After download is complete, the user needs to unpack and compile on their
system before building DPDK.

The following table maps DPDK versions with past FlexRAN SDK releases:

.. _table_flexran_releases:

.. table:: DPDK and FlexRAN SDK releases compliance

   =====================  ============================
   DPDK version           FlexRAN SDK release
   =====================  ============================
   18.02                  1.3.0
   18.05                  1.4.0
   =====================  ============================

FlexRAN SDK Installation
~~~~~~~~~~~~~~~~~~~~~~~~

The following are pre-requisites for building FlexRAN SDK Libraries:
 (a) An AVX2 supporting machine
 (b) Windriver TS 2 or CentOS 7 operating systems
 (c) Intel ICC 17.0.3 compiler installed

The following instructions should be followed in this exact order:

#. Set the environment variables:

    .. code-block:: console

        source <path-to-icc-compiler-install-folder>/linux/bin/compilervars.sh intel64 -platform linux


#. Extract the ``576288-576288-intel-swa-sw-flexran-release-package-r1-4-0.zip``
   package, then run the SDK extractor script and accept the license:

    .. code-block:: console

        cd <path-to-workspace>/FlexRAN-1.4.0/
        ./SDK-R1.4.0.sh

#. Generate makefiles based on system configuration:

    .. code-block:: console

        cd <path-to-workspace>/FlexRAN-1.4.0/SDK-R1.4.0/sdk/
        ./create-makefiles-linux.sh

#. If build as a shared library is required, then add ``"-fPIC"`` option to file
   ``<path-to-workspace>/FlexRAN-1.4.0/SDK-R1.4.0/sdk/cmake/intel-compile-options.cmake``
   as follows:

    .. code-block:: console

        add_compile_options("-fPIC")

#. A build folder is generated in this form ``build-<ISA>-<CC>``, enter that
   folder and install:

    .. code-block:: console

        cd build-avx2-icc/
        make install


Initialization
--------------

In order to enable this virtual bbdev PMD, the user must:

* Build the ``FLEXRAN SDK`` libraries (explained in Installation section).

* Export the environmental variables ``FLEXRAN_SDK`` to the path where the
  FlexRAN SDK libraries were installed. And ``DIR_WIRELESS_SDK`` to the path
  where the libraries were extracted.

Example:

.. code-block:: console

    export FLEXRAN_SDK=<path-to-workspace>/FlexRAN-1.4.0/SDK-R1.4.0/sdk/build-avx2-icc/install
    export DIR_WIRELESS_SDK=<path-to-workspace>/FlexRAN-1.4.0/SDK-R1.4.0/sdk/


* Set ``CONFIG_RTE_LIBRTE_PMD_BBDEV_TURBO_SW=y`` in DPDK common configuration
  file ``config/common_base``.

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
    -c validation -v ./test_vectors/bbdev_vector_t?_default.data
