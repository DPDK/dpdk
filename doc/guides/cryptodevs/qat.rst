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

Intel(R) QuickAssist (QAT) Crypto Poll Mode Driver
==================================================

The QAT PMD provides poll mode crypto driver support for **Intel QuickAssist
Technology DH895xxC**, **Intel QuickAssist Technology C62x** and
**Intel QuickAssist Technology C3xxx** hardware accelerator.


Features
--------

The QAT PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_3DES_CTR``
* ``RTE_CRYPTO_CIPHER_AES128_CBC``
* ``RTE_CRYPTO_CIPHER_AES192_CBC``
* ``RTE_CRYPTO_CIPHER_AES256_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CTR``
* ``RTE_CRYPTO_CIPHER_AES192_CTR``
* ``RTE_CRYPTO_CIPHER_AES256_CTR``
* ``RTE_CRYPTO_CIPHER_SNOW3G_UEA2``
* ``RTE_CRYPTO_CIPHER_AES_GCM``
* ``RTE_CRYPTO_CIPHER_NULL``
* ``RTE_CRYPTO_CIPHER_KASUMI_F8``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_AES_XCBC_MAC``
* ``RTE_CRYPTO_AUTH_SNOW3G_UIA2``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_NULL``
* ``RTE_CRYPTO_AUTH_KASUMI_F9``
* ``RTE_CRYPTO_AUTH_AES_GMAC``


Limitations
-----------

* Chained mbufs are not supported.
* Hash only is not supported except SNOW 3G UIA2 and KASUMI F9.
* Cipher only is not supported except SNOW 3G UEA2, KASUMI F8 and 3DES.
* Only supports the session-oriented API implementation (session-less APIs are not supported).
* SNOW 3G (UEA2) and KASUMI (F8) supported only if cipher length, cipher offset fields are byte-aligned.
* SNOW 3G (UIA2) and KASUMI (F9) supported only if hash length, hash offset fields are byte-aligned.
* No BSD support as BSD QAT kernel driver not available.


Installation
------------

To use the DPDK QAT PMD an SRIOV-enabled QAT kernel driver is required. The
VF devices exposed by this driver will be used by QAT PMD.

To enable QAT in DPDK, follow the instructions mentioned in
http://dpdk.org/doc/guides/linux_gsg/build_dpdk.html

Quick instructions as follows:

.. code-block:: console

	make config T=x86_64-native-linuxapp-gcc
	sed -i 's,\(CONFIG_RTE_LIBRTE_PMD_QAT\)=n,\1=y,' build/.config
	make

If you are running on kernel 4.4 or greater, see instructions for
`Installation using kernel.org driver`_ below. If you are on a kernel earlier
than 4.4, see `Installation using 01.org QAT driver`_.

For **Intel QuickAssist Technology C62x** and **Intel QuickAssist Technology C3xxx**
device, kernel 4.5 or greater is needed.
See instructions for `Installation using kernel.org driver`_ below.


Installation using 01.org QAT driver
------------------------------------

NOTE: There is no driver available for **Intel QuickAssist Technology C62x** and
**Intel QuickAssist Technology C3xxx** devices on 01.org.

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

For **Intel QuickAssist Technology DH895xxC**:

Assuming you are running on at least a 4.4 kernel, you can use the stock kernel.org QAT
driver to start the QAT hardware.

The steps below assume you are:

* Running DPDK on a platform with one ``DH895xCC`` device.
* On a kernel at least version 4.4.

In BIOS ensure that SRIOV is enabled and either
a) disable VT-d or
b) enable VT-d and set ``"intel_iommu=on iommu=pt"`` in the grub file.

Ensure the QAT driver is loaded on your system, by executing::

    lsmod | grep qat

You should see the following output::

    qat_dh895xcc            5626  0
    intel_qat              82336  1 qat_dh895xcc

Next, you need to expose the Virtual Functions (VFs) using the sysfs file system.

First find the bdf of the physical function (PF) of the DH895xCC device::

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

For **Intel QuickAssist Technology C62x**:
Assuming you are running on at least a 4.5 kernel, you can use the stock kernel.org QAT
driver to start the QAT hardware.

The steps below assume you are:

* Running DPDK on a platform with one ``C62x`` device.
* On a kernel at least version 4.5.

In BIOS ensure that SRIOV is enabled and either
a) disable VT-d or
b) enable VT-d and set ``"intel_iommu=on iommu=pt"`` in the grub file.

Ensure the QAT driver is loaded on your system, by executing::

    lsmod | grep qat

You should see the following output::

    qat_c62x               16384  0
    intel_qat             122880  1 qat_c62x

Next, you need to expose the VFs using the sysfs file system.

