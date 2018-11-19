..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Lcore-related options
~~~~~~~~~~~~~~~~~~~~~

*   ``-c <core mask>``

    Set the hexadecimal bitmask of the cores to run on.

*   ``-l <core list>``

    List of cores to run on

    The argument format is ``<c1>[-c2][,c3[-c4],...]``
    where ``c1``, ``c2``, etc are core indexes between 0 and 128.

*   ``--lcores <core map>``

    Map lcore set to physical cpu set

    The argument format is::

       <lcores[@cpus]>[<,lcores[@cpus]>...]

    Lcore and CPU lists are grouped by ``(`` and ``)`` Within the group.
    The ``-`` character is used as a range separator and ``,`` is used as a
    single number separator.
    The grouping ``()`` can be omitted for single element group.
    The ``@`` can be omitted if cpus and lcores have the same value.

.. Note::
    At a given instance only one core option ``--lcores``, ``-l`` or ``-c`` can
    be used.

*   ``--master-lcore <core ID>``

    Core ID that is used as master.

*   ``-s <service core mask>``

    Hexadecimal bitmask of cores to be used as service cores.

Device-related options
~~~~~~~~~~~~~~~~~~~~~~

*   ``-b, --pci-blacklist <[domain:]bus:devid.func>``

    Blacklist a PCI device to prevent EAL from using it. Multiple -b options are
    allowed.

.. Note::
    PCI blacklist cannot be used with ``-w`` option.

*   ``-w, --pci-whitelist <[domain:]bus:devid.func>``

    Add a PCI device in white list.

.. Note::
    PCI whitelist cannot be used with ``-b`` option.

*   ``--vdev <device arguments>``

    Add a virtual device using the format::

       <driver><id>[,key=val, ...]

    For example::

       --vdev 'net_pcap0,rx_pcap=input.pcap,tx_pcap=output.pcap'

*   ``-d <path to shared object or directory>``

    Load external drivers. An argument can be a single shared object file, or a
    directory containing multiple driver shared objects. Multiple -d options are
    allowed.

*   ``--no-pci``

    Disable PCI bus.

Multiprocessing-related options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*   ``--proc-type <primary|secondary|auto>``

    Set the type of the current process.

Memory-related options
~~~~~~~~~~~~~~~~~~~~~~

*   ``-n <number of channels>``

    Set the number of memory channels to use.

*   ``-r <number of ranks>``

    Set the number of memory ranks (auto-detected by default).

*   ``-m <megabytes>``

    Amount of memory to preallocate at startup.

*   ``--in-memory``

    Do not create any shared data structures and run entirely in memory. Implies
    ``--no-shconf`` and (if applicable) ``--huge-unlink``.

*   ``--iova-mode <pa|va>``

    Force IOVA mode to a specific value.

Debugging options
~~~~~~~~~~~~~~~~~

*   ``--no-shconf``

    No shared files created (implies no secondary process support).

*   ``--no-huge``

    Use anonymous memory instead of hugepages (implies no secondary process
    support).

*   ``--log-level <type:val>``

    Specify log level for a specific component. For example::

        --log-level eal:8

    Can be specified multiple times.

Other options
~~~~~~~~~~~~~

*   ``-h``, ``--help``

    Display help message listing all EAL parameters.

*   ``-v``

    Display the version information on startup.

*   ``mbuf-pool-ops-name``:

    Pool ops name for mbuf to use.
