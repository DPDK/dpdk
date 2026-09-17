.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2026 The DPDK contributors

dpdk-validate-bpf Application
=============================

The ``dpdk-validate-bpf`` tool is an application
that allows evaluating BPF programs against the ``lib/bpf`` library.
It can be used to pre-validate BPF programs
before loading them in a real application
to ensure that they pass the internal verifier.
This makes it a useful tool for integrating into build systems
and continuous integration (CI) pipelines
to verify the correctness of BPF programs during compilation,
as well as for debugging validation issues.

Running the Application
-----------------------

The tool requires a path to a compiled BPF object file.
It also provides options
to configure the execution environment and external symbol definitions.

For a comprehensive list of options and their meanings,
refer to the built-in help by running ``dpdk-validate-bpf --help``.

The most important options for a minimal example are:

``--prog-arg <type>``
  Define program arguments, if different from ``struct rte_mbuf *``.
  This argument may be repeated up to 5 times.

``--xsym '<type> <name> | <type> <name>(<type>, ...)'``
  Define an external symbol (variable or function) that the BPF program uses.
  This allows the validator to recognize external calls and memory accesses.
  Multiple external symbols can be defined by repeating this option.

``--section <name>``
  Specify the ELF section name in the BPF object file to load and validate.
  The default section name is ``.text``,
  which is typically fine in cases where the object file contains only one program.

Interactive Debugging Mode
--------------------------

The interactive mode (enabled with the ``--debug`` flag)
allows the user to debug the BPF validation process itself.
This is particularly useful when BPF program fails verification,
allowing you to trace the state changes per instruction
and understand the validator's decisions.

Design
~~~~~~

The interactive debugger provides a command-line interface similar to GDB.
It allows setting breakpoints at specific instructions,
or catchpoints on validator events (such as ``branch-enter`` or ``branch-prune``).
Users can step through the validation process,
inspect the state of registers,
and query if certain conditional jumps may be taken
based on the validator's knowledge of the program state.

Usage Example
~~~~~~~~~~~~~

Launch the tool in debug mode with a compiled BPF object file:

.. code-block:: console

   $ dpdk-validate-bpf bpf_prog.o --prog-arg={'void *',uint64_t} --debug
    =>          0:  b7 00 00 00 01 00 00 00         mov r0, #0x1
    (validate) list 3
    =>          0:  b7 00 00 00 01 00 00 00         mov r0, #0x1
                1:  25 02 01 00 0e 00 00 00         jgt r2, #0xe, L3
                2:  b7 00 00 00 00 00 00 00         mov r0, #0x0
    (validate) break 2
    Breakpoint 0 at 2.
    (validate) catch branch-enter
    Catchpoint 1 on branch-enter.
    (validate) continue
    Catchpoint 1 on branch-enter.
    Entered new branch at pc 1.
    =>          3:  95 00 00 00 00 00 00 00         exit
    (validate) where
                1:  25 02 01 00 0e 00 00 00         jgt r2, #0xe, L3        ; taken
    =>          3:  95 00 00 00 00 00 00 00         exit
    (validate) info r1
       r1:  %buffer<0> + 0
    (validate) info r2
       r2:  0xf..UINT64_MAX
    (validate) may r2 <= 14
    NO
    (validate) continue
    Catchpoint 1 on branch-enter.
    Entered new branch at pc 1.
    Breakpoint 0 at 2.
    =>          2:  b7 00 00 00 00 00 00 00         mov r0, #0x0
    (validate) continue
    Validation succeeded.
    (validate) quit