First find the bdf of the C62x device::

    lspci -d:37c8

You should see output similar to::

    1a:00.0 Co-processor: Intel Corporation Device 37c8
    3d:00.0 Co-processor: Intel Corporation Device 37c8
    3f:00.0 Co-processor: Intel Corporation Device 37c8

For each c62x device there are 3 PFs.
Using the sysfs, for each PF, enable the 16 VFs::

    echo 16 > /sys/bus/pci/drivers/c6xx/0000\:1a\:00.0/sriov_numvfs

If you get an error, it's likely you're using a QAT kernel driver earlier than kernel 4.5.

To verify that the VFs are available for use - use ``lspci -d:37c9`` to confirm
the bdf of the 48 VF devices are available per ``C62x`` device.

To complete the installation - follow instructions in `Binding the available VFs to the DPDK UIO driver`_.

For **Intel QuickAssist Technology C3xxx**:
Assuming you are running on at least a 4.5 kernel, you can use the stock kernel.org QAT
driver to start the QAT hardware.

The steps below assume you are:

* Running DPDK on a platform with one ``C3xxx`` device.
* On a kernel at least version 4.5.

In BIOS ensure that SRIOV is enabled and either
a) disable VT-d or
b) enable VT-d and set ``"intel_iommu=on iommu=pt"`` in the grub file.

Ensure the QAT driver is loaded on your system, by executing::

    lsmod | grep qat

You should see the following output::

    qat_c3xxx               16384  0
    intel_qat             122880  1 qat_c3xxx

Next, you need to expose the Virtual Functions (VFs) using the sysfs file system.

First find the bdf of the physical function (PF) of the C3xxx device

    lspci -d:19e2

You should see output similar to::

    01:00.0 Co-processor: Intel Corporation Device 19e2

For c3xxx device there is 1 PFs.
Using the sysfs, enable the 16 VFs::

    echo 16 > /sys/bus/pci/drivers/c3xxx/0000\:01\:00.0/sriov_numvfs

If you get an error, it's likely you're using a QAT kernel driver earlier than kernel 4.5.

To verify that the VFs are available for use - use ``lspci -d:19e3`` to confirm
the bdf of the 16 VF devices are available per ``C3xxx`` device.
To complete the installation - follow instructions in `Binding the available VFs to the DPDK UIO driver`_.

Binding the available VFs to the DPDK UIO driver
------------------------------------------------

For **Intel(R) QuickAssist Technology DH895xcc** device:
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

For **Intel(R) QuickAssist Technology C62x** device:
The unbind command below assumes ``bdfs`` of ``1a:01.00-1a:02.07``, ``3d:01.00-3d:02.07`` and ``3f:01.00-3f:02.07``,
if yours are different adjust the unbind command below::

   cd $RTE_SDK
   modprobe uio
   insmod ./build/kmod/igb_uio.ko

   for device in $(seq 1 2); do \
       for fn in $(seq 0 7); do \
           echo -n 0000:1a:0${device}.${fn} > \
           /sys/bus/pci/devices/0000\:1a\:0${device}.${fn}/driver/unbind; \

           echo -n 0000:3d:0${device}.${fn} > \
           /sys/bus/pci/devices/0000\:3d\:0${device}.${fn}/driver/unbind; \

           echo -n 0000:3f:0${device}.${fn} > \
           /sys/bus/pci/devices/0000\:3f\:0${device}.${fn}/driver/unbind; \
       done; \
   done

   echo "8086 37c9" > /sys/bus/pci/drivers/igb_uio/new_id

You can use ``lspci -vvd:37c9`` to confirm that all devices are now in use by igb_uio kernel driver.

For **Intel(R) QuickAssist Technology C3xxx** device:
The unbind command below assumes ``bdfs`` of ``01:01.00-01:02.07``,
if yours are different adjust the unbind command below::

   cd $RTE_SDK
   modprobe uio
   insmod ./build/kmod/igb_uio.ko

   for device in $(seq 1 2); do \
       for fn in $(seq 0 7); do \
           echo -n 0000:01:0${device}.${fn} > \
           /sys/bus/pci/devices/0000\:01\:0${device}.${fn}/driver/unbind; \

       done; \
   done

   echo "8086 19e3" > /sys/bus/pci/drivers/igb_uio/new_id

You can use ``lspci -vvd:19e3`` to confirm that all devices are now in use by igb_uio kernel driver.


The other way to bind the VFs to the DPDK UIO driver is by using the ``dpdk-devbind.py`` script:

.. code-block:: console

    cd $RTE_SDK
    ./tools/dpdk-devbind.py -b igb_uio 0000:03:01.1
