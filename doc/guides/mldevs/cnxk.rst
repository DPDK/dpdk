..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2022 Marvell.

Marvell cnxk Machine Learning Poll Mode Driver
==============================================

The cnxk ML poll mode driver provides support for offloading
Machine Learning inference operations to Machine Learning accelerator units
on the **Marvell OCTEON cnxk** SoC family.

The cnxk ML PMD code is organized into multiple files with all file names
starting with cn10k, providing support for CN106XX and CN106XXS.

More information about OCTEON cnxk SoCs may be obtained from `<https://www.marvell.com>`_

Supported OCTEON cnxk SoCs
--------------------------

- CN106XX
- CN106XXS

Features
--------

The OCTEON cnxk ML PMD provides support for the following set of operations:

Slow-path device and ML model handling:

* Device probing, configuration and close
* Device start and stop
* Model loading and unloading
* Model start and stop
* Data quantization and dequantization

Fast-path Inference:

* Inference execution
* Error handling


Compilation Prerequisites
-------------------------

This driver requires external libraries
to optionally enable support for models compiled using Apache TVM framework.
The following dependencies are not part of DPDK and must be installed separately:

Jansson
~~~~~~~

This library enables support to parse and read JSON files.

DLPack
~~~~~~

This library provides headers for open in-memory tensor structures.

.. note::

   DPDK CNXK ML driver requires DLPack version 0.7

.. code-block:: console

   git clone https://github.com/dmlc/dlpack.git
   cd dlpack
   git checkout v0.7 -b v0.7
   cmake -S ./ -B build \
      -DCMAKE_INSTALL_PREFIX=<install_prefix> \
      -DBUILD_MOCK=OFF
   make -C build
   make -C build install

When cross-compiling, compiler must be provided to CMake:

.. code-block:: console

   -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
   -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++

DMLC
~~~~

  This is a common bricks library for building scalable
  and portable distributed machine learning.

.. code-block:: console

   git clone https://github.com/dmlc/dmlc-core.git
   cd dmlc-core
   git checkout main
   cmake -S ./ -B build \
      -DCMAKE_INSTALL_PREFIX=<install_prefix> \
      -DCMAKE_C_FLAGS="-fpermissive" \
      -DCMAKE_CXX_FLAGS="-fpermissive" \
      -DUSE_OPENMP=OFF
    make -C build
    make -C build install

When cross-compiling, compiler must be provided to CMake:

.. code-block:: console

   -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
   -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++

TVM
~~~

Apache TVM provides a runtime libraries used to execute models
on CPU cores or hardware accelerators.

.. note::

   DPDK CNXK ML driver requires TVM version 0.10.0

.. code-block:: console

   git clone https://github.com/apache/tvm.git
   cd tvm
   git checkout v0.11.0 -b v0.11.0
   git submodule update --init
   cmake -S ./ -B build \
      -DCMAKE_INSTALL_PREFIX=<install_prefix> \
      -DBUILD_STATIC_RUNTIME=OFF
   make -C build
   make -C build install

When cross-compiling, more options must be provided to CMake:

.. code-block:: console

   -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
   -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
   -DMACHINE_NAME=aarch64-linux-gnu \
   -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
   -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY

TVMDP
~~~~~

  Marvell's `TVM Dataplane Library <https://github.com/MarvellEmbeddedProcessors/tvmdp>`_
  works as an interface between TVM runtime and DPDK drivers.
  TVMDP library provides a simplified C interface
  for TVM's runtime based on C++.

.. note::

   TVMDP library is dependent on TVM, dlpack, jansson and dmlc-core libraries.

.. code-block:: console

   git clone https://github.com/MarvellEmbeddedProcessors/tvmdp.git
   cd tvmdp
   git checkout main
   cmake -S ./ -B build \
      -DCMAKE_INSTALL_PREFIX=<install_prefix> \
      -DBUILD_SHARED_LIBS=ON
   make -C build
   make -C build install

When cross-compiling, more options must be provided to CMake:

.. code-block:: console

   -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
   -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
   -DCMAKE_FIND_ROOT_PATH=<install_prefix>

libarchive
~~~~~~~~~~

Apache TVM framework generates compiled models as tar archives.
This library enables support to decompress and read archive files
in tar, xz and other formats.


Installation
------------

