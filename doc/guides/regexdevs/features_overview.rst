.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

Overview of RegEx Drivers Features
==================================

This section explains the supported features that are listed in the table below.

Cross buffer
  Support cross buffer detection.

PCRE start anchor
  Support PCRE start anchor.

PCRE atomic grouping
  Support PCRE atomic grouping.

PCRE back reference
  Support PCRE back reference.

PCRE back tracking ctrl
  Support PCRE back tracking ctrl.

PCRE call outs
  Support PCRE call routes.

PCRE forward reference
  Support Forward reference.

PCRE greedy
  Support PCRE greedy mode.

PCRE match all
  Support PCRE match all.

PCRE match as end
  Support match as end.

PCRE match point rst
  Support PCRE match point reset directive.

PCRE New line conventions
  Support new line conventions.

PCRE new line SEQ
  Support new line sequence.

PCRE look around
  Support PCRE look around.

PCRE possessive qualifiers
  Support PCRE possessive qualifiers.

PCRE subroutine references
  Support PCRE subroutine references.

PCRE UTF 8
  Support UTF-8.

PCRE UTF 16
  Support UTF-16.

PCRE UTF 32
  Support UTF-32.

PCRE word boundary
  Support word boundaries.

Run time compilation
  Support compilation during run time.

Armv8
  Support armv8 (64bit) architecture.

x86
  Support x86 architecture.

.. note::

   Most of the features capabilities should be provided by the drivers via the
   RegEx ``info_get`` operation.


References
==========

  * `PCRE: PCRE pattern man page <https://www.pcre.org/original/doc/html/pcrepattern.html>`_


Features Table
==============

.. _table_regex_pmd_features:

.. include:: overview_feature_table.txt

.. Note::

   Features marked with "P" are partially supported. Refer to the appropriate
   driver guide in the following sections for details.
