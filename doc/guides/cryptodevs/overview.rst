..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2017 Intel Corporation.

Crypto Device Supported Functionality Matrices
==============================================

Supported Feature Flags
-----------------------

.. _table_crypto_pmd_features:

.. include:: overview_feature_table.txt

Note, the mbuf scatter gather feature (aka chained mbufs, scatter-gather-lists
or SGLs) indicate all following combinations are supported unless otherwise called
out in the Limitations section of each PMD.

* In place operation, input buffer as multiple segments, same buffer used for output
* Out of place operation, input buffer as single segment and output as multiple segments
* Out of place operation, input buffer as multiple segments and output as single segment
* Out of place operation, input buffer as multiple segments and output as multiple segments


Supported Cipher Algorithms
---------------------------

.. _table_crypto_pmd_cipher_algos:

.. include:: overview_cipher_table.txt

Supported Authentication Algorithms
-----------------------------------

.. _table_crypto_pmd_auth_algos:

.. include:: overview_auth_table.txt

Supported AEAD Algorithms
-------------------------

.. _table_crypto_pmd_aead_algos:

.. include:: overview_aead_table.txt
