..  BSD LICENSE
    Copyright(c) 2015-2016 Intel Corporation. All rights reserved.

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

Quick Assist Crypto Poll Mode Driver
====================================

The QAT PMD provides poll mode crypto driver support for **Intel QuickAssist
Technology DH895xxC** hardware accelerator.


Features
--------

The QAT PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_SYM_CIPHER_AES128_CBC``
* ``RTE_CRYPTO_SYM_CIPHER_AES192_CBC``
* ``RTE_CRYPTO_SYM_CIPHER_AES256_CBC``
* ``RTE_CRYPTO_SYM_CIPHER_AES128_CTR``
* ``RTE_CRYPTO_SYM_CIPHER_AES192_CTR``
* ``RTE_CRYPTO_SYM_CIPHER_AES256_CTR``
* ``RTE_CRYPTO_SYM_CIPHER_SNOW3G_UEA2``
* ``RTE_CRYPTO_CIPHER_AES_GCM``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_AES_XCBC_MAC``
* ``RTE_CRYPTO_AUTH_SNOW3G_UIA2``


Limitations
-----------

* Chained mbufs are not supported.
* Hash only is not supported except Snow3G UIA2.
* Cipher only is not supported except Snow3G UEA2.
* Only supports the session-oriented API implementation (session-less APIs are not supported).
* Not performance tuned.
* Snow3g(UEA2) supported only if cipher length, cipher offset fields are byte-aligned.
* Snow3g(UIA2) supported only if hash length, hash offset fields are byte-aligned.
* No BSD support as BSD QAT kernel driver not available.


Installation
------------

To use the DPDK QAT PMD an SRIOV-enabled QAT kernel driver is required. The
VF devices exposed by this driver will be used by QAT PMD.

If you are running on kernel 4.4 or greater, see instructions for
`Installation using kernel.org driver`_ below. If you are on a kernel earlier
than 4.4, see `Installation using 01.org QAT driver`_.


Installation using 01.org QAT driver
------------------------------------

Download the latest QuickAssist Technology Driver from `01.org
<https://01.org/packet-processing/intel%C2%AE-quickassist-technology-drivers-and-patches>`_
Consult the *Getting Started Guide* at the same URL for further information.

The steps below assume you are:

* Building on a platform with one ``DH895xCC`` device.
* Using package ``qatmux.l.2.3.0-34.tgz``.
* On Fedora21 kernel ``3.17.4-301.fc21.x86_64``.

In the BIOS ensure that SRIOV is enabled and VT-d is disabled.

Uninstall any existing QAT driver, for example by running:

* ``./installer.sh uninstall`` in the directory where originally installed.

* or ``rmmod qat_dh895xcc; rmmod intel_qat``.

Build and install the SRIOV-enabled QAT driver::

    mkdir /QAT
    cd /QAT
    # copy qatmux.l.2.3.0-34.tgz to this location
    tar zxof qatmux.l.2.3.0-34.tgz

    export ICP_WITHOUT_IOMMU=1
    ./installer.sh install QAT1.6 host

You can use ``cat /proc/icp_dh895xcc_dev0/version`` to confirm the driver is correctly installed.
You can use ``lspci -d:443`` to confirm the bdf of the 32 VF devices are available per ``DH895xCC`` device.

To complete the installation - follow instructions in `Binding the available VFs to the DPDK UIO driver`_.

**Note**: If using a later kernel and the build fails with an error relating to ``strict_stroul`` not being available apply the following patch:

