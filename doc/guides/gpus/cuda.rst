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

The CUDA GPU driver library has an header-only dependency on ``cuda.h`` and ``cudaTypedefs.h``.
To get these headers there are two options:

- Install `CUDA Toolkit <https://developer.nvidia.com/cuda-toolkit>`_
  (either regular or stubs installation).
- Download these two headers from this `CUDA headers
  <https://gitlab.com/nvidia/headers/cuda-individual/cudart>`_ repository.

You need to indicate to meson where CUDA headers files are through the CFLAGS variable.
Three ways:

- Set ``export CFLAGS=-I/usr/local/cuda/include`` before building
- Add CFLAGS in the meson command line ``CFLAGS=-I/usr/local/cuda/include meson build``
- Add the ``-Dc_args`` in meson command line ``meson build -Dc_args=-I/usr/local/cuda/include``

If headers are not found, the CUDA GPU driver library is not built.

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

- Register new child devices aka new CUDA Driver contexts.
- Allocate memory on the GPU.
- Register CPU memory to make it visible from GPU.

Minimal requirements
--------------------

Minimal requirements to enable the CUDA driver library are:

- NVIDIA GPU Ampere or Volta
- CUDA 11.4 Driver API or newer

`GPUDirect RDMA Technology <https://docs.nvidia.com/cuda/gpudirect-rdma/index.html>`_
allows compatible network cards (e.g. Mellanox) to directly send and receive packets
using GPU memory instead of additional memory copies through the CPU system memory.
To enable this technology, system requirements are:

- `nvidia-peermem <https://docs.nvidia.com/cuda/gpudirect-rdma/index.html#nvidia-peermem>`_
  module running on the system;
- Mellanox network card ConnectX-5 or newer (BlueField models included);
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

The application is based on vanilla DPDK example l2fwd
and is enhanced with GPU memory managed through gpudev library
and CUDA to launch the swap of packets MAC addresses workload on the GPU.

l2fwd-nv is not intended to be used for performance
(testpmd is the good candidate for this).
The goal is to show different use-cases about how a CUDA application can use DPDK to:

- Allocate memory on GPU device using gpudev library.
- Use that memory to create an external GPU memory mempool.
- Receive packets directly in GPU memory.
- Coordinate the workload on the GPU with the network and CPU activity to receive packets.
- Send modified packets directly from the GPU memory.
