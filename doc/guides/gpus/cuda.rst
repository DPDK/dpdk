.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2021 NVIDIA Corporation & Affiliates

CUDA GPU driver
===============

The CUDA GPU driver library (**librte_gpu_cuda**) provides support for NVIDIA GPUs.
Information and documentation about these devices can be found on the
`NVIDIA website <http://www.nvidia.com>`_. Help is also provided by the
`NVIDIA CUDA Toolkit developer zone <https://docs.nvidia.com/cuda>`_.

Build dependencies
------------------

The CUDA GPU driver library has a header-only dependency on ``cuda.h`` and ``cudaTypedefs.h``.
To get these headers, there are two options:

- Install `CUDA Toolkit <https://developer.nvidia.com/cuda-toolkit>`_
  (either regular or stubs installation).
- Download these two headers from this `CUDA headers
  <https://gitlab.com/nvidia/headers/cuda-individual/cudart>`_ repository.

You can point to CUDA header files either with the ``CFLAGS`` environment variable,
or with the ``c_args`` Meson option. Examples:

- ``CFLAGS=-I/usr/local/cuda/include meson setup build``
- ``meson setup build -Dc_args=-I/usr/local/cuda/include``

If headers are not found, the CUDA GPU driver library is not built.

CPU map GPU memory
~~~~~~~~~~~~~~~~~~

To enable this gpudev feature (i.e. implement the ``rte_gpu_mem_cpu_map``),
you need the `GDRCopy <https://github.com/NVIDIA/gdrcopy>`_ library and driver
installed on your system.

A quick recipe to download, build and run GDRCopy library and driver:

.. code-block:: console

  $ git clone https://github.com/NVIDIA/gdrcopy.git
  $ make
  $ # make install to install GDRCopy library system wide
  $ # Launch gdrdrv kernel module on the system
  $ sudo ./insmod.sh

You need to indicate to Meson where GDRCopy header files are as in case of CUDA headers.
An example would be:

.. code-block:: console

  $ meson setup build -Dc_args="-I/usr/local/cuda/include -I/path/to/gdrcopy/include"

If headers are not found, the CUDA GPU driver library is built without the CPU map capability,
and will return an error if the application invokes the gpudev ``rte_gpu_mem_cpu_map`` function.


CUDA Shared Library
-------------------

To avoid any system configuration issue, the CUDA API **libcuda.so** shared library
is not linked at building time because of a Meson bug that looks
for `cudart` module even if the `meson.build` file only requires default `cuda` module.

**libcuda.so** is loaded at runtime in the ``cuda_gpu_probe`` function through ``dlopen``
when the very first GPU is detected.
If CUDA installation resides in a custom directory,
the environment variable ``CUDA_PATH_L`` should specify where ``dlopen``
can look for **libcuda.so**.

All CUDA API symbols are loaded at runtime as well.
For this reason, to build the CUDA driver library,
no need to install the CUDA library.

CPU map GPU memory
~~~~~~~~~~~~~~~~~~

Similarly to CUDA shared library, if the **libgdrapi.so** shared library
is not installed in default locations (e.g. /usr/local/lib),
you can use the variable ``GDRCOPY_PATH_L``.

As an example, to enable the CPU map feature sanity check,
run the ``app/test-gpudev`` application with:

.. code-block:: console

  $ sudo CUDA_PATH_L=/path/to/libcuda GDRCOPY_PATH_L=/path/to/libgdrapi ./build/app/dpdk-test-gpudev

Additionally, the ``gdrdrv`` kernel module built with the GDRCopy project
has to be loaded on the system:

.. code-block:: console

  $ lsmod | egrep gdrdrv
  gdrdrv                 20480  0
  nvidia              35307520  19 nvidia_uvm,nv_peer_mem,gdrdrv,nvidia_modeset


Design
------

**librte_gpu_cuda** relies on CUDA Driver API (no need for CUDA Runtime API).

Goal of this driver library is not to provide a wrapper for the whole CUDA Driver API.
Instead, the scope is to implement the generic features of gpudev API.
For a CUDA application, integrating the gpudev library functions
using the CUDA driver library is quite straightforward
and doesn't create any compatibility problem.

Initialization
~~~~~~~~~~~~~~

During initialization, CUDA driver library detects NVIDIA physical GPUs
on the system or specified via EAL device options (e.g. ``-a b6:00.0``).
The driver initializes the CUDA driver environment through ``cuInit(0)`` function.
For this reason, it's required to set any CUDA environment configuration before
calling ``rte_eal_init`` function in the DPDK application.

If the CUDA driver environment has been already initialized, the ``cuInit(0)``
in CUDA driver library has no effect.

CUDA Driver sub-contexts
~~~~~~~~~~~~~~~~~~~~~~~~

After initialization, a CUDA application can create multiple sub-contexts
on GPU physical devices.
Through gpudev library, is possible to register these sub-contexts
in the CUDA driver library as child devices having as parent a GPU physical device.

CUDA driver library also supports `MPS
<https://docs.nvidia.com/deploy/pdf/CUDA_Multi_Process_Service_Overview.pdf>`__.

GPU memory management
~~~~~~~~~~~~~~~~~~~~~

The CUDA driver library maintains a table of GPU memory addresses allocated
and CPU memory addresses registered associated to the input CUDA context.
Whenever the application tried to deallocate or deregister a memory address,
if the address is not in the table the CUDA driver library will return an error.

Features
--------

- Register new child devices, aka CUDA driver contexts.
- Allocate memory on the GPU.
- Register CPU memory to make it visible from GPU.

Minimal requirements
--------------------

Minimal requirements to enable the CUDA driver library are:

- NVIDIA GPU Ampere or Volta
- CUDA 11.4 Driver API or newer

`GPUDirect RDMA Technology <https://docs.nvidia.com/cuda/gpudirect-rdma/index.html>`_
allows compatible network cards (e.g. ConnectX) to directly send and receive packets
using GPU memory instead of additional memory copies through the CPU system memory.
To enable this technology, system requirements are:

- `nvidia-peermem <https://docs.nvidia.com/cuda/gpudirect-rdma/index.html#nvidia-peermem>`_
  module running on the system;
- NVIDIA network card ConnectX-5 or newer (BlueField models included);
- DPDK mlx5 PMD enabled;
- To reach the best performance, an additional PCIe switch between GPU and NIC is recommended.

Limitations
-----------

Supported only on Linux.

Supported GPUs
--------------

The following NVIDIA GPU devices are supported by this CUDA driver library:

- NVIDIA A100 80GB PCIe
- NVIDIA A100 40GB PCIe
- NVIDIA A30 24GB
- NVIDIA A10 24GB
- NVIDIA V100 32GB PCIe
- NVIDIA V100 16GB PCIe

External references
-------------------

A good example of how to use the GPU CUDA driver library through the gpudev library
is the l2fwd-nv application that can be found `here <https://github.com/NVIDIA/l2fwd-nv>`_.

The application is based on the DPDK example l2fwd,
with GPU memory managed through gpudev library.
It includes a CUDA workload swapping MAC addresses
of packets received in the GPU.

l2fwd-nv is not intended to be used for performance
(testpmd is the good candidate for this).
The goal is to show different use-cases about how a CUDA application can use DPDK to:

- Allocate memory on GPU device using gpudev library.
- Use that memory to create an external GPU memory mempool.
- Receive packets directly in GPU memory.
- Coordinate the workload on the GPU with the network and CPU activity to receive packets.
- Send modified packets directly from the GPU memory.
