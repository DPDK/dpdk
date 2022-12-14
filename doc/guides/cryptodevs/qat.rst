..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2019 Intel Corporation.

Intel(R) QuickAssist (QAT) Crypto Poll Mode Driver
==================================================

QAT documentation consists of three parts:

* Details of the symmetric and asymmetric crypto services below.
* Details of the :doc:`compression service <../compressdevs/qat_comp>`
  in the compressdev drivers section.
* Details of building the common QAT infrastructure and the PMDs to support the
  above services. See :ref:`building_qat` below.


Symmetric Crypto Service on QAT
-------------------------------

The QAT symmetric crypto PMD (hereafter referred to as `QAT SYM [PMD]`) provides
poll mode crypto driver support for the following hardware accelerator devices:

* ``Intel QuickAssist Technology DH895xCC``
* ``Intel QuickAssist Technology C62x``
* ``Intel QuickAssist Technology C3xxx``
* ``Intel QuickAssist Technology 200xx``
* ``Intel QuickAssist Technology D15xx``
* ``Intel QuickAssist Technology C4xxx``
* ``Intel QuickAssist Technology 4xxx``


Features
~~~~~~~~

The QAT SYM PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_3DES_CTR``
* ``RTE_CRYPTO_CIPHER_AES128_CBC``
* ``RTE_CRYPTO_CIPHER_AES192_CBC``
* ``RTE_CRYPTO_CIPHER_AES256_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CTR``
* ``RTE_CRYPTO_CIPHER_AES192_CTR``
* ``RTE_CRYPTO_CIPHER_AES256_CTR``
* ``RTE_CRYPTO_CIPHER_AES_XTS``
* ``RTE_CRYPTO_CIPHER_SNOW3G_UEA2``
* ``RTE_CRYPTO_CIPHER_NULL``
* ``RTE_CRYPTO_CIPHER_KASUMI_F8``
* ``RTE_CRYPTO_CIPHER_DES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_DOCSISBPI``
* ``RTE_CRYPTO_CIPHER_DES_DOCSISBPI``
* ``RTE_CRYPTO_CIPHER_ZUC_EEA3``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1``
* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_SHA3_224``
* ``RTE_CRYPTO_AUTH_SHA3_256``
* ``RTE_CRYPTO_AUTH_SHA3_384``
* ``RTE_CRYPTO_AUTH_SHA3_512``
* ``RTE_CRYPTO_AUTH_AES_XCBC_MAC``
* ``RTE_CRYPTO_AUTH_SNOW3G_UIA2``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_NULL``
* ``RTE_CRYPTO_AUTH_KASUMI_F9``
* ``RTE_CRYPTO_AUTH_AES_GMAC``
* ``RTE_CRYPTO_AUTH_ZUC_EIA3``
* ``RTE_CRYPTO_AUTH_AES_CMAC``

Supported AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``
* ``RTE_CRYPTO_AEAD_AES_CCM``
* ``RTE_CRYPTO_AEAD_CHACHA20_POLY1305``

Protocol offloads:

* ``RTE_SECURITY_PROTOCOL_DOCSIS``

Supported Chains
~~~~~~~~~~~~~~~~

All the usual chains are supported and also some mixed chains:

.. table:: Supported hash-cipher chains for wireless digest-encrypted cases

   +------------------+-----------+-------------+----------+----------+
   | Cipher algorithm | NULL AUTH | SNOW3G UIA2 | ZUC EIA3 | AES CMAC |
   +==================+===========+=============+==========+==========+
   | NULL CIPHER      | Y         | 2&3         | 2&3      | Y        |
   +------------------+-----------+-------------+----------+----------+
   | SNOW3G UEA2      | 2&3       | 1&2&3       | 2&3      | 2&3      |
   +------------------+-----------+-------------+----------+----------+
   | ZUC EEA3         | 2&3       | 2&3         | 2&3      | 2&3      |
   +------------------+-----------+-------------+----------+----------+
   | AES CTR          | 1&2&3     | 2&3         | 2&3      | Y        |
   +------------------+-----------+-------------+----------+----------+

