..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Berkeley Packet Filter (BPF) Library
====================================

The DPDK provides an BPF library that gives the ability
to load and execute Enhanced Berkeley Packet Filter (eBPF) bytecode within
user-space dpdk application.

It supports basic set of features from eBPF spec.
Please refer to the
`eBPF spec <https://www.kernel.org/doc/Documentation/networking/filter.txt>`_
for more information.
Also it introduces basic framework to load/unload BPF-based filters
on eth devices (right now only via SW RX/TX callbacks).

The library API provides the following basic operations for working with BPF programs:

* **Loading**: The extensible API (``rte_bpf_load_ex``) is the recommended way
  to load a BPF program.
  By utilizing ``struct rte_bpf_prm_ex``, you can load an eBPF program
  from an ELF file on disk.

* **Cleanup:** Destroy a BPF execution context and free the associated memory
  using ``rte_bpf_destroy``.

*   Execute eBPF bytecode associated with provided input parameter.

The following is a concise example of loading an eBPF program from an ELF file,

.. code-block:: c

   struct rte_bpf_prm_ex prm = {
       .sz = sizeof(struct rte_bpf_prm_ex),
       .origin = RTE_BPF_ORIGIN_ELF_FILE,
       .elf_file = {
           .path = "ptype.o",
           .section = ".text",
       },
       .nb_prog_arg = 2,
       .prog_arg = {
           [0] = {
               .type = RTE_BPF_ARG_PTR_MBUF,
               .size = sizeof(struct rte_mbuf),
               .buf_size = RTE_MBUF_DEFAULT_BUF_SIZE,
           },
           [1] = {
               .type = RTE_BPF_ARG_RAW,
               .size = sizeof(uint64_t),
           },
       },
   };
   struct rte_bpf *bpf = rte_bpf_load_ex(&prm);
   if (bpf == NULL) {
       /* Handle load failure */
   }

   rte_bpf_destroy(bpf);

Packet data load instructions
-----------------------------

DPDK supports two non-generic instructions: ``(BPF_ABS | size | BPF_LD)``
and ``(BPF_IND | size | BPF_LD)`` which are used to access packet data.
These instructions can only be used when execution context is a pointer to
``struct rte_mbuf`` and have seven implicit operands.
Register ``R6`` is an implicit input that must contain pointer to ``rte_mbuf``.
Register ``R0`` is an implicit output which contains the data fetched from the
packet. Registers ``R1-R5`` are scratch registers
and must not be used to store the data across these instructions.
These instructions have implicit program exit condition as well. When
eBPF program is trying to access the data beyond the packet boundary,
the interpreter will abort the execution of the program. JIT compilers
therefore must preserve this property. ``src_reg`` and ``imm32`` fields are
explicit inputs to these instructions.
For example, ``(BPF_IND | BPF_W | BPF_LD)`` means:

.. code-block:: c

    uint32_t tmp;
    R0 = rte_pktmbuf_read((const struct rte_mbuf *)R6,  src_reg + imm32, sizeof(tmp), &tmp);
    if (R0 == NULL)
        return FAILED;
    R0 = ntohl(*(uint32_t *)R0);

and ``R1-R5`` were scratched.


Not currently supported eBPF features
-------------------------------------

 - JIT support only available for X86_64 and arm64 platforms
 - cBPF
 - tail-pointer call
 - eBPF MAP
 - external function calls for 32-bit platforms
