..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Federal Information Processing Standards (FIPS) CryptoDev Validation
====================================================================

Overview
--------

Federal Information Processing Standards (FIPS) are publicly announced standards
developed by the United States federal government for use in computer systems by
non-military government agencies and government contractors.

This application is used to parse and perform symmetric cryptography
computation to the NIST Cryptographic Algorithm Validation Program (CAVP) test
vectors.

For an algorithm implementation to be listed on a cryptographic module
validation certificate as an Approved security function, the algorithm
implementation must meet all the requirements of FIPS 140-2 and must
successfully complete the cryptographic algorithm validation process.

Limitations
-----------

* Only NIST CAVP request files are parsed by this application.
* The version of request file supported is ``CAVS 21.0``
* If the header comment in a ``.req`` file does not contain a Algo tag
  i.e ``AES,TDES,GCM`` you need to manually add it into the header comment for
  example::

      # VARIABLE KEY - KAT for CBC / # TDES VARIABLE KEY - KAT for CBC

* The application does not supply the test vectors. The user is expected to
  obtain the test vector files from `NIST
  <https://csrc.nist.gov/projects/cryptographic-algorithm-validation-
  program/block-ciphers>`_ website. To obtain the ``.req`` files you need to
  email a person from the NIST website and pay for the ``.req`` files.
  The ``.rsp`` files from the site can be used to validate and compare with
  the ``.rsp`` files created by the FIPS application.

* Supported test vectors
    * AES-CBC (128,192,256) - GFSbox, KeySbox, MCT, MMT
    * AES-GCM (128,192,256) - EncryptExtIV, Decrypt
    * AES-CCM (128) - VADT, VNT, VPT, VTT, DVPT
    * AES-CMAC (128) - Generate, Verify
    * HMAC (SHA1, SHA224, SHA256, SHA384, SHA512)
    * TDES-CBC (1 Key, 2 Keys, 3 Keys) - MMT, Monte, Permop, Subkey, Varkey,
      VarText

Application Information
-----------------------

If a ``.req`` is used as the input file after the application is finished
running it will generate a response file or ``.rsp``. Differences between the
two files are, the ``.req`` file has missing information for instance if doing
encryption you will not have the cipher text and that will be generated in the
response file. Also if doing decryption it will not have the plain text until it
finished the work and in the response file it will be added onto the end of each
operation.

The application can be run with a ``.rsp`` file and what the outcome of that
will be is it will add a extra line in the generated ``.rsp`` which should be
the same as the ``.rsp`` used to run the application, this is useful for
validating if the application has done the operation correctly.


Compiling the Application
-------------------------

* Compile Application

    .. code-block:: console

         make -C examples/fips_validation

*  Run ``dos2unix`` on the request files

    .. code-block:: console

         dos2unix AES/req/*
         dos2unix AES_GCM/req/*
         dos2unix CCM/req/*
         dos2unix CMAC/req/*
         dos2unix HMAC/req/*
         dos2unix TDES/req/*

Running the Application
-----------------------

The application requires a number of command line options:

    .. code-block:: console

         ./fips_validation [EAL options]
         -- --req-file FILE_PATH/FOLDER_PATH
         --rsp-file FILE_PATH/FOLDER_PATH
         [--cryptodev DEVICE_NAME] [--cryptodev-id ID] [--path-is-folder]

where,
  * req-file: The path of the request file or folder, separated by
    ``path-is-folder`` option.

  * rsp-file: The path that the response file or folder is stored. separated by
    ``path-is-folder`` option.

  * cryptodev: The name of the target DPDK Crypto device to be validated.

  * cryptodev-id: The id of the target DPDK Crypto device to be validated.

  * path-is-folder: If presented the application expects req-file and rsp-file
    are folder paths.


To run the application in linuxapp environment to test one AES FIPS test data
file for crypto_aesni_mb PMD, issue the command:

.. code-block:: console

    $ ./fips_validation --vdev crypto_aesni_mb --
    --req-file /PATH/TO/REQUEST/FILE.req --rsp-file ./PATH/TO/RESPONSE/FILE.rsp
    --cryptodev crypto_aesni_mb

To run the application in linuxapp environment to test all AES-GCM FIPS test
data files in one folder for crypto_aesni_gcm PMD, issue the command:

.. code-block:: console

    $ ./fips_validation --vdev crypto_aesni_gcm0 --
    --req-file /PATH/TO/REQUEST/FILE/FOLDER/
    --rsp-file ./PATH/TO/RESPONSE/FILE/FOLDER/
    --cryptodev-id 0 --path-is-folder