The OCTEON cnxk ML PMD may be compiled natively on an OCTEON cnxk platform
or cross-compiled on an x86 platform.

In order for Meson to find the dependencies above during the configure stage,
it is required to update environment variables as below:

.. code-block:: console

   CMAKE_PREFIX_PATH='<install_prefix>/lib/cmake/tvm:<install_prefix>/lib/cmake/dlpack:<install_prefix>/lib/cmake/dmlc'
   PKG_CONFIG_PATH='<install_prefix>/lib/pkgconfig'

Refer to :doc:`../platform/cnxk` for instructions to build your DPDK application.


Initialization
--------------

List the ML PF devices available on cn10k platform:

.. code-block:: console

   lspci -d:a092

``a092`` is the ML device PF id. You should see output similar to:

.. code-block:: console

   0000:00:10.0 System peripheral: Cavium, Inc. Device a092

Bind the ML PF device to the vfio_pci driver:

.. code-block:: console

   cd <dpdk directory>
   usertools/dpdk-devbind.py -u 0000:00:10.0
   usertools/dpdk-devbind.py -b vfio-pci 0000:00:10.0


VDEV support
------------

On platforms which don't support ML hardware acceleration through PCI device,
the Marvell ML CNXK PMD can execute inference operations on a vdev
with the ML models compiled using Apache TVM framework.

VDEV can be enabled by passing the EAL arguments

.. code-block:: console

   --vdev ml_mvtvm

VDEV can also be used on platforms with ML HW accelerator.
However to use vdev in this case, the PCI device has to be unbound.
When PCI device is bound, creation of vdev is skipped.


Runtime Config Options
----------------------

**Firmware file path** (default ``/lib/firmware/mlip-fw.bin``)

  Path to the firmware binary to be loaded during device configuration.
  The parameter ``fw_path`` can be used by the user
  to load ML firmware from a custom path.

  This option is supported only on PCI HW accelerator.

  For example::

     -a 0000:00:10.0,fw_path="/home/user/ml_fw.bin"

  With the above configuration, driver loads the firmware from the path
  ``/home/user/ml_fw.bin``.


**Enable DPE warnings** (default ``1``)

  ML firmware can be configured during load to handle the DPE errors reported
  by ML inference engine.
  When enabled, firmware would mask the DPE non-fatal hardware errors as warnings.
  The parameter ``enable_dpe_warnings`` is used fo this configuration.

  This option is supported only on PCI HW accelerator.

  For example::

     -a 0000:00:10.0,enable_dpe_warnings=0

  With the above configuration, DPE non-fatal errors reported by HW
  are considered as errors.


**Model data caching** (default ``1``)

  Enable caching model data on ML ACC cores.
  Enabling this option executes a dummy inference request
  in synchronous mode during model start stage.
  Caching of model data improves the inferencing throughput / latency for the model.
  The parameter ``cache_model_data`` is used to enable data caching.

  This option is supported on PCI HW accelerator and vdev.

  For example::

     -a 0000:00:10.0,cache_model_data=0

  With the above configuration, model data caching is disabled on HW accelerator.

  For example::

     --vdev ml_mvtvm,cache_model_data=0

  With the above configuration, model data caching is disabled on vdev.


**OCM allocation mode** (default ``lowest``)

  Option to specify the method to be used while allocating OCM memory
  for a model during model start.
  Two modes are supported by the driver.
  The parameter ``ocm_alloc_mode`` is used to select the OCM allocation mode.

  ``lowest``
    Allocate OCM for the model from first available free slot.
    Search for the free slot is done starting from the lowest tile ID and lowest page ID.
  ``largest``
    Allocate OCM for the model from the slot with largest amount of free space.

  This option is supported only on PCI HW accelerator.

  For example::

     -a 0000:00:10.0,ocm_alloc_mode=lowest

  With the above configuration, OCM allocation for the model would be done
  from the first available free slot / from the lowest possible tile ID.

**OCM page size** (default ``16384``)

  Option to specify the page size in bytes to be used for OCM management.
  Available OCM is split into multiple pages of specified sizes
  and the pages are allocated to the models.
  The parameter ``ocm_page_size`` is used to specify the page size to be used.

  Supported page sizes by the driver are 1 KB, 2 KB, 4 KB, 8 KB and 16 KB.
  Default page size is 16 KB.

  This option is supported only on PCI HW accelerator.

  For example::

     -a 0000:00:10.0,ocm_page_size=8192

  With the above configuration, page size of OCM is set to 8192 bytes / 8 KB.


