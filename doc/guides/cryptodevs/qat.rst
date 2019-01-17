..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2016 Intel Corporation.

Intel(R) QuickAssist (QAT) Crypto Poll Mode Driver
==================================================

QAT documentation consists of three parts:

* Details of the symmetric crypto service below.
* Details of the `compression service <http://doc.dpdk.org/guides/compressdevs/qat_comp.html>`_
  in the compressdev drivers section.
* Details of building the common QAT infrastructure and the PMDs to support the
  above services. See :ref:`building_qat` below.


Symmetric Crypto Service on QAT
-------------------------------

The QAT crypto PMD provides poll mode crypto driver support for the following
hardware accelerator devices:

* ``Intel QuickAssist Technology DH895xCC``
* ``Intel QuickAssist Technology C62x``
* ``Intel QuickAssist Technology C3xxx``
* ``Intel QuickAssist Technology D15xx``
* ``Intel QuickAssist Technology C4xxx``


Features
~~~~~~~~

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
* ``RTE_CRYPTO_AUTH_AES_CMAC``

Supported AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``
* ``RTE_CRYPTO_AEAD_AES_CCM``


Limitations
~~~~~~~~~~~

* Only supports the session-oriented API implementation (session-less APIs are not supported).
* SNOW 3G (UEA2), KASUMI (F8) and ZUC (EEA3) supported only if cipher length and offset fields are byte-multiple.
* SNOW 3G (UIA2) and ZUC (EIA3) supported only if hash length and offset fields are byte-multiple.
* No BSD support as BSD QAT kernel driver not available.
* ZUC EEA3/EIA3 is not supported by dh895xcc devices
* Maximum additional authenticated data (AAD) for GCM is 240 bytes long and must be passed to the device in a buffer rounded up to the nearest block-size multiple (x16) and padded with zeros.
* Queue pairs are not thread-safe (that is, within a single queue pair, RX and TX from different lcores is not supported).

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



.. _building_qat:

Building PMDs on QAT
--------------------

A QAT device can host multiple acceleration services:

* symmetric cryptography
* data compression

These services are provided to DPDK applications via PMDs which register to
implement the corresponding cryptodev and compressdev APIs. The PMDs use
common QAT driver code which manages the QAT PCI device. They also depend on a
QAT kernel driver being installed on the platform, see :ref:`qat_kernel` below.


Configuring and Building the DPDK QAT PMDs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Further information on configuring, building and installing DPDK is described
`here <http://doc.dpdk.org/guides/linux_gsg/build_dpdk.html>`_.


Quick instructions for QAT cryptodev PMD are as follows:

.. code-block:: console

	cd to the top-level DPDK directory
	make defconfig
	sed -i 's,\(CONFIG_RTE_LIBRTE_PMD_QAT_SYM\)=n,\1=y,' build/.config
	make

Quick instructions for QAT compressdev PMD are as follows:

.. code-block:: console

	cd to the top-level DPDK directory
	make defconfig
	make


Build Configuration
~~~~~~~~~~~~~~~~~~~

These are the build configuration options affecting QAT, and their default values:

.. code-block:: console

	CONFIG_RTE_LIBRTE_PMD_QAT=y
	CONFIG_RTE_LIBRTE_PMD_QAT_SYM=n
	CONFIG_RTE_PMD_QAT_MAX_PCI_DEVICES=48
	CONFIG_RTE_PMD_QAT_COMP_SGL_MAX_SEGMENTS=16
	CONFIG_RTE_PMD_QAT_COMP_IM_BUFFER_SIZE=65536

CONFIG_RTE_LIBRTE_PMD_QAT must be enabled for any QAT PMD to be built.

The QAT cryptodev PMD has an external dependency on libcrypto, so is not
built by default. CONFIG_RTE_LIBRTE_PMD_QAT_SYM should be enabled to build it.

The QAT compressdev PMD has no external dependencies, so needs no configuration
options and is built by default.

The number of VFs per PF varies - see table below. If multiple QAT packages are
installed on a platform then CONFIG_RTE_PMD_QAT_MAX_PCI_DEVICES should be
adjusted to the number of VFs which the QAT common code will need to handle.
Note, there are separate config items for max cryptodevs CONFIG_RTE_CRYPTO_MAX_DEVS
and max compressdevs CONFIG_RTE_COMPRESS_MAX_DEVS, if necessary these should be
adjusted to handle the total of QAT and other devices which the process will use.

QAT allocates internal structures to handle SGLs. For the compression service
CONFIG_RTE_PMD_QAT_COMP_SGL_MAX_SEGMENTS can be changed if more segments are needed.
An extra (max_inflight_ops x 16) bytes per queue_pair will be used for every increment.

QAT compression PMD needs intermediate buffers to support Deflate compression
with Dynamic Huffman encoding. CONFIG_RTE_PMD_QAT_COMP_IM_BUFFER_SIZE
specifies the size of a single buffer, the PMD will allocate a multiple of these,
plus some extra space for associated meta-data. For GEN2 devices, 20 buffers plus
1472 bytes are allocated.

.. Note::

	If the compressed output of a Deflate operation using Dynamic Huffman
        Encoding is too big to fit in an intermediate buffer, then the
        operation will return RTE_COMP_OP_STATUS_ERROR and an error will be
        displayed. Options for the application in this case
        are to split the input data into smaller chunks and resubmit
        in multiple operations or to configure QAT with
        larger intermediate buffers.


Device and driver naming
~~~~~~~~~~~~~~~~~~~~~~~~

* The qat cryptodev driver name is "crypto_qat".
  The "rte_cryptodev_devices_get()" returns the devices exposed by this driver.