* The combinations marked as "Y" are supported on all QAT hardware versions.
* The combinations marked as "2&3" are supported on GEN2 and GEN3 QAT hardware only.
* The combinations marked as "1&2&3" are supported on GEN1, GEN2 and GEN3 QAT hardware only.


Limitations
~~~~~~~~~~~

* Only supports the session-oriented API implementation (session-less APIs are not supported).
* SNOW 3G (UEA2), KASUMI (F8) and ZUC (EEA3) supported only if cipher length and offset fields are byte-multiple.
* SNOW 3G (UIA2) and ZUC (EIA3) supported only if hash length and offset fields are byte-multiple.
* No BSD support as BSD QAT kernel driver not available.
* ZUC EEA3/EIA3 is not supported by dh895xcc devices
* Maximum additional authenticated data (AAD) for GCM is 240 bytes long and must be passed to the device in a buffer rounded up to the nearest block-size multiple (x16) and padded with zeros.
* Queue-pairs are thread-safe on Intel CPUs but Queues are not (that is, within a single
  queue-pair all enqueues to the TX queue must be done from one thread and all dequeues
  from the RX queue must be done from one thread, but enqueues and dequeues may be done
  in different threads.)
* A GCM limitation exists, but only in the case where there are multiple
  generations of QAT devices on a single platform.
  To optimise performance, the GCM crypto session should be initialised for the
  device generation to which the ops will be enqueued. Specifically if a GCM
  session is initialised on a GEN2 device, but then attached to an op enqueued
  to a GEN3 device, it will work but cannot take advantage of hardware
  optimisations in the GEN3 device. And if a GCM session is initialised on a
  GEN3 device, then attached to an op sent to a GEN1/GEN2 device, it will not be
  enqueued to the device and will be marked as failed. The simplest way to
  mitigate this is to use the PCI allowlist to avoid mixing devices of different
  generations in the same process if planning to use for GCM.
* The mixed algo feature on GEN2 is not supported by all kernel drivers. Check
  the notes under the Available Kernel Drivers table below for specific details.
* Out-of-place is not supported for combined Crypto-CRC DOCSIS security
  protocol.
* ``RTE_CRYPTO_CIPHER_DES_DOCSISBPI`` is not supported for combined Crypto-CRC
  DOCSIS security protocol.
* Multi-segment buffers are not supported for combined Crypto-CRC DOCSIS
  security protocol.

Extra notes on KASUMI F9
~~~~~~~~~~~~~~~~~~~~~~~~

When using KASUMI F9 authentication algorithm, the input buffer must be
constructed according to the
`3GPP KASUMI specification <http://cryptome.org/3gpp/35201-900.pdf>`_
(section 4.4, page 13). The input buffer has to have COUNT (4 bytes),
FRESH (4 bytes), MESSAGE and DIRECTION (1 bit) concatenated. After the DIRECTION
bit, a single '1' bit is appended, followed by between 0 and 7 '0' bits, so that
the total length of the buffer is multiple of 8 bits. Note that the actual
message can be any length, specified in bits.

Once this buffer is passed this way, when creating the crypto operation,
length of data to authenticate "op.sym.auth.data.length" must be the length
of all the items described above, including the padding at the end.
Also, offset of data to authenticate "op.sym.auth.data.offset"
must be such that points at the start of the COUNT bytes.

Asymmetric Crypto Service on QAT
--------------------------------

The QAT asymmetric crypto PMD (hereafter referred to as `QAT ASYM [PMD]`) provides
poll mode crypto driver support for the following hardware accelerator devices:

* ``Intel QuickAssist Technology DH895xCC``
* ``Intel QuickAssist Technology C62x``
* ``Intel QuickAssist Technology C3xxx``
* ``Intel QuickAssist Technology D15xx``
* ``Intel QuickAssist Technology C4xxx``
* ``Intel QuickAssist Technology 4xxx``
* ``Intel QuickAssist Technology 401xxx``

