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

*   ``--main-lcore <core ID>``

    Core ID that is used as main.

*   ``-s <service core mask>``

    Hexadecimal bitmask of cores to be used as service cores.

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

*   ``--base-virtaddr <address>``

    Attempt to use a different starting address for all memory maps of the
    primary DPDK process. This can be helpful if secondary processes cannot
    start due to conflicts in address map.

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

*   ``-h``, ``--help``

    Display help message listing all EAL parameters.

*   ``-v``

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