.. code-block:: diff

   /QAT/QAT1.6/quickassist/utilities/downloader/Target_CoreLibs/uclo/include/linux/uclo_platform.h
   + #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,5)
   + #define STR_TO_64(str, base, num, endPtr) {endPtr=NULL; if (kstrtoul((str), (base), (num))) printk("Error strtoull convert %s\n", str); }
   + #else
   #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
   #define STR_TO_64(str, base, num, endPtr) {endPtr=NULL; if (strict_strtoull((str), (base), (num))) printk("Error strtoull convert %s\n", str); }
   #else
   #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
   #define STR_TO_64(str, base, num, endPtr) {endPtr=NULL; strict_strtoll((str), (base), (num));}
   #else
   #define STR_TO_64(str, base, num, endPtr)                                 \
        do {                                                               \
              if (str[0] == '-')                                           \
              {                                                            \
                   *(num) = -(simple_strtoull((str+1), &(endPtr), (base))); \
              }else {                                                      \
                   *(num) = simple_strtoull((str), &(endPtr), (base));      \
              }                                                            \
        } while(0)
   + #endif
   #endif
   #endif


If the build fails due to missing header files you may need to do following:

* ``sudo yum install zlib-devel``
* ``sudo yum install openssl-devel``

If the build or install fails due to mismatching kernel sources you may need to do the following:

* ``sudo yum install kernel-headers-`uname -r```
* ``sudo yum install kernel-src-`uname -r```
* ``sudo yum install kernel-devel-`uname -r```


Installation using kernel.org driver
------------------------------------

Assuming you are running on at least a 4.4 kernel, you can use the stock kernel.org QAT
driver to start the QAT hardware.

The steps below assume you are:

* Running DPDK on a platform with one ``DH895xCC`` device.
* On a kernel at least version 4.4.

In BIOS ensure that SRIOV is enabled and VT-d is disabled.

Ensure the QAT driver is loaded on your system, by executing::

    lsmod | grep qat

You should see the following output::

    qat_dh895xcc            5626  0
    intel_qat              82336  1 qat_dh895xcc

Next, you need to expose the VFs using the sysfs file system.

First find the bdf of the DH895xCC device::

    lspci -d : 435

You should see output similar to::

    03:00.0 Co-processor: Intel Corporation Coleto Creek PCIe Endpoint

Using the sysfs, enable the VFs::

    echo 32 > /sys/bus/pci/drivers/dh895xcc/0000\:03\:00.0/sriov_numvfs

If you get an error, it's likely you're using a QAT kernel driver earlier than kernel 4.4.

To verify that the VFs are available for use - use ``lspci -d:443`` to confirm
the bdf of the 32 VF devices are available per ``DH895xCC`` device.

To complete the installation - follow instructions in `Binding the available VFs to the DPDK UIO driver`_.

**Note**: If the QAT kernel modules are not loaded and you see an error like
    ``Failed to load MMP firmware qat_895xcc_mmp.bin`` this may be as a
    result of not using a distribution, but just updating the kernel directly.

Download firmware from the kernel firmware repo at:
http://git.kernel.org/cgit/linux/kernel/git/firmware/linux-firmware.git/tree/

Copy qat binaries to /lib/firmware:
*    ``cp qat_895xcc.bin /lib/firmware``
*    ``cp qat_895xcc_mmp.bin /lib/firmware``

cd to your linux source root directory and start the qat kernel modules:
*    ``insmod ./drivers/crypto/qat/qat_common/intel_qat.ko``
*    ``insmod ./drivers/crypto/qat/qat_dh895xcc/qat_dh895xcc.ko``

**Note**:The following warning in /var/log/messages can be ignored:
    ``IOMMU should be enabled for SR-IOV to work correctly``



Binding the available VFs to the DPDK UIO driver
------------------------------------------------

The unbind command below assumes ``bdfs`` of ``03:01.00-03:04.07``, if yours are different adjust the unbind command below::

   cd $RTE_SDK
   modprobe uio
   insmod ./build/kmod/igb_uio.ko

   for device in $(seq 1 4); do \
       for fn in $(seq 0 7); do \
           echo -n 0000:03:0${device}.${fn} > \
           /sys/bus/pci/devices/0000\:03\:0${device}.${fn}/driver/unbind; \
       done; \
   done

   echo "8086 0443" > /sys/bus/pci/drivers/igb_uio/new_id

You can use ``lspci -vvd:443`` to confirm that all devices are now in use by igb_uio kernel driver.