The QAT ASYM PMD has support for:

* ``RTE_CRYPTO_ASYM_XFORM_MODEX``
* ``RTE_CRYPTO_ASYM_XFORM_MODINV``
* ``RTE_CRYPTO_ASYM_XFORM_RSA``
* ``RTE_CRYPTO_ASYM_XFORM_ECDSA``
* ``RTE_CRYPTO_ASYM_XFORM_ECPM``
* ``RTE_CRYPTO_ASYM_XFORM_ECDH``

Limitations
~~~~~~~~~~~

* Big integers longer than 4096 bits are not supported.
* Queue-pairs are thread-safe on Intel CPUs but Queues are not (that is, within a single
  queue-pair all enqueues to the TX queue must be done from one thread and all dequeues
  from the RX queue must be done from one thread, but enqueues and dequeues may be done
  in different threads.)
* RSA-2560, RSA-3584 are not supported

.. _building_qat:

Building PMDs on QAT
--------------------

A QAT device can host multiple acceleration services:

* symmetric cryptography
* data compression
* asymmetric cryptography

These services are provided to DPDK applications via PMDs which register to
implement the corresponding cryptodev and compressdev APIs. The PMDs use
common QAT driver code which manages the QAT PCI device. They also depend on a
QAT kernel driver being installed on the platform, see :ref:`qat_kernel` below.


Configuring and Building the DPDK QAT PMDs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Further information on configuring, building and installing DPDK is described
:doc:`here <../linux_gsg/build_dpdk>`.

.. _building_qat_config:

Build Configuration
~~~~~~~~~~~~~~~~~~~

These are the build configuration options affecting QAT, and their default values:

.. code-block:: console

	RTE_PMD_QAT_MAX_PCI_DEVICES=48
	RTE_PMD_QAT_COMP_IM_BUFFER_SIZE=65536

Both QAT SYM PMD and QAT ASYM PMD have an external dependency on libcrypto, so are not
built by default.

The QAT compressdev PMD has no external dependencies, so is built by default.

The number of VFs per PF varies - see table below. If multiple QAT packages are
installed on a platform then RTE_PMD_QAT_MAX_PCI_DEVICES should be
adjusted to the number of VFs which the QAT common code will need to handle.

.. Note::

        There are separate config items (not QAT-specific) for max cryptodevs
        RTE_CRYPTO_MAX_DEVS and max compressdevs RTE_COMPRESS_MAX_DEVS,
        if necessary these should be adjusted to handle the total of QAT and other
        devices which the process will use. In particular for crypto, where each
        QAT VF may expose two crypto devices, sym and asym, it may happen that the
        number of devices will be bigger than MAX_DEVS and the process will show an error
        during PMD initialisation. To avoid this problem RTE_CRYPTO_MAX_DEVS may be
        increased or -a, allow domain:bus:devid:func option may be used.


QAT compression PMD needs intermediate buffers to support Deflate compression
with Dynamic Huffman encoding. RTE_PMD_QAT_COMP_IM_BUFFER_SIZE
specifies the size of a single buffer, the PMD will allocate a multiple of these,
plus some extra space for associated meta-data. For GEN2 devices, 20 buffers are
allocated while for GEN1 devices, 12 buffers are allocated, plus 1472 bytes overhead.

.. Note::

	If the compressed output of a Deflate operation using Dynamic Huffman
	Encoding is too big to fit in an intermediate buffer, then the
	operation will be split into smaller operations and their results will
	be merged afterwards.
	This is not possible if any checksum calculation was requested - in such
	case the code falls back to fixed compression.
	To avoid this less performant case, applications should configure
	the intermediate buffer size to be larger than the expected input data size
	(compressed output size is usually unknown, so the only option is to make
	larger than the input size).


