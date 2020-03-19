..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _install_from_ports:

Installing DPDK from the Ports Collection
=========================================

The easiest way to get up and running with the DPDK on FreeBSD is to
install it using the FreeBSD `pkg` utility or from the ports collection.
Details of installing applications from packages or the ports collection are documented in the
`FreeBSD Handbook <http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook/index.html>`_,
chapter `Installing Applications: Packages and Ports <https://www.freebsd.org/doc/handbook/ports.html>`_.

.. note::

   Please ensure that the latest patches are applied to third party libraries
   and software to avoid any known vulnerabilities.


Installing the DPDK Package for FreeBSD
---------------------------------------

DPDK can be installed on FreeBSD using the command::

	pkg install dpdk

After the installation of the DPDK package, instructions will be printed on
how to install the kernel modules required to use the DPDK. A more
complete version of these instructions can be found in the sections
:ref:`loading_contigmem` and :ref:`loading_nic_uio`. Normally, lines like
those below would be added to the file ``/boot/loader.conf``.

.. code-block:: shell

    # Reserve 2 x 1G blocks of contiguous memory using contigmem driver:
    hw.contigmem.num_buffers=2
    hw.contigmem.buffer_size=1073741824
    contigmem_load="YES"

    # Identify NIC devices for DPDK apps to use and load nic_uio driver:
    hw.nic_uio.bdfs="2:0:0,2:0:1"
    nic_uio_load="YES"


Installing the DPDK FreeBSD Port
--------------------------------

If so desired, the user can install DPDK using the ports collection rather than from
a pre-compiled binary package.
On a system with the ports collection installed in ``/usr/ports``, the DPDK
can be installed using the commands::

    cd /usr/ports/net/dpdk

    make install


Compiling and Running the Example Applications
----------------------------------------------

When the DPDK has been installed from the ports collection it installs
its example applications in ``/usr/local/share/dpdk/examples``.
These examples can be compiled and run as described in :ref:`compiling_sample_apps`.

.. note::

   DPDK example applications must be complied using `gmake` rather than
   BSD `make`. To detect the installed DPDK libraries, `pkg-config` should
   also be installed on the system.

.. note::

   To install a copy of the DPDK compiled using gcc, please download the
   official DPDK package from https://core.dpdk.org/download/ and install manually using
   the instructions given in the next chapter, :ref:`building_from_source`

An example application can therefore be copied to a user's home directory and
compiled and run as below, where we have 2 memory blocks of size 1G reserved
via the contigmem module, and 4 NIC ports bound to the nic_uio module::

    cp -r /usr/local/share/dpdk/examples/helloworld .

    cd helloworld/

    gmake
    cc -O3 -I/usr/local/include -include rte_config.h -march=corei7 -D__BSD_VISIBLE  main.c -o build/helloworld-shared  -L/usr/local/lib -lrte_bpf -lrte_flow_classify -lrte_pipeline -lrte_table -lrte_port -lrte_fib -lrte_ipsec -lrte_stack -lrte_security -lrte_sched -lrte_reorder -lrte_rib -lrte_rcu -lrte_rawdev -lrte_pdump -lrte_member -lrte_lpm -lrte_latencystats -lrte_jobstats -lrte_ip_frag -lrte_gso -lrte_gro -lrte_eventdev -lrte_efd -lrte_distributor -lrte_cryptodev -lrte_compressdev -lrte_cfgfile -lrte_bitratestats -lrte_bbdev -lrte_acl -lrte_timer -lrte_hash -lrte_metrics -lrte_cmdline -lrte_pci -lrte_ethdev -lrte_meter -lrte_net -lrte_mbuf -lrte_mempool -lrte_ring -lrte_eal -lrte_kvargs
    ln -sf helloworld-shared build/helloworld

    sudo ./build/helloworld -l 0-3
    EAL: Sysctl reports 8 cpus
    EAL: Detected 8 lcore(s)
    EAL: Detected 1 NUMA nodes
    EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
    EAL: Selected IOVA mode 'PA'
    EAL: Contigmem driver has 2 buffers, each of size 1GB
    EAL: Mapped memory segment 0 @ 0x1040000000: physaddr:0x180000000, len 1073741824
    EAL: Mapped memory segment 1 @ 0x1080000000: physaddr:0x1c0000000, len 1073741824
    EAL: PCI device 0000:00:19.0 on NUMA socket 0
    EAL:   probe driver: 8086:153b net_e1000_em
    EAL:   0000:00:19.0 not managed by UIO driver, skipping
    EAL: PCI device 0000:01:00.0 on NUMA socket 0
    EAL:   probe driver: 8086:1572 net_i40e
    EAL: PCI device 0000:01:00.1 on NUMA socket 0
    EAL:   probe driver: 8086:1572 net_i40e
    EAL: PCI device 0000:01:00.2 on NUMA socket 0
    EAL:   probe driver: 8086:1572 net_i40e
    EAL: PCI device 0000:01:00.3 on NUMA socket 0
    EAL:   probe driver: 8086:1572 net_i40e
    hello from core 1
    hello from core 2
    hello from core 3
    hello from core 0


.. note::

   To run a DPDK process as a non-root user, adjust the permissions on
   the ``/dev/contigmem`` and ``/dev/uio device`` nodes as described in section
   :ref:`running_non_root`

.. note::

   For an explanation of the command-line parameters that can be passed to an
   DPDK application, see section :ref:`running_sample_app`.
