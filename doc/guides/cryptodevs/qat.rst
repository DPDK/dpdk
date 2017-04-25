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

The QAT PMD provides poll mode crypto driver support for the following
hardware accelerator devices:

* ``Intel QuickAssist Technology DH895xCC``
* ``Intel QuickAssist Technology C62x``
* ``Intel QuickAssist Technology C3xxx``
* ``Intel QuickAssist Technology D15xx``


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
* ``RTE_CRYPTO_CIPHER_DES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_DOCSISBPI``
* ``RTE_CRYPTO_CIPHER_DES_DOCSISBPI``
* ``RTE_CRYPTO_CIPHER_ZUC_EEA3``

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
* ``RTE_CRYPTO_AUTH_ZUC_EIA3``


Limitations
-----------

* Hash only is not supported except SNOW 3G UIA2 and KASUMI F9.
* Only supports the session-oriented API implementation (session-less APIs are not supported).
* SNOW 3G (UEA2) and KASUMI (F8) supported only if cipher length, cipher offset fields are byte-aligned.
* SNOW 3G (UIA2) and KASUMI (F9) supported only if hash length, hash offset fields are byte-aligned.
* No BSD support as BSD QAT kernel driver not available.
* ZUC EEA3/EIA3 is not supported by dh895xcc devices
* Maximum additional authenticated data (AAD) for GCM is 240 bytes long.


Installation
------------

To enable QAT in DPDK, follow the instructions for modifying the compile-time
configuration file as described `here <http://dpdk.org/doc/guides/linux_gsg/build_dpdk.html>`_.

Quick instructions are as follows:

.. code-block:: console

	cd to the top-level DPDK directory
	make config T=x86_64-native-linuxapp-gcc
	sed -i 's,\(CONFIG_RTE_LIBRTE_PMD_QAT\)=n,\1=y,' build/.config
	make

To use the DPDK QAT PMD an SRIOV-enabled QAT kernel driver is required. The VF
devices exposed by this driver will be used by the QAT PMD. The devices and
available kernel drivers and device ids are :

.. _table_qat_pmds_drivers:

.. table:: QAT devices and drivers

   +----------+--------+---------------+------------+--------+---------+--------+------------+
   | Device   | Driver | Kernel Module | Pci Driver | PF Did | Num PFs | Vf Did | VFs per PF |
   +==========+========+===============+============+========+=========+========+============+
   | DH895xCC | 01.org | icp_qa_al     | n/a        | 435    | 1       | 443    | 32         |
   +----------+--------+---------------+------------+--------+---------+--------+------------+
   | DH895xCC | 4.4+   | qat_dh895xcc  | dh895xcc   | 435    | 1       | 443    | 32         |
   +----------+--------+---------------+------------+--------+---------+--------+------------+
   | C62x     | 4.5+   | qat_c62x      | c6xx       | 37c8   | 3       | 37c9   | 16         |
   +----------+--------+---------------+------------+--------+---------+--------+------------+
   | C3xxx    | 4.5+   | qat_c3xxx     | c3xxx      | 19e2   | 1       | 19e3   | 16         |
   +----------+--------+---------------+------------+--------+---------+--------+------------+
   | D15xx    | p      | qat_d15xx     | d15xx      | 6f54   | 1       | 6f55   | 16         |
   +----------+--------+---------------+------------+--------+---------+--------+------------+


The ``Driver`` column indicates either the Linux kernel version in which
support for this device was introduced or a driver available on Intel's 01.org
website. There are both linux and 01.org kernel drivers available for some
devices. p = release pending.

If you are running on a kernel which includes a driver for your device, see
`Installation using kernel.org driver`_ below. Otherwise see
`Installation using 01.org QAT driver`_.


Installation using kernel.org driver
------------------------------------

The examples below are based on the C62x device, if you have a different device
use the corresponding values in the above table.

In BIOS ensure that SRIOV is enabled and either:

* Disable VT-d or
* Enable VT-d and set ``"intel_iommu=on iommu=pt"`` in the grub file.

Check that the QAT driver is loaded on your system, by executing::

    lsmod | grep qa

You should see the kernel module for your device listed, e.g.::

    qat_c62x               5626  0
    intel_qat              82336  1 qat_c62x

Next, you need to expose the Virtual Functions (VFs) using the sysfs file system.

First find the BDFs (Bus-Device-Function) of the physical functions (PFs) of
your device, e.g.::

    lspci -d : 37c8

You should see output similar to::

    1a:00.0 Co-processor: Intel Corporation Device 37c8
    3d:00.0 Co-processor: Intel Corporation Device 37c8
    3f:00.0 Co-processor: Intel Corporation Device 37c8