Running QAT PMD with minimum threshold for burst size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If only a small number or packets can be enqueued. Each enqueue causes an expensive MMIO write.
These MMIO write occurrences can be optimised by setting any of the following parameters:

- qat_sym_enq_threshold
- qat_asym_enq_threshold
- qat_comp_enq_threshold

When any of these parameters is set rte_cryptodev_enqueue_burst function will
return 0 (thereby avoiding an MMIO) if the device is congested and number of packets
possible to enqueue is smaller.
To use this feature the user must set the parameter on process start as a device additional parameter::

  -a 03:01.1,qat_sym_enq_threshold=32,qat_comp_enq_threshold=16

All parameters can be used with the same device regardless of order. Parameters are separated
by comma. When the same parameter is used more than once first occurrence of the parameter
is used.
Maximum threshold that can be set is 32.

Running QAT PMD with Intel IPSEC MB library for symmetric precomputes function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The QAT PMD use Openssl library for partial hash calculation in symmetirc precomputes function by
default, the following parameter is allow QAT PMD switch over to multi-buffer job API if Intel
IPSEC MB library installed on system.

- qat_ipsec_mb_lib

To use this feature the user must set the parameter on process start as a device additional parameter::

  -a 03:01.1,qat_ipsec_mb_lib=1


Device and driver naming
~~~~~~~~~~~~~~~~~~~~~~~~

* The qat cryptodev symmetric crypto driver name is "crypto_qat".
* The qat cryptodev asymmetric crypto driver name is "crypto_qat_asym".

The "rte_cryptodev_devices_get()" returns the devices exposed by either of these drivers.

* Each qat sym crypto device has a unique name, in format
  "<pci bdf>_<service>", e.g. "0000:41:01.0_qat_sym".
* Each qat asym crypto device has a unique name, in format
  "<pci bdf>_<service>", e.g. "0000:41:01.0_qat_asym".
  This name can be passed to "rte_cryptodev_get_dev_id()" to get the device_id.

.. Note::

	The cryptodev driver name is passed to the dpdk-test-crypto-perf tool in the "-devtype" parameter.

	The qat crypto device name is in the format of the worker parameter passed to the crypto scheduler.

* The qat compressdev driver name is "compress_qat".
  The rte_compressdev_devices_get() returns the devices exposed by this driver.

* Each qat compression device has a unique name, in format
  <pci bdf>_<service>, e.g. "0000:41:01.0_qat_comp".
  This name can be passed to rte_compressdev_get_dev_id() to get the device_id.

.. _qat_kernel:

Dependency on the QAT kernel driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use QAT an SRIOV-enabled QAT kernel driver is required. The VF
devices created and initialised by this driver will be used by the QAT PMDs.

Instructions for installation are below, but first an explanation of the
relationships between the PF/VF devices and the PMDs visible to
DPDK applications.

Each QuickAssist PF device exposes a number of VF devices. Each VF device can
enable one symmetric cryptodev PMD and/or one asymmetric cryptodev PMD and/or
one compressdev PMD.
These QAT PMDs share the same underlying device and pci-mgmt code, but are
enumerated independently on their respective APIs and appear as independent
devices to applications.

.. Note::

   Each VF can only be used by one DPDK process. It is not possible to share
   the same VF across multiple processes, even if these processes are using
   different acceleration services.

   Conversely one DPDK process can use one or more QAT VFs and can expose both
   cryptodev and compressdev instances on each of those VFs.


Available kernel drivers
~~~~~~~~~~~~~~~~~~~~~~~~

Kernel drivers for each device for each service are listed in the following table. (Scroll right
to see the full table)


.. _table_qat_pmds_drivers:

