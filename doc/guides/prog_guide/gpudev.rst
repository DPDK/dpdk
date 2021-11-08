.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2021 NVIDIA Corporation & Affiliates

General-Purpose Graphics Processing Unit Library
================================================

When mixing networking activity with task processing on a GPU device,
there may be the need to put in communication the CPU with the device
in order to manage the memory, synchronize operations, exchange info, etc..

By means of the generic GPU interface provided by this library,
it is possible to allocate a chunk of GPU memory and use it
to create a DPDK mempool with external mbufs having the payload
on the GPU memory, enabling any network interface card
(which support this feature like Mellanox NIC)
to directly transmit and receive packets using GPU memory.

Additionally, this library provides a number of functions
to enhance the dialog between CPU and GPU.

Out of scope of this library is to provide a wrapper for GPU specific libraries
(e.g. CUDA Toolkit or OpenCL), thus it is not possible to launch workload
on the device or create GPU specific objects
(e.g. CUDA Driver context or CUDA Streams in case of NVIDIA GPUs).


Features
--------

This library provides a number of features:

- Interoperability with device-specific library through generic handlers.


API Overview
------------
