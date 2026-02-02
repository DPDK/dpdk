..  SPDX-License-Identifier: BSD-3-Clause

Flow Parser Library
===================

Overview
--------

The flow parser library provides **one way** to create ``rte_flow`` C structures
by parsing testpmd-style command strings. This is particularly useful for
applications that need to accept flow rules from user input, configuration
files, or external control planes using the familiar testpmd syntax.

.. note::

   This library is not the only way to create rte_flow structures. Applications
   can also construct ``struct rte_flow_attr``, ``struct rte_flow_item[]``, and
   ``struct rte_flow_action[]`` directly in C code and pass them to the rte_flow
   API (``rte_flow_create()``, ``rte_flow_validate()``, etc.). The parser library
   is an alternative approach for cases where string-based input is preferred.

Public API
----------

The public API consists of only **four functions** declared in
``rte_flow_parser.h``:

* ``rte_flow_parser_init()`` - Initialize the parser library.
* ``rte_flow_parser_parse_attr_str()`` - Parse flow attributes from a string.
* ``rte_flow_parser_parse_pattern_str()`` - Parse flow pattern from a string.
* ``rte_flow_parser_parse_actions_str()`` - Parse flow actions from a string.

These functions provide lightweight parsing of testpmd-style flow rule
fragments into standard ``rte_flow`` C structures that can be used with
``rte_flow_create()``, ``rte_flow_validate()``, and other rte_flow APIs.

The library is built as ``librte_flow_parser``.

.. note::

   Additional internal functions (cmdline integration, full command parsing,
   encapsulation configuration accessors) are available in the internal header
   ``rte_flow_parser_private.h`` for use by testpmd and DPDK test applications.
   These private APIs are not part of the stable ABI and may change without
   notice.

Parser Initialization
---------------------

The library uses a single global parser instance. Initialize it once at
startup:

* ``rte_flow_parser_init(const struct rte_flow_parser_ops *ops)``
  initializes the global parser with a pair of callback tables. Pass ``NULL``
  for standalone parsing mode (using only the lightweight helpers).

The parser keeps internal state (defaults, temporary buffers) in global
storage. The parser is not thread-safe; guard concurrent parsing with
external synchronization.

Parsing Helpers
---------------

The public API provides lightweight helpers to parse flow rule fragments:

* ``rte_flow_parser_parse_attr_str(src, attr)`` parses flow attributes
  (e.g., ``"ingress group 1 priority 5"``) into the provided
  ``struct rte_flow_attr``.
* ``rte_flow_parser_parse_pattern_str(src, pattern, pattern_n)`` parses
  a pattern list (e.g., ``"eth / ipv4 src is 192.168.1.1 / tcp / end"``)
  and returns a pointer to the resulting ``struct rte_flow_item`` array
  plus its length.
* ``rte_flow_parser_parse_actions_str(src, actions, actions_n)`` parses
  an actions list (e.g., ``"queue index 5 / count / end"``) and returns
  a pointer to the resulting ``struct rte_flow_action`` array plus its length.

These helpers use internal storage; the returned pointers remain valid until
the next parse call on the same thread.

Example Usage
-------------

``examples/flow_parsing/main.c`` demonstrates the lightweight parsing helpers:

* Parse flow attributes with ``rte_flow_parser_parse_attr_str()``.
* Parse match patterns with ``rte_flow_parser_parse_pattern_str()``.
* Parse flow actions with ``rte_flow_parser_parse_actions_str()``.
* Print parsed results showing the structured data.

Build and run the example::

  meson configure -Dexamples=flow_parsing build
  ninja -C build
  ./build/examples/dpdk-flow_parsing

The output shows each parsed flow component, demonstrating that the parser
is decoupled from testpmd and usable in standalone applications without
requiring EAL initialization.

Internal API (testpmd only)
---------------------------

The internal header ``rte_flow_parser_private.h`` provides additional
functionality used by testpmd and DPDK test applications:

* Full command parsing (``rte_flow_parser_parse()``, ``rte_flow_parser_run()``)
* Callback model with ``struct rte_flow_parser_ops`` for query and command hooks
* Cmdline integration (``rte_flow_parser_cmdline_register()``, token callbacks)
* Encapsulation configuration accessors (``rte_flow_parser_*_conf()`` functions)
* Parser state reset (``rte_flow_parser_reset_defaults()``)