**Enable hardware queue lock** (default ``0``)

  Option to select the job request enqueue function to use
  to queue the requests to hardware queue.
  The parameter ``hw_queue_lock`` is used to select the enqueue function.

  ``0``
    Disable (default), use lock-free version of hardware enqueue function
    for job queuing in enqueue burst operation.
    To avoid race condition in request queuing to hardware,
    disabling ``hw_queue_lock`` restricts the number of queue-pairs
    supported by cnxk driver to 1.
  ``1``
    Enable, use spin-lock version of hardware enqueue function for job queuing.
    Enabling spinlock version would disable restrictions on the number of queue-pairs
    that can be supported by the driver.

  This option is supported only on PCI HW accelerator.

  For example::

     -a 0000:00:10.0,hw_queue_lock=1

  With the above configuration, spinlock version of hardware enqueue function is used
  in the fast path enqueue burst operation.

**Maximum queue pairs** (default ``1``)

  VDEV supports additional EAL arguments to configure the maximum number
  of queue-pairs on the ML device through the option ``max_qps``.

  This option is supported only on vdev.

  For example::

     --vdev ml_mvtvm,max_qps=4

  With the above configuration, 4 queue-pairs are created on the vdev.


Debugging Options
-----------------

.. _table_octeon_cnxk_ml_debug_options:

.. table:: OCTEON cnxk ML PMD debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | ML         | --log-level='pmd\.ml\.cnxk,8'                         |
   +---+------------+-------------------------------------------------------+


Extended stats
--------------

Marvell cnxk ML PMD supports reporting the device and model extended statistics.

PMD supports the below list of 4 device extended stats.

.. _table_octeon_cnxk_ml_device_xstats_names:

.. table:: OCTEON cnxk ML PMD device xstats names

   +---+---------------------+----------------------------------------------+
   | # | Type                | Description                                  |
   +===+=====================+==============================================+
   | 1 | nb_models_loaded    | Number of models loaded                      |
   +---+---------------------+----------------------------------------------+
   | 2 | nb_models_unloaded  | Number of models unloaded                    |
   +---+---------------------+----------------------------------------------+
   | 3 | nb_models_started   | Number of models started                     |
   +---+---------------------+----------------------------------------------+
   | 4 | nb_models_stopped   | Number of models stopped                     |
   +---+---------------------+----------------------------------------------+


PMD supports the below list of 6 extended stats types per each model.

.. _table_octeon_cnxk_ml_model_xstats_names:

.. table:: OCTEON cnxk ML PMD model xstats names

   +---+---------------------+----------------------------------------------+
   | # | Type                | Description                                  |
   +===+=====================+==============================================+
   | 1 | Avg-HW-Latency      | Average hardware latency                     |
   +---+---------------------+----------------------------------------------+
   | 2 | Min-HW-Latency      | Minimum hardware latency                     |
   +---+---------------------+----------------------------------------------+
   | 3 | Max-HW-Latency      | Maximum hardware latency                     |
   +---+---------------------+----------------------------------------------+
   | 4 | Avg-FW-Latency      | Average firmware latency                     |
   +---+---------------------+----------------------------------------------+
   | 5 | Min-FW-Latency      | Minimum firmware latency                     |
   +---+---------------------+----------------------------------------------+
   | 6 | Max-FW-Latency      | Maximum firmware latency                     |
   +---+---------------------+----------------------------------------------+

Latency values reported by the PMD through xstats can have units,
either in cycles or nano seconds.
The units of the latency is determined during DPDK initialization
and would depend on the availability of SCLK.
Latencies are reported in nano seconds when the SCLK is available and in cycles otherwise.
Application needs to initialize at least one RVU for the clock to be available.

xstats names are dynamically generated by the PMD and would have the format
``Model-<model_id>-Type-<units>``.

For example::

   Model-1-Avg-FW-Latency-ns

The above xstat name would report average firmware latency in nano seconds
for model ID 1.

The number of xstats made available by the PMD change dynamically.
The number would increase with loading a model and would decrease with unloading a model.
The application needs to update the xstats map after a model is either loaded or unloaded.
