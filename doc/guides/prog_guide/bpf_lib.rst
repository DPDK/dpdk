..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Berkeley Packet Filter Library
==============================

The DPDK provides an BPF library that gives the ability
to load and execute Enhanced Berkeley Packet Filter (eBPF) bytecode within
user-space dpdk application.

It supports basic set of features from eBPF spec.
Please refer to the
`eBPF spec <https://www.kernel.org/doc/Documentation/networking/filter.txt>`
for more information.
Also it introduces basic framework to load/unload BPF-based filters
on eth devices (right now only via SW RX/TX callbacks).

The library API provides the following basic operations:

*  Create a new BPF execution context and load user provided eBPF code into it.

*   Destroy an BPF execution context and its runtime structures and free the associated memory.

*   Execute eBPF bytecode associated with provided input parameter.

*   Provide information about natively compiled code for given BPF context.

*   Load BPF program from the ELF file and install callback to execute it on given ethdev port/queue.

Not currently supported eBPF features
-------------------------------------

 - JIT for non X86_64 platforms
 - cBPF
 - tail-pointer call
 - eBPF MAP
 - skb
 - external function calls for 32-bit platforms