Enable the VFs for each PF by echoing the number of VFs per PF to the pci driver::

     echo 16 > /sys/bus/pci/drivers/c6xx/0000:1a:00.0/sriov_numvfs
     echo 16 > /sys/bus/pci/drivers/c6xx/0000:3d:00.0/sriov_numvfs
     echo 16 > /sys/bus/pci/drivers/c6xx/0000:3f:00.0/sriov_numvfs

Check that the VFs are available for use. For example ``lspci -d:37c9`` should
list 48 VF devices available for a ``C62x`` device.

To complete the installation follow the instructions in
`Binding the available VFs to the DPDK UIO driver`_.

.. Note::

   If the QAT kernel modules are not loaded and you see an error like ``Failed
   to load MMP firmware qat_895xcc_mmp.bin`` in kernel logs, this may be as a
   result of not using a distribution, but just updating the kernel directly.

   Download firmware from the `kernel firmware repo
   <http://git.kernel.org/cgit/linux/kernel/git/firmware/linux-firmware.git/tree/>`_.

   Copy qat binaries to ``/lib/firmware``::

      cp qat_895xcc.bin /lib/firmware
      cp qat_895xcc_mmp.bin /lib/firmware

   Change to your linux source root directory and start the qat kernel modules::

      insmod ./drivers/crypto/qat/qat_common/intel_qat.ko
      insmod ./drivers/crypto/qat/qat_dh895xcc/qat_dh895xcc.ko


.. Note::

   If you see the following warning in ``/var/log/messages`` it can be ignored:
   ``IOMMU should be enabled for SR-IOV to work correctly``.


Installation using 01.org QAT driver
------------------------------------

Download the latest QuickAssist Technology Driver from `01.org
<https://01.org/packet-processing/intel%C2%AE-quickassist-technology-drivers-and-patches>`_.
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

    # Copy qatmux.l.2.3.0-34.tgz to this location
    tar zxof qatmux.l.2.3.0-34.tgz

    export ICP_WITHOUT_IOMMU=1
    ./installer.sh install QAT1.6 host

You can use ``cat /proc/icp_dh895xcc_dev0/version`` to confirm the driver is correctly installed.
You can use ``lspci -d:443`` to confirm the  of the 32 VF devices available per ``DH895xCC`` device.

To complete the installation - follow instructions in `Binding the available VFs to the DPDK UIO driver`_.

.. Note::

   If using a later kernel and the build fails with an error relating to
   ``strict_stroul`` not being available apply the following patch:

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


.. Note::

   If the build fails due to missing header files you may need to do following::

      sudo yum install zlib-devel
      sudo yum install openssl-devel

.. Note::

   If the build or install fails due to mismatching kernel sources you may need to do the following::

      sudo yum install kernel-headers-`uname -r`
      sudo yum install kernel-src-`uname -r`
      sudo yum install kernel-devel-`uname -r`


Binding the available VFs to the DPDK UIO driver
------------------------------------------------

Unbind the VFs from the stock driver so they can be bound to the uio driver.

For an Intel(R) QuickAssist Technology DH895xCC device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The unbind command below assumes ``BDFs`` of ``03:01.00-03:04.07``, if your
VFs are different adjust the unbind command below::

    for device in $(seq 1 4); do \
        for fn in $(seq 0 7); do \
            echo -n 0000:03:0${device}.${fn} > \
            /sys/bus/pci/devices/0000\:03\:0${device}.${fn}/driver/unbind; \
        done; \
    done

For an Intel(R) QuickAssist Technology C62x device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The unbind command below assumes ``BDFs`` of ``1a:01.00-1a:02.07``,
``3d:01.00-3d:02.07`` and ``3f:01.00-3f:02.07``, if your VFs are different
adjust the unbind command below::

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

For Intel(R) QuickAssist Technology C3xxx or D15xx device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The unbind command below assumes ``BDFs`` of ``01:01.00-01:02.07``, if your
VFs are different adjust the unbind command below::

    for device in $(seq 1 2); do \
        for fn in $(seq 0 7); do \
            echo -n 0000:01:0${device}.${fn} > \
            /sys/bus/pci/devices/0000\:01\:0${device}.${fn}/driver/unbind; \
        done; \
    done

Bind to the DPDK uio driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install the DPDK igb_uio driver, bind the VF PCI Device id to it and use lspci
to confirm the VF devices are now in use by igb_uio kernel driver,
e.g. for the C62x device::

    cd to the top-level DPDK directory
    modprobe uio
    insmod ./build/kmod/igb_uio.ko
    echo "8086 37c9" > /sys/bus/pci/drivers/igb_uio/new_id
    lspci -vvd:37c9


Another way to bind the VFs to the DPDK UIO driver is by using the
``dpdk-devbind.py`` script::

    cd to the top-level DPDK directory
    ./usertools/dpdk-devbind.py -b igb_uio 0000:03:01.1
