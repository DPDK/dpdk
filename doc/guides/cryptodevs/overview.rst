..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Crypto Device Supported Functionality Matrices
----------------------------------------------

Supported Feature Flags

.. csv-table::
   :header: "Feature Flags", "qat", "null", "aesni_mb", "aesni_gcm", "snow3g", "kasumi"
   :stub-columns: 1

   "RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO",x,x,x,x,x,x
   "RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO",,,,,,
   "RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING",x,x,x,x,x,x
   "RTE_CRYPTODEV_FF_CPU_SSE",,,x,x,x,x
   "RTE_CRYPTODEV_FF_CPU_AVX",,,x,x,x,x
   "RTE_CRYPTODEV_FF_CPU_AVX2",,,x,x,,
   "RTE_CRYPTODEV_FF_CPU_AESNI",,,x,x,,
   "RTE_CRYPTODEV_FF_HW_ACCELERATED",x,,,,,

Supported Cipher Algorithms

.. csv-table::
   :header: "Cipher Algorithms", "qat", "null", "aesni_mb", "aesni_gcm", "snow3g", "kasumi"
   :stub-columns: 1

   "NULL",,x,,,,
   "AES_CBC_128",x,,x,,,
   "AES_CBC_192",x,,x,,,
   "AES_CBC_256",x,,x,,,
   "AES_CTR_128",x,,x,,,
   "AES_CTR_192",x,,x,,,
   "AES_CTR_256",x,,x,,,
   "SNOW3G_UEA2",x,,,,x,
   "KASUMI_F8",,,,,,x

Supported Authentication Algorithms

.. csv-table::
   :header: "Cipher Algorithms", "qat", "null", "aesni_mb", "aesni_gcm", "snow3g", "kasumi"
   :stub-columns: 1

   "NONE",,x,,,,
   "MD5",,,,,,
   "MD5_HMAC",,,x,,,
   "SHA1",,,,,,
   "SHA1_HMAC",x,,x,,,
   "SHA224",,,,,,
   "SHA224_HMAC",,,x,,,
   "SHA256",,,,,,
   "SHA256_HMAC",x,,x,,,
   "SHA384",,,,,,
   "SHA384_HMAC",,,x,,,
   "SHA512",,,,,,
   "SHA512_HMAC",x,,x,,,
   "AES_XCBC",x,,x,,,
   "SNOW3G_UIA2",x,,,,x,
   "KASUMI_F9",,,,,,x

Supported AEAD Algorithms

.. csv-table::
   :header: "AEAD Algorithms", "qat", "null", "aesni_mb", "aesni_gcm", "snow3g", "kasumi"
   :stub-columns: 1

   "AES_GCM_128",x,,x,,,
   "AES_GCM_192",x,,,,,
   "AES_GCM_256",x,,,,,