.. table:: QAT device generations, devices and drivers

   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | S   | A   | C   | Gen | Device   | Driver/ver    | Kernel Module | Pci Driver | PF Did | #PFs | VF Did | VFs/PF |
   +=====+=====+=====+=====+==========+===============+===============+============+========+======+========+========+
   | Yes | No  | No  | 1   | DH895xCC | linux/4.4+    | qat_dh895xcc  | dh895xcc   | 435    | 1    | 443    | 32     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | No  | "   | "        | IDZ/4.12.0+   | "             | "          | "      | "    | "      | "      |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | Yes | "   | "        | IDZ/4.13.0+   | "             | "          | "      | "    | "      | "      |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | No  | No  | 2   | C62x     | linux/4.5+    | qat_c62x      | c6xx       | 37c8   | 3    | 37c9   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | Yes | "   | "        | IDZ/4.12.0+   | "             | "          | "      | "    | "      | "      |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | No  | No  | 2   | C3xxx    | linux/4.5+    | qat_c3xxx     | c3xxx      | 19e2   | 1    | 19e3   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | Yes | "   | "        | IDZ/4.12.0+   | "             | "          | "      | "    | "      | "      |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | No  | No  | 2   | 200xx    | p             | qat_200xx     | 200xx      | 18ee   | 1    | 18ef   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | No  | No  | 2   | D15xx    | p             | qat_d15xx     | d15xx      | 6f54   | 1    | 6f55   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | No  | 3   | C4xxx    | p             | qat_c4xxx     | c4xxx      | 18a0   | 1    | 18a1   | 128    |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | No  | 4   | 4xxx     | linux/5.11+   | qat_4xxx      | 4xxx       | 4940   | 4    | 4941   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | Yes | 4   | 4xxx     | linux/5.17+   | qat_4xxx      | 4xxx       | 4940   | 4    | 4941   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | No  | No  | 4   | 4xxx     | IDZ/ N/A      | qat_4xxx      | 4xxx       | 4940   | 4    | 4941   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | Yes | Yes | 4   | 401xxx   | linux/5.19+   | qat_401xxx    | 4xxx       | 4942   | 2    | 4943   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+
   | Yes | No  | No  | 4   | 401xxx   | IDZ/ N/A      | qat_401xxx    | 4xxx       | 4942   | 2    | 4943   | 16     |
   +-----+-----+-----+-----+----------+---------------+---------------+------------+--------+------+--------+--------+

* Note: Symmetric mixed crypto algorithms feature on Gen 2 works only with IDZ driver version 4.9.0+

The first 3 columns indicate the service:

* S = Symmetric crypto service (via cryptodev API)
* A = Asymmetric crypto service  (via cryptodev API)
* C = Compression service (via compressdev API)

The ``Driver`` column indicates either the Linux kernel version in which
support for this device was introduced or a driver available on Intel Developer Zone (IDZ).
There are both linux in-tree and IDZ kernel drivers available for some
devices. p = release pending.

If you are running on a kernel which includes a driver for your device, see
`Installation using kernel.org driver`_ below. Otherwise see
`Installation using IDZ QAT driver`_.

.. note::

   The asymmetric service is not supported by DPDK QAT PMD for the Gen 3 platform.
   The actual crypto services enabled on the system depend
   on QAT driver capabilities and hardware slice configuration.

Installation using kernel.org driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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

    lspci -d:37c8

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
`Binding the available VFs to the vfio-pci driver`_.

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


Installation using IDZ QAT driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Download the latest QuickAssist Technology Driver from `Intel Developer Zone
<https://developer.intel.com/quickassist>`_.
Consult the *Quick Start Guide* at the same URL for further information.

The steps below assume you are:

* Building on a platform with one ``C62x`` device.
* Using package ``qat1.7.l.4.2.0-000xx.tar.gz``.
* On Fedora26 kernel ``4.11.11-300.fc26.x86_64``.

In the BIOS ensure that SRIOV is enabled and VT-d is disabled.

Uninstall any existing QAT driver, for example by running:

* ``./installer.sh uninstall`` in the directory where originally installed.


