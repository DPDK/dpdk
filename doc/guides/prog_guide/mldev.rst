..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2022 Marvell.

Machine Learning Device Library
===============================

The MLDEV library provides a Machine Learning device framework for the management and
provisioning of hardware and software ML poll mode drivers,
defining an API which support a number of ML operations
including device handling and inference processing.
The ML model creation and training is outside of the scope of this library.

The ML framework is built on the following model:

.. _figure_mldev_work_flow:

.. figure:: img/mldev_flow.*

   Work flow of inference on MLDEV

ML Device
   A hardware or software-based implementation of ML device API
   for running inferences using a pre-trained ML model.

ML Model
   An ML model is an algorithm trained over a dataset.
   A model consists of procedure/algorithm and data/pattern
   required to make predictions on live data.
   Once the model is created and trained outside of the DPDK scope,
   the model can be loaded via ``rte_ml_model_load()``
   and then start it using ``rte_ml_model_start()`` API function.
   The ``rte_ml_model_params_update()`` can be used to update the model parameters
   such as weights and bias without unloading the model using ``rte_ml_model_unload()``.

ML Inference
   ML inference is the process of feeding data to the model
   via ``rte_ml_enqueue_burst()`` API function
   and use ``rte_ml_dequeue_burst()`` API function
   to get the calculated outputs / predictions from the started model.


Design Principles
-----------------

The MLDEV library follows the same basic principles as those used in DPDK's
Ethernet Device framework and the Crypto framework.
The MLDEV framework provides a generic Machine Learning device framework
which supports both physical (hardware) and virtual (software) ML devices
as well as an ML API to manage and configure ML devices.
The API also supports performing ML inference operations
through ML poll mode driver.


Device Operations
-----------------

Device Creation
~~~~~~~~~~~~~~~

Physical ML devices are discovered during the PCI probe/enumeration,
through the EAL functions which are executed at DPDK initialization,
based on their PCI device identifier, each unique PCI BDF (bus/bridge, device, function).
ML physical devices, like other physical devices in DPDK can be allowed or blocked
using the EAL command line options.


Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each device, whether virtual or physical is uniquely designated by two identifiers:

- A unique device index used to designate the ML device
  in all functions exported by the MLDEV API.

- A device name used to designate the ML device in console messages,
  for administration or debugging purposes.


Device Features and Capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ML devices may support different feature set.
In order to get the supported PMD feature ``rte_ml_dev_info_get()`` API
which return the info of the device and its supported features.


Device Configuration
~~~~~~~~~~~~~~~~~~~~

The configuration of each ML device includes the following operations:

- Allocation of resources, including hardware resources if a physical device.
- Resetting the device into a well-known default state.
- Initialization of statistics counters.

The ``rte_ml_dev_configure()`` API is used to configure a ML device.

.. code-block:: c

   int rte_ml_dev_configure(int16_t dev_id, const struct rte_ml_dev_config *cfg);

The ``rte_ml_dev_config`` structure is used to pass the configuration parameters
for the ML device, for example number of queue pairs, maximum number of models,
maximum size of model and so on.

Configuration of Queue Pairs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each ML device can be configured with number of queue pairs.
Each queue pair is configured using ``rte_ml_dev_queue_pair_setup()``


Logical Cores, Memory and Queues Pair Relationships
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Multiple logical cores should never share the same queue pair
for enqueuing operations or dequeueing operations on the same ML device
since this would require global locks and hinder performance.


Configuration of Machine Learning models
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pre-trained ML models that are built using external ML compiler / training frameworks
are used to perform inference operations.
These models are configured on an ML device in a two-stage process
that includes loading the model on an ML device,
and starting the model to accept inference operations.
Inference operations can be queued for a model
only when the model is in started state.
Model load stage assigns a Model ID,
which is unique for the model in a driver's context.
Model ID is used during all subsequent slow-path and fast-path operations.

Model loading and start is done
through the ``rte_ml_model_load()`` and ``rte_ml_model_start()`` functions.

Similarly stop and unloading are done
through ``rte_ml_model_stop()`` and ``rte_ml_model_unload()`` functions.

Stop and unload functions would release the resources allocated for the models.
Inference tasks cannot be queued for a model that is stopped.

Detailed information related to the model can be retrieved from the driver
using the function ``rte_ml_model_info_get()``.
Model information is accessible to the application
through the ``rte_ml_model_info`` structure.
Information available to the user would include the details related to
the inputs and outputs, and the maximum batch size supported by the model.

User can optionally update the model parameters such as weights and bias,
without unloading the model, through the ``rte_ml_model_params_update()`` function.
A model should be in stopped state to update the parameters.
Model has to be started in order to enqueue inference requests after parameters update.


Enqueue / Dequeue
~~~~~~~~~~~~~~~~~

The burst enqueue API uses a ML device identifier and a queue pair identifier
to specify the device queue pair to schedule the processing on.
The ``nb_ops`` parameter is the number of operations to process
which are supplied in the ``ops`` array of ``rte_ml_op`` structures.
The enqueue function returns the number of operations it enqueued for processing,
a return value equal to ``nb_ops`` means that all packets have been enqueued.

The dequeue API uses the same format as the enqueue API of processed
but the ``nb_ops`` and ``ops`` parameters are now used to specify
the max processed operations the user wishes to retrieve
and the location in which to store them.
The API call returns the actual number of processed operations returned;
this can never be larger than ``nb_ops``.

``rte_ml_op`` provides the required information to the driver
to queue an ML inference task.
ML op specifies the model to be used and the number of batches
to be executed in the inference task.
Input and output buffer information is specified through
the structure ``rte_ml_buff_seg``, which supports segmented data.
Input is provided through the ``rte_ml_op::input``
and output through ``rte_ml_op::output``.
Data pointed in each op, should not be released until the dequeue of that op.


Quantize and Dequantize
~~~~~~~~~~~~~~~~~~~~~~~

Inference operations performed with lower precision types would improve
the throughput and efficiency of the inference execution
with a minimal loss of accuracy, which is within the tolerance limits.
Quantization and dequantization is the process of converting data
from a higher precision type to a lower precision type and vice-versa.
ML library provides the functions ``rte_ml_io_quantize()`` and ``rte_ml_io_dequantize()``
to enable data type conversions.
User needs to provide the address of the quantized and dequantized data buffers
to the functions, along the number of the batches in the buffers.

For quantization, the dequantized data is assumed to be
of the type ``dtype`` provided by the ``rte_ml_model_info::input``
and the data is converted to ``qtype`` provided by the ``rte_ml_model_info::input``.

For dequantization, the quantized data is assumed to be
of the type ``qtype`` provided by the ``rte_ml_model_info::output``
and the data is converted to ``dtype`` provided by the ``rte_ml_model_info::output``.

Size of the buffers required for the input and output can be calculated
using the functions ``rte_ml_io_input_size_get()`` and ``rte_ml_io_output_size_get()``.
These functions would get the buffer sizes for both quantized and dequantized data
for the given number of batches.
