..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2017 Intel Corporation.

Crypto Device Supported Functionality Matrices
==============================================

Supported Feature Flags
-----------------------

.. _table_crypto_pmd_features:

.. include:: overview_feature_table.txt

.. Note::

   - "In Place SGL" feature flag stands for "In place Scatter-gather list",
     which means that an input buffer can consist of multiple segments,
     being the operation in-place (input address = output address).

   - "OOP SGL In SGL Out" feature flag stands for
     "Out-of-place Scatter-gather list Input, Scatter-gather list Output",
     which means PMD supports different scatter-gather styled input and output buffers
     (i.e. both can consist of multiple segments).

   - "OOP SGL In LB Out" feature flag stands for
     "Out-of-place Scatter-gather list Input, Linear Buffers Output",
     which means PMD supports input from scatter-gather styled buffers,
     outputting linear buffers (i.e. single segment).

   - "OOP LB In SGL Out" feature flag stands for
     "Out-of-place Linear Buffers Input, Scatter-gather list Output",
     which means PMD supports input from linear buffer, outputting
     scatter-gather styled buffers.

   - "OOP LB In LB Out" feature flag stands for
     "Out-of-place Linear Buffers Input, Linear Buffers Output",
     which means that Out-of-place operation is supported,
     with linear input and output buffers.

   - "RSA PRIV OP KEY EXP" feature flag means PMD support RSA private key
     operation (Sign and Decrypt) using exponent key type only.

   - "RSA PRIV OP KEY QT" feature flag means PMD support RSA private key
     operation (Sign and Decrypt) using quintuple (crt) type key only.

   - "Digest encrypted" feature flag means PMD support hash-cipher cases,
     where generated digest is appended to and encrypted with the data.

   - "CIPHER_MULTIPLE_DATA_UNITS" feature flag means PMD support operations
      on multiple data-units message.

   - "CIPHER_WRAPPED_KEY" feature flag means PMD support wrapped key in cipher
      xform.


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

Supported Asymmetric Algorithms
-------------------------------

.. _table_crypto_pmd_asym_algos:

.. include:: overview_asym_table.txt

Supported Operating Systems
-------------------------------

.. _table_crypto_pmd_os:

.. include:: overview_os_table.txt