Build and install the SRIOV-enabled QAT driver::

    mkdir /QAT
    cd /QAT

    # Copy the package to this location and unpack
    tar zxof qat1.7.l.4.2.0-000xx.tar.gz

    ./configure --enable-icp-sriov=host
    make install

You can use ``cat /sys/kernel/debug/qat<your device type and bdf>/version/fw`` to confirm the driver is correctly installed and is using firmware version 4.2.0.
You can use ``lspci -d:37c9`` to confirm the presence of the 16 VF devices available per ``C62x`` PF.

Confirm the driver is correctly installed and is using firmware version 4.2.0::

    cat /sys/kernel/debug/qat<your device type and bdf>/version/fw


Confirm the presence of 48 VF devices - 16 per PF::

    lspci -d:37c9


To complete the installation - follow instructions in
`Binding the available VFs to the vfio-pci driver`_.

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
      sudo yum install libudev-devel

.. Note::

   If the build or install fails due to mismatching kernel sources you may need to do the following::

      sudo yum install kernel-headers-`uname -r`
      sudo yum install kernel-src-`uname -r`
      sudo yum install kernel-devel-`uname -r`

.. Note::

   If the build fails on newer GCC versions (such as GCC 12) with an error relating to
   ``-lc`` not being found, apply the following patch:

   .. code-block:: diff

      /QAT/quickassist/lookaside/access_layer/src/Makefile
      cd $(ICP_FINAL_OUTPUT_DIR);\
      cmd="$(LINKER) $(LIB_SHARED_FLAGS) -o \
        $(LIB_SHARED) $(ADDITIONAL_OBJECTS) $(ADDITIONAL_LIBS) *.o -lpthread -ludev \
      - -Bstatic -L$(ADF_DIR)/src/build/$(ICP_OS)/$(ICP_OS_LEVEL) \
      - -ladf_user -L$(OSAL_DIR)/src/build/$(ICP_OS)/$(ICP_OS_LEVEL)/ \
      - -losal -Bdynamic -lc"; \
      + -Bstatic -L$(ADF_DIR)/src/build/$(ICP_OS)/$(ICP_OS_LEVEL) \
      + -ladf_user -L$(OSAL_DIR)/src/build/$(ICP_OS)/$(ICP_OS_LEVEL)/ \
      + -losal -Bdynamic -L/lib/x86_64-linux-gnu/ -lc"; \
      echo "$$cmd"; \
      $$cmd

   Followed by this patch:

   .. code-block:: diff

      /QAT/quickassist/build_system/build_files/OS/linux_common_user_space_rules.mk
      @echo 'Creating shared library ${LIB_SHARED}'; \
      cd $($(PROG_ACY)_FINAL_OUTPUT_DIR);\
      -  echo $(LINKER) $(LIB_SHARED_FLAGS) -o $@  $(OBJECTS) $(ADDITIONAL_OBJECTS) -lc;\
      -  $(LINKER) $(LIB_SHARED_FLAGS) -o $@  $(OBJECTS) $(ADDITIONAL_OBJECTS) -lc ;
      +  echo $(LINKER) $(LIB_SHARED_FLAGS) -o $@  $(OBJECTS) $(ADDITIONAL_OBJECTS) \
      +  -L/lib/x86_64-linux-gnu/ -lc;\
      +  $(LINKER) $(LIB_SHARED_FLAGS) -o $@  $(OBJECTS) $(ADDITIONAL_OBJECTS) \
      +  -L/lib/x86_64-linux-gnu/ -lc ;


Binding the available VFs to the vfio-pci driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Note:

* Please note that due to security issues, the usage of older DPDK igb_uio
  driver is not recommended. This document shows how to use the more secure
  vfio-pci driver.
* If QAT fails to bind to vfio-pci on Linux kernel 5.9+, please see the
  QATE-39220 and QATE-7495 issues in
  `IDZ doc <https://cdrdv2.intel.com/v1/dl/getContent/710057?explicitVersion=true>`_
  which details the constraint about trusted guests and add `disable_denylist=1`
  to the vfio-pci params to use QAT. See also `this patch description <https://lkml.org/lkml/2020/7/23/1155>`_.