* Each qat crypto device has a unique name, in format
  "<pci bdf>_<service>", e.g. "0000:41:01.0_qat_sym".
  This name can be passed to "rte_cryptodev_get_dev_id()" to get the device_id.

.. Note::

	The qat crypto driver name is passed to the dpdk-test-crypto-perf tool in the "-devtype" parameter.

	The qat crypto device name is in the format of the slave parameter passed to the crypto scheduler.

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
enable one cryptodev PMD and/or one compressdev PMD.
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

Kernel drivers for each device are listed in the following table. Scroll right
to check that the driver and device supports the service you require.


.. _table_qat_pmds_drivers:

.. table:: QAT device generations, devices and drivers

   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | Gen | Device   | Driver/ver    | Kernel Module | Pci Driver | PF Did | #PFs | VF Did | VFs/PF | cryptodev | compressdev |
   +=====+==========+===============+===============+============+========+======+========+========+===========+=============+
   | 1   | DH895xCC | linux/4.4+    | qat_dh895xcc  | dh895xcc   | 435    | 1    | 443    | 32     | Yes       | No          |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | "   | "        | 01.org/4.2.0+ | "             | "          | "      | "    | "      | "      | Yes       | No          |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | 2   | C62x     | linux/4.5+    | qat_c62x      | c6xx       | 37c8   | 3    | 37c9   | 16     | Yes       | No          |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | "   | "        | 01.org/4.2.0+ | "             | "          | "      | "    | "      | "      | Yes       | Yes         |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | 2   | C3xxx    | linux/4.5+    | qat_c3xxx     | c3xxx      | 19e2   | 1    | 19e3   | 16     | Yes       | No          |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | "   | "        | 01.org/4.2.0+ | "             | "          | "      | "    | "      | "      | Yes       | Yes         |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | 2   | D15xx    | p             | qat_d15xx     | d15xx      | 6f54   | 1    | 6f55   | 16     | Yes       | No          |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+
   | 3   | C4xxx    | p             | qat_c4xxx     | c4xxx      | 18a0   | 1    | 18a1   | 128    | Yes       | No          |
   +-----+----------+---------------+---------------+------------+--------+------+--------+--------+-----------+-------------+


The ``Driver`` column indicates either the Linux kernel version in which
support for this device was introduced or a driver available on Intel's 01.org
website. There are both linux and 01.org kernel drivers available for some
devices. p = release pending.

If you are running on a kernel which includes a driver for your device, see
`Installation using kernel.org driver`_ below. Otherwise see
`Installation using 01.org QAT driver`_.


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
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Download the latest QuickAssist Technology Driver from `01.org
<https://01.org/packet-processing/intel%C2%AE-quickassist-technology-drivers-and-patches>`_.
Consult the *Getting Started Guide* at the same URL for further information.

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
      sudo yum install libudev-devel

.. Note::

   If the build or install fails due to mismatching kernel sources you may need to do the following::

      sudo yum install kernel-headers-`uname -r`
      sudo yum install kernel-src-`uname -r`
      sudo yum install kernel-devel-`uname -r`


Binding the available VFs to the DPDK UIO driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Unbind the VFs from the stock driver so they can be bound to the uio driver.

For an Intel(R) QuickAssist Technology DH895xCC device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The unbind command below assumes ``BDFs`` of ``03:01.00-03:04.07``, if your
VFs are different adjust the unbind command below::

    for device in $(seq 1 4); do \
        for fn in $(seq 0 7); do \
            echo -n 0000:03:0${device}.${fn} > \
            /sys/bus/pci/devices/0000\:03\:0${device}.${fn}/driver/unbind; \
        done; \
    done

For an Intel(R) QuickAssist Technology C62x device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The unbind command below assumes ``BDFs`` of ``01:01.00-01:02.07``, if your
VFs are different adjust the unbind command below::

    for device in $(seq 1 2); do \
        for fn in $(seq 0 7); do \
            echo -n 0000:01:0${device}.${fn} > \
            /sys/bus/pci/devices/0000\:01\:0${device}.${fn}/driver/unbind; \
        done; \
    done

Bind to the DPDK uio driver
^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Testing
~~~~~~~

QAT crypto PMD can be tested by running the test application::

    make defconfig
    make test-build -j
    cd ./build/app
    ./test -l1 -n1 -w <your qat bdf>
    RTE>>cryptodev_qat_autotest

QAT compression PMD can be tested by running the test application::

    make defconfig
    sed -i 's,\(CONFIG_RTE_COMPRESSDEV_TEST\)=n,\1=y,' build/.config
    make test-build -j
    cd ./build/app
    ./test -l1 -n1 -w <your qat bdf>
    RTE>>compressdev_autotest


Debugging
~~~~~~~~~

There are 2 sets of trace available via the dynamic logging feature:

* pmd.qat_dp exposes trace on the data-path.
* pmd.qat_general exposes all other trace.

pmd.qat exposes both sets of traces.
They can be enabled using the log-level option (where 8=maximum log level) on
the process cmdline, e.g. using any of the following::

    --log-level="pmd.qat_general,8"
    --log-level="pmd.qat_dp,8"
    --log-level="pmd.qat,8"

.. Note::

    The global RTE_LOG_DP_LEVEL overrides data-path trace so must be set to
    RTE_LOG_DEBUG to see all the trace. This variable is in config/rte_config.h
    for meson build and config/common_base for gnu make.
    Also the dynamic global log level overrides both sets of trace, so e.g. no
    QAT trace would display in this case::

	--log-level="7" --log-level="pmd.qat_general,8"
