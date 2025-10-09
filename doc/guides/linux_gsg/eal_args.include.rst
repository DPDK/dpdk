..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Lcore-related options
~~~~~~~~~~~~~~~~~~~~~

*   ``-l, --lcores <core list>``

    List of cores to run on

    Simplest argument format is ``<c1>[-c2][,c3[-c4],...]``
    where ``c1``, ``c2``, etc are core indexes between 0 and ``RTE_MAX_LCORE`` (default 128).

    This argument can also be used to map lcore set to physical cpu set

    The argument format is::

       <lcores[@cpus]>[<,lcores[@cpus]>...]

    Lcore and CPU lists are grouped by ``(`` and ``)`` Within the group.
    The ``-`` character is used as a range separator and ``,`` is used as a
    single number separator.
    The grouping ``()`` can be omitted for single element group.
    The ``@`` can be omitted if cpus and lcores have the same value.

    Examples:

    ``--lcores=1-3``
      Run threads on physical CPUs 1, 2 and 3,
      with each thread having the same lcore id as the physical CPU id.

    ``--lcores=1@(1,2)``
      Run a single thread with lcore id 1,
      but with that thread bound to both physical CPUs 1 and 2,
      so it can run on either, as determined by the operating system.

    ``--lcores=1@31,2@32,3@33``
      Run threads having internal lcore ids of 1, 2 and 3,
      but with the threads being bound to physical CPUs 31, 32 and 33 respectively.

    ``--lcores='(1-3)@(31-33)'``
      Run three threads with lcore ids 1, 2 and 3.
      Unlike the previous example above,
      each of these threads is not bound to one specific physical CPU,
      but rather, all three threads are instead bound to the three physical CPUs 31, 32 and 33.
      This means that each of the three threads can move between the physical CPUs 31-33,
      as decided by the OS as the application runs.

    ``--lcores='(1-3)@20'``
      Run three threads, with lcore ids 1, 2 and 3,
      where all three threads are bound to (can only run on) physical CPU 20.

.. note::

   Binding multiple DPDK lcores to a single physical CPU can cause problems with poor performance
   or deadlock when using DPDK rings or memory pools or spinlocks.
   Such a configuration should only be used with care.

.. note::

   As shown in the examples above, and depending on the shell in use,
   it is sometimes necessary to enclose the lcores parameter value in quotes,
   for example, when the parameter value starts with a ``(`` character.

.. note::

    At a given instance only one core option ``--lcores``, ``-l`` or ``-c`` can
    be used.

*   ``--main-lcore <core ID>``

    Core ID that is used as main.

*   ``-S, --service-corelist <service core list>``

    List of cores to be used as service cores.


Device-related options
~~~~~~~~~~~~~~~~~~~~~~

*   ``-b, --block <[domain:]bus:devid.func>``

    Skip probing a PCI device to prevent EAL from using it.
    Multiple -b options are allowed.

.. Note::
    Block list cannot be used with the allow list ``-a`` option.

*   ``-a, --allow <[domain:]bus:devid.func>``

    Add a PCI device in to the list of devices to probe.

.. Note::
    Allow list cannot be used with the block list ``-b`` option.

*   ``--vdev <device arguments>``

    Add a virtual device using the format::

       <driver><id>[,key=val, ...]

    For example::

       --vdev 'net_pcap0,rx_pcap=input.pcap,tx_pcap=output.pcap'

*   ``-d, --driver-path <path to shared object or directory>``

    Load external drivers. An argument can be a single shared object file, or a
    directory containing multiple driver shared objects. Multiple -d options are
    allowed.

*   ``--no-pci``

    Disable PCI bus.

Multiprocessing-related options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*   ``--proc-type <primary|secondary|auto>``

    Set the type of the current process.

*   ``--base-virtaddr <address>``

    Attempt to use a different starting address for all memory maps of the
    primary DPDK process. This can be helpful if secondary processes cannot
    start due to conflicts in address map.

Memory-related options
~~~~~~~~~~~~~~~~~~~~~~

*   ``-n, --memory-channels <number of channels>``

    Set the number of memory channels to use.

*   ``-r, --memory-ranks <number of ranks>``

    Set the number of memory ranks (auto-detected by default).

*   ``-m, --memory-size <megabytes>``

    Amount of memory to preallocate at startup.

*   ``--in-memory``

    Do not create any shared data structures and run entirely in memory. Implies
    ``--no-shconf`` and (if applicable) ``--huge-unlink``.

*   ``--iova-mode <pa|va>``

    Force IOVA mode to a specific value.

*   ``--huge-worker-stack[=size]``

    Allocate worker stack memory from hugepage memory. Stack size defaults
    to system pthread stack size unless the optional size (in kbytes) is
    specified.

Debugging options
~~~~~~~~~~~~~~~~~

*   ``--no-shconf``

    No shared files created (implies no secondary process support).

*   ``--no-huge``

    Use anonymous memory instead of hugepages (implies no secondary process
    support).

*   ``--log-level <type:val>``

    Specify log level for a specific component. For example::

        --log-level lib.eal:debug

    Can be specified multiple times.

*   ``--trace=<regex-match>``

    Enable trace based on regular expression trace name. By default, the trace is
    disabled. User must specify this option to enable trace.
    For example:

    Global trace configuration for EAL only::

        --trace=eal

    Global trace configuration for ALL the components::

        --trace=.*

    Can be specified multiple times up to 32 times.

*   ``--trace-dir=<directory path>``

    Specify trace directory for trace output. For example:

    Configuring ``/tmp/`` as a trace output directory::

        --trace-dir=/tmp

    By default, trace output will created at ``home`` directory and parameter
    must be specified once only.

*   ``--trace-bufsz=<val>``

    Specify maximum size of allocated memory for trace output for each thread.
    Valid unit can be either ``B`` or ``K`` or ``M`` for ``Bytes``, ``KBytes``
    and ``MBytes`` respectively. For example:

    Configuring ``2MB`` as a maximum size for trace output file::

        --trace-bufsz=2M

    By default, size of trace output file is ``1MB`` and parameter
    must be specified once only.

*   ``--trace-mode=<o[verwrite] | d[iscard] >``

    Specify the mode of update of trace output file. Either update on a file
    can be wrapped or discarded when file size reaches its maximum limit.
    For example:

    To ``discard`` update on trace output file::

        --trace-mode=d or --trace-mode=discard

    Default mode is ``overwrite`` and parameter must be specified once only.

Other options
~~~~~~~~~~~~~

*   ``-h, --help``

    Display help message listing all EAL parameters.

*   ``-v, --version``

    Display the version information on startup.

*   ``--mbuf-pool-ops-name``:

    Pool ops name for mbuf to use.

*    ``--telemetry``:

    Enable telemetry (enabled by default).

*    ``--no-telemetry``:

    Disable telemetry.

*    ``--force-max-simd-bitwidth=<val>``:

    Specify the maximum SIMD bitwidth size to handle. This limits which vector paths,
    if any, are taken, as any paths taken must use a bitwidth below the max bitwidth limit.
    For example, to allow all SIMD bitwidths up to and including AVX-512::

        --force-max-simd-bitwidth=512

    The following example shows limiting the bitwidth to 64-bits to disable all vector code::

        --force-max-simd-bitwidth=64

    To disable use of max SIMD bitwidth limit::

        --force-max-simd-bitwidth=0