Unbind the VFs from the stock driver so they can be bound to the vfio-pci driver.

For an Intel(R) QuickAssist Technology DH895xCC device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The unbind command below assumes ``BDFs`` of ``03:01.00-03:04.07``, if your
VFs are different adjust the unbind command below::

    cd to the top-level DPDK directory
    for device in $(seq 1 4); do \
        for fn in $(seq 0 7); do \
            usertools/dpdk-devbind.py -u 0000:03:0${device}.${fn}; \
        done; \
    done

For an Intel(R) QuickAssist Technology C62x device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The unbind command below assumes ``BDFs`` of ``1a:01.00-1a:02.07``,
``3d:01.00-3d:02.07`` and ``3f:01.00-3f:02.07``, if your VFs are different
adjust the unbind command below::

    cd to the top-level DPDK directory
    for device in $(seq 1 2); do \
        for fn in $(seq 0 7); do \
            usertools/dpdk-devbind.py -u 0000:1a:0${device}.${fn}; \
            usertools/dpdk-devbind.py -u 0000:3d:0${device}.${fn}; \
            usertools/dpdk-devbind.py -u 0000:3f:0${device}.${fn}; \
        done; \
    done

For Intel(R) QuickAssist Technology C3xxx or 200xx or D15xx device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The unbind command below assumes ``BDFs`` of ``01:01.00-01:02.07``, if your
VFs are different adjust the unbind command below::

    cd to the top-level DPDK directory
    for device in $(seq 1 2); do \
        for fn in $(seq 0 7); do \
            usertools/dpdk-devbind.py -u 0000:01:0${device}.${fn}; \
        done; \
    done

Bind to the vfio-pci driver
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Load the vfio-pci driver, bind the VF PCI Device id to it using the
``dpdk-devbind.py`` script then use the ``--status`` option
to confirm the VF devices are now in use by vfio-pci kernel driver,
e.g. for the C62x device::

    cd to the top-level DPDK directory
    modprobe vfio-pci
    usertools/dpdk-devbind.py -b vfio-pci 0000:03:01.1
    usertools/dpdk-devbind.py --status

Use ``modprobe vfio-pci disable_denylist=1`` from kernel 5.9 onwards.
See note in the section `Binding the available VFs to the vfio-pci driver`_
above.

Testing
~~~~~~~

QAT SYM crypto PMD can be tested by running the test application::

    cd ./<build_dir>/app/test
    ./dpdk-test -l1 -n1 -a <your qat bdf>
    RTE>>cryptodev_qat_autotest

QAT ASYM crypto PMD can be tested by running the test application::

    cd ./<build_dir>/app/test
    ./dpdk-test -l1 -n1 -a <your qat bdf>
    RTE>>cryptodev_qat_asym_autotest

QAT compression PMD can be tested by running the test application::

    cd ./<build_dir>/app/test
    ./dpdk-test -l1 -n1 -a <your qat bdf>
    RTE>>compressdev_autotest


Debugging
~~~~~~~~~

There are 2 sets of trace available via the dynamic logging feature:

* pmd.qat.dp exposes trace on the data-path.
* pmd.qat.general exposes all other trace.

pmd.qat exposes both sets of traces.
They can be enabled using the log-level option (where 8=maximum log level) on
the process cmdline, e.g. using any of the following::

    --log-level="pmd.qat.general,8"
    --log-level="pmd.qat.dp,8"
    --log-level="pmd.qat,8"

.. Note::

    The global RTE_LOG_DP_LEVEL overrides data-path trace so must be set to
    RTE_LOG_DEBUG to see all the trace. This variable is in config/rte_config.h
    for meson build.
    Also the dynamic global log level overrides both sets of trace, so e.g. no
    QAT trace would display in this case::

	--log-level="7" --log-level="pmd.qat.general,8"
