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


Installation
------------

The OCTEON cnxk ML PMD may be compiled natively on an OCTEON cnxk platform
or cross-compiled on an x86 platform.

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


Runtime Config Options
----------------------

**Firmware file path** (default ``/lib/firmware/mlip-fw.bin``)

  Path to the firmware binary to be loaded during device configuration.
  The parameter ``fw_path`` can be used by the user
  to load ML firmware from a custom path.

  For example::

     -a 0000:00:10.0,fw_path="/home/user/ml_fw.bin"

  With the above configuration, driver loads the firmware from the path
  ``/home/user/ml_fw.bin``.


**Enable DPE warnings** (default ``1``)

  ML firmware can be configured during load to handle the DPE errors reported
  by ML inference engine.
  When enabled, firmware would mask the DPE non-fatal hardware errors as warnings.
  The parameter ``enable_dpe_warnings`` is used fo this configuration.

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

  For example::

     -a 0000:00:10.0,cache_model_data=0

  With the above configuration, model data caching is disabled.


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

  For example::

     -a 0000:00:10.0,hw_queue_lock=1

  With the above configuration, spinlock version of hardware enqueue function is used
  in the fast path enqueue burst operation.


**Polling memory location** (default ``ddr``)

  ML cnxk driver provides the option to select the memory location to be used
  for polling to check the inference request completion.
  Driver supports using either the DDR address space (``ddr``)
  or ML registers (``register``) as polling locations.
  The parameter ``poll_mem`` is used to specify the poll location.

  For example::

     -a 0000:00:10.0,poll_mem="register"

  With the above configuration, ML cnxk driver is configured to use ML registers
  for polling in fastpath requests.


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

Marvell cnxk ML PMD supports reporting the inference latencies
through extended statistics.
The PMD supports the below list of 6 extended stats types per each model.
Total number of extended stats would be equal to 6 x number of models loaded.

.. _table_octeon_cnxk_ml_xstats_names:

.. table:: OCTEON cnxk ML PMD xstats names

   +---+---------------------+----------------------------------------------+
   | # | Type                | Description                                  |
   +===+=====================+==============================================+
   | 1 | Avg-HW-Latency      | Average hardware latency                     |
   +---+---------------------+----------------------------------------------+
   | 2 | Min-HW-Latency      | Minimum hardware latency                     |
   +---+---------------------+----------------------------------------------+
   | 3 | Max-HW-Latency      | Maximum hardware latency                     |
   +---+---------------------+----------------------------------------------+
   | 4 | Avg-HW-Latency      | Average firmware latency                     |
   +---+---------------------+----------------------------------------------+
   | 5 | Avg-HW-Latency      | Minimum firmware latency                     |
   +---+---------------------+----------------------------------------------+
   | 6 | Avg-HW-Latency      | Maximum firmware latency                     |
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
