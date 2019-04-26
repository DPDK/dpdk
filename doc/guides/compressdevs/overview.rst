..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Compression Device Supported Functionality Matrices
===================================================

Supported Feature Flags
-----------------------

.. _table_compression_pmd_features:

.. include:: overview_feature_table.txt

.. Note::

   - "Pass-through" feature flag refers to the ability of the PMD
     to let input buffers pass-through it, copying the input to the output,
     without making any modifications to it (no compression done).

   - "OOP SGL In SGL Out" feature flag stands for
     "Out-of-place Scatter-gather list Input, Scatter-gather list Output",
     which means PMD supports different scatter-gather styled input and output buffers
     (i.e. both can consists of multiple segments).

   - "OOP SGL In LB Out" feature flag stands for
     "Out-of-place Scatter-gather list Input, Linear Buffers Output",
     which means PMD supports input from scatter-gathered styled buffers, outputting linear buffers
     (i.e. single segment).

   - "OOP LB In SGL Out" feature flag stands for
     "Out-of-place Linear Buffers Input, Scatter-gather list Output",
     which means PMD supports input from linear buffer, outputting scatter-gathered styled buffers.
