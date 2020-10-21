..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Virtual Machine Power Management Application
============================================

Applications running in virtual environments have an abstract view of
the underlying hardware on the host. Specifically, applications cannot
see the binding of virtual components to physical hardware. When looking
at CPU resourcing, the pinning of Virtual CPUs (vCPUs) to Physical CPUs
(pCPUs) on the host is not apparent to an application and this pinning
may change over time. In addition, operating systems on Virtual Machines
(VMs) do not have the ability to govern their own power policy. The
Machine Specific Registers (MSRs) for enabling P-state transitions are
not exposed to the operating systems running on the VMs.

The solution demonstrated in this sample application shows an example of
how a DPDK application can indicate its processing requirements using
VM-local only information (vCPU/lcore, and so on) to a host resident VM
Power Manager. The VM Power Manager is responsible for:

- **Accepting requests for frequency changes for a vCPU**
- **Translating the vCPU to a pCPU using libvirt**
- **Performing the change in frequency**

This application demonstrates the following features:

- **The handling of VM application requests to change frequency.**
  VM applications can request frequency changes for a vCPU. The VM
  Power Management Application uses libvirt to translate that
  virtual CPU (vCPU) request to a physical CPU (pCPU) request and
  performs the frequency change.

- **The acceptance of power management policies from VM applications.**
  A VM application can send a policy to the host application. The
  policy contains rules that define the power management behaviour
  of the VM. The host application then applies the rules of the
  policy independent of the VM application. For example, the
  policy can contain time-of-day information for busy/quiet
  periods, and the host application can scale up/down the relevant
  cores when required. See :ref:`sending_policy` for information on
  setting policy values.

- **Out-of-band monitoring of workloads using core hardware event counters.**
  The host application can manage power for an application by looking
  at the event counters of the cores and taking action based on the
  branch miss/hit ratio. See :ref:`enabling_out_of_band`.

  **Note**: This functionality also applies in non-virtualised environments.

In addition to the ``librte_power`` library used on the host, the
application uses a special version of ``librte_power`` on each VM, which
directs frequency changes and policies to the host monitor rather than
the APCI ``cpufreq`` ``sysfs`` interface used on the host in non-virtualised
environments.

.. _figure_vm_power_mgr_highlevel:

.. figure:: img/vm_power_mgr_highlevel.*

   Highlevel Solution

In the above diagram, the DPDK Applications are shown running in
virtual machines, and the VM Power Monitor application is shown running
in the host.

**DPDK VM Application**

- Reuse ``librte_power`` interface, but uses an implementation that
  forwards frequency requests to the host using a ``virtio-serial`` channel
- Each lcore has exclusive access to a single channel
- Sample application reuses ``l3fwd_power``
- A CLI for changing frequency from within a VM is also included

**VM Power Monitor**

- Accepts VM commands over ``virtio-serial`` endpoints, monitored
  using ``epoll``
- Commands include the virtual core to be modified, using ``libvirt`` to get
  the physical core mapping
- Uses ``librte_power`` to affect frequency changes using Linux userspace
  power governor (``acpi_cpufreq`` OR ``intel_pstate`` driver)
- CLI: For adding VM channels to monitor, inspecting and changing channel
  state, manually altering CPU frequency. Also allows for the changings
  of vCPU to pCPU pinning

Sample Application Architecture Overview
----------------------------------------

The VM power management solution employs ``qemu-kvm`` to provide
communications channels between the host and VMs in the form of a
``virtio-serial`` connection that appears as a para-virtualised serial
device on a VM and can be configured to use various backends on the
host. For this example, the configuration of each ``virtio-serial`` endpoint
on the host as an ``AF_UNIX`` file socket, supporting poll/select and
``epoll`` for event notification. In this example, each channel endpoint on
the host is monitored for ``EPOLLIN`` events using ``epoll``. Each channel
is specified as ``qemu-kvm`` arguments or as ``libvirt`` XML for each VM,
where each VM can have several channels up to a maximum of 64 per VM. In this
example, each DPDK lcore on a VM has exclusive access to a channel.

To enable frequency changes from within a VM, the VM forwards a
``librte_power`` request over the ``virtio-serial`` channel to the host. Each
request contains the vCPU and power command (scale up/down/min/max). The
API for the host ``librte_power`` and guest ``librte_power`` is consistent
across environments, with the selection of VM or host implementation
determined automatically at runtime based on the environment. On
receiving a request, the host translates the vCPU to a pCPU using the
libvirt API before forwarding it to the host ``librte_power``.


.. _figure_vm_power_mgr_vm_request_seq:

.. figure:: img/vm_power_mgr_vm_request_seq.*

In addition to the ability to send power management requests to the
host, a VM can send a power management policy to the host. In some
cases, using a power management policy is a preferred option because it
can eliminate possible latency issues that can occur when sending power
management requests. Once the VM sends the policy to the host, the VM no
longer needs to worry about power management, because the host now
manages the power for the VM based on the policy. The policy can specify
power behavior that is based on incoming traffic rates or time-of-day
power adjustment (busy/quiet hour power adjustment for example). See
:ref:`sending_policy` for more information.

One method of power management is to sense how busy a core is when
processing packets and adjusting power accordingly. One technique for
doing this is to monitor the ratio of the branch miss to branch hits
counters and scale the core power accordingly. This technique is based
on the premise that when a core is not processing packets, the ratio of
branch misses to branch hits is very low, but when the core is
processing packets, it is measurably higher. The implementation of this
capability is as a policy of type ``BRANCH_RATIO``.
See :ref:`sending_policy` for more information on using the
BRANCH_RATIO policy option.

A JSON interface enables the specification of power management requests
and policies in JSON format. The JSON interfaces provide a more
convenient and more easily interpreted interface for the specification
of requests and policies. See :ref:`power_man_requests` for more information.

Performance Considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

While the Haswell microarchitecture allows for independent power control
for each core, earlier microarchitectures do not offer such fine-grained
control. When deploying on pre-Haswell platforms, greater care must be
taken when selecting which cores are assigned to a VM, for example, a
core does not scale down in frequency until all of its siblings are
similarly scaled down.

Configuration
-------------

BIOS
~~~~

To use the power management features of the DPDK, you must enable
Enhanced Intel SpeedStep® Technology in the platform BIOS. Otherwise,
the ``sys`` file folder ``/sys/devices/system/cpu/cpu0/cpufreq`` does not
exist, and you cannot use CPU frequency-based power management. Refer to the
relevant BIOS documentation to determine how to access these settings.

Host Operating System
~~~~~~~~~~~~~~~~~~~~~

The DPDK Power Management library can use either the ``acpi_cpufreq`` or
the ``intel_pstate`` kernel driver for the management of core frequencies. In
many cases, the ``intel_pstate`` driver is the default power management
environment.

Should the ``acpi-cpufreq driver`` be required, the ``intel_pstate``
module must be disabled, and the ``acpi-cpufreq`` module loaded in its place.

To disable the ``intel_pstate`` driver, add the following to the ``grub``
Linux command line:

   ``intel_pstate=disable``

On reboot, load the ``acpi_cpufreq`` module:

   ``modprobe acpi_cpufreq``

Hypervisor Channel Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Configure ``virtio-serial`` channels using ``libvirt`` XML.
The XML structure is as follows: 

.. code-block:: XML

   <name>{vm_name}</name>
   <controller type='virtio-serial' index='0'>
      <address type='pci' domain='0x0000' bus='0x00' slot='0x06' function='0x0'/>
   </controller>
   <channel type='unix'>
      <source mode='bind' path='/tmp/powermonitor/{vm_name}.{channel_num}'/>
      <target type='virtio' name='virtio.serial.port.poweragent.{vm_channel_num}'/>
      <address type='virtio-serial' controller='0' bus='0' port='{N}'/>
   </channel>

Where a single controller of type ``virtio-serial`` is created, up to 32
channels can be associated with a single controller, and multiple
controllers can be specified. The convention is to use the name of the
VM in the host path ``{vm_name}`` and to increment ``{channel_num}`` for each
channel. Likewise, the port value ``{N}`` must be incremented for each
channel.

On the host, for each channel to appear in the path, ensure the creation
of the ``/tmp/powermonitor/`` directory and the assignment of ``qemu``
permissions:

.. code-block:: console

   mkdir /tmp/powermonitor/
   chown qemu:qemu /tmp/powermonitor

Note that files and directories in ``/tmp`` are generally removed when
rebooting the host and you may need to perform the previous steps after
each reboot.

The serial device as it appears on a VM is configured with the target
element attribute name and must be in the form:
``virtio.serial.port.poweragent.{vm_channel_num}``, where
``vm_channel_num`` is typically the lcore channel to be used in
DPDK VM applications.

Each channel on a VM is present at:

``/dev/virtio-ports/virtio.serial.port.poweragent.{vm_channel_num}``

Compiling and Running the Host Application
------------------------------------------

Compiling the Host Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For information on compiling the DPDK and sample applications, see
see :doc:`compiling`.

The application is located in the ``vm_power_manager`` subdirectory.

To build just the ``vm_power_manager`` application using ``make``:

.. code-block:: console

   cd dpdk/examples/vm_power_manager/
   make

The resulting binary is ``dpdk/build/examples/vm_power_manager``.

To build just the ``vm_power_manager`` application using ``meson``/``ninja``:

.. code-block:: console

   cd dpdk
   meson build
   cd build
   ninja
   meson configure -Dexamples=vm_power_manager
   ninja

The resulting binary is ``dpdk/build/examples/dpdk-vm_power_manager``.

Running the Host Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application does not have any specific command line options other
than the EAL options:

.. code-block:: console

   ./<build_dir>/examples/dpdk-vm_power_mgr [EAL options]

The application requires exactly two cores to run. One core for the CLI
and the other for the channel endpoint monitor. For example, to run on
cores 0 and 1 on a system with four memory channels, issue the command:

.. code-block:: console

   ./<build_dir>/examples/dpdk-vm_power_mgr -l 0-1 -n 4

After successful initialization, the VM Power Manager CLI prompt appears:

.. code-block:: console

   vm_power>

Now, it is possible to add virtual machines to the VM Power Manager:

.. code-block:: console

   vm_power> add_vm {vm_name}

When a ``{vm_name}`` is specified with the ``add_vm`` command, a lookup is
performed with ``libvirt`` to ensure that the VM exists. ``{vm_name}`` is a
unique identifier to associate channels with a particular VM and for
executing operations on a VM within the CLI. VMs do not have to be
running to add them.

It is possible to issue several commands from the CLI to manage VMs.

Remove the virtual machine identified by ``{vm_name}`` from the VM Power
Manager using the command:

.. code-block:: console

   rm_vm {vm_name}

Add communication channels for the specified VM using the following
command. The ``virtio`` channels must be enabled in the VM configuration
(``qemu/libvirt``) and the associated VM must be active. ``{list}`` is a
comma-separated list of channel numbers to add. Specifying the keyword
``all`` attempts to add all channels for the VM:

.. code-block:: console

   set_pcpu {vm_name} {vcpu} {pcpu}

  Enable query of physical core information from a VM:

.. code-block:: console

   set_query {vm_name} enable|disable

Manual control and inspection can also be carried in relation CPU frequency scaling:

  Get the current frequency for each core specified in the mask:

.. code-block:: console

   show_cpu_freq_mask {mask}

  Set the current frequency for the cores specified in {core_mask} by scaling each up/down/min/max:

.. code-block:: console

   add_channels {vm_name} {list}|all

Enable or disable the communication channels in ``{list}`` (comma-separated)
for the specified VM. Alternatively, replace ``list`` with the keyword
``all``. Disabled channels receive packets on the host. However, the commands
they specify are ignored. Set the status to enabled to begin processing
requests again:

.. code-block:: console

   set_channel_status {vm_name} {list}|all enabled|disabled

Print to the CLI information on the specified VM. The information lists
the number of vCPUs, the pinning to pCPU(s) as a bit mask, along with
any communication channels associated with each VM, and the status of
each channel:

.. code-block:: console

   show_vm {vm_name}

Set the binding of a virtual CPU on a VM with name ``{vm_name}`` to the
physical CPU mask:

.. code-block:: console

   set_pcpu_mask {vm_name} {vcpu} {pcpu}

Set the binding of the virtual CPU on the VM to the physical CPU:
 
  .. code-block:: console

   set_pcpu {vm_name} {vcpu} {pcpu}

It is also possible to perform manual control and inspection in relation
to CPU frequency scaling.

Get the current frequency for each core specified in the mask:

.. code-block:: console

   show_cpu_freq_mask {mask}

Set the current frequency for the cores specified in ``{core_mask}`` by
scaling each up/down/min/max:

.. code-block:: console

   set_cpu_freq {core_mask} up|down|min|max

Get the current frequency for the specified core:

.. code-block:: console

   show_cpu_freq {core_num}

Set the current frequency for the specified core by scaling up/down/min/max:

.. code-block:: console

   set_cpu_freq {core_num} up|down|min|max

.. _enabling_out_of_band:

Command Line Options for Enabling Out-of-band Branch Ratio Monitoring
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are a couple of command line parameters for enabling the out-of-band
monitoring of branch ratios on cores doing busy polling using PMDs as
described below:

``--core-branch-ratio {list of cores}:{branch ratio for listed cores}``
   Specify the list of cores to monitor the ratio of branch misses
   to branch hits.  A tightly-polling PMD thread has a very low
   branch ratio, therefore the core frequency scales down to the
   minimum allowed value. On receiving packets, the code path changes,
   causing the branch ratio to increase. When the ratio goes above
   the ratio threshold, the core frequency scales up to the maximum
   allowed value. The specified branch-ratio is a floating point number
   that identifies the threshold at which to scale up or down for the
   elements of the core-list. If not included the default branch ratio of
   0.01 but will need adjustment for different workloads

   This parameter can be used multiple times for different sets of cores.
   The branch ratio mechanism can also be useful for non-PMD cores and
   hyper-threaded environments where C-States are disabled.


Compiling and Running the Guest Applications
--------------------------------------------

It is possible to use the ``l3fwd-power`` application (for example) with the
``vm_power_manager``.

The distribution also provides a guest CLI for validating the setup.

For both ``l3fwd-power`` and the guest CLI, the host application must use
the ``add_channels`` command to monitor the channels for the VM. To do this,
issue the following commands in the host application:

.. code-block:: console

   vm_power> add_vm vmname
   vm_power> add_channels vmname all
   vm_power> set_channel_status vmname all enabled
   vm_power> show_vm vmname

Compiling the Guest Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For information on compiling DPDK and the sample applications in general,
see :doc:`compiling`.

For compiling and running the ``l3fwd-power`` sample application, see
:doc:`l3_forward_power_man`.

The application is in the ``guest_cli`` subdirectory under ``vm_power_manager``.

To build just the ``guest_vm_power_manager`` application using ``make``, issue
the following commands:

.. code-block:: console

   cd dpdk/examples/vm_power_manager/guest_cli/
   make

The resulting binary is ``dpdk/build/examples/guest_cli``.

**Note**: This sample application conditionally links in the Jansson JSON
library. Consequently, if you are using a multilib or cross-compile
environment, you may need to set the ``PKG_CONFIG_LIBDIR`` environmental
variable to point to the relevant ``pkgconfig`` folder so that the correct
library is linked in.

For example, if you are building for a 32-bit target, you could find the
correct directory using the following find command:

.. code-block:: console

   # find /usr -type d -name pkgconfig
   /usr/lib/i386-linux-gnu/pkgconfig
   /usr/lib/x86_64-linux-gnu/pkgconfig

Then use:

.. code-block:: console

   export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig

You then use the ``make`` command as normal, which should find the 32-bit
version of the library, if it installed. If not, the application builds
without the JSON interface functionality.

To build just the ``vm_power_manager`` application using ``meson``/``ninja``:

.. code-block:: console

   cd dpdk
   meson build
   cd build
   ninja
   meson configure -Dexamples=vm_power_manager/guest_cli
   ninja

The resulting binary is ``dpdk/build/examples/guest_cli``.

Running the Guest Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The standard EAL command line parameters are necessary:

.. code-block:: console

   ./<build_dir>/examples/dpdk-vm_power_mgr [EAL options] -- [guest options]

The guest example uses a channel for each lcore enabled. For example, to
run on cores 0, 1, 2 and 3:

.. code-block:: console

   ./<build_dir>/examples/dpdk-guest_vm_power_mgr -l 0-3

.. _sending_policy:

Command Line Options Available When Sending a Policy to the Host
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Optionally, there are several command line options for a user who needs
to send a power policy to the host application:

``--vm-name {name of guest vm}``
   Allows the user to change the virtual machine name
   passed down to the host application using the power policy.
   The default is ubuntu2.

``--vcpu-list {list vm cores}``
   A comma-separated list of cores in the VM that the user
   wants the host application to monitor.
   The list of cores in any VM starts at zero,
   and the host application maps these to the physical cores
   once the policy passes down to the host.
   Valid syntax includes individual cores 2,3,4,
   a range of cores 2-4, or a combination of both 1,3,5-7.

``--busy-hours {list of busy hours}``
   A comma-separated list of hours in which to set the core
   frequency to the maximum.
   Valid syntax includes individual hours 2,3,4,
   a range of hours 2-4, or a combination of both 1,3,5-7.
   Valid hour values are 0 to 23.

``--quiet-hours {list of quiet hours}``
   A comma-separated list of hours in which to set the core frequency
   to minimum. Valid syntax includes individual hours 2,3,4,
   a range of hours 2-4, or a combination of both 1,3,5-7.
   Valid hour values are 0 to 23.

``--policy {policy type}``
   The type of policy. This can be one of the following values:

   - TRAFFIC - Based on incoming traffic rates on the NIC.
   - TIME - Uses a busy/quiet hours policy.
   - BRANCH_RATIO - Uses branch ratio counters to determine core busyness.
   - WORKLOAD - Sets the frequency to low, medium or high
     based on the received policy setting.

   **Note**: Not all policy types need all parameters.
   For example, BRANCH_RATIO only needs the vcpu-list parameter.

After successful initialization, the VM Power Manager Guest CLI prompt
appears:

.. code-block:: console

   vm_power(guest)>

To change the frequency of an lcore, use a ``set_cpu_freq`` command similar
to the following:

.. code-block:: console

   set_cpu_freq {core_num} up|down|min|max

where, ``{core_num}`` is the lcore and channel to change frequency by
scaling up/down/min/max.

To start an application, configure the power policy, and send it to the
host, use a command like the following:

.. code-block:: console

   ./<build_dir>/examples/dpdk-guest_vm_power_mgr -l 0-3 -n 4 -- --vm-name=ubuntu --policy=BRANCH_RATIO --vcpu-list=2-4

Once the VM Power Manager Guest CLI appears, issuing the 'send_policy now' command
will send the policy to the host:

.. code-block:: console

  send_policy now

Once the policy is sent to the host, the host application takes over the power monitoring
of the specified cores in the policy.

.. _power_man_requests:

JSON Interface for Power Management Requests and Policies
---------------------------------------------------------

In addition to the command line interface for the host command, and a
``virtio-serial`` interface for VM power policies, there is also a JSON
interface through which power commands and policies can be sent.

**Note**: This functionality adds a dependency on the Jansson library.
Install the Jansson development package on the system to avail of the
JSON parsing functionality in the app. Issue the ``apt-get install
libjansson-dev`` command to install the development package. The command
and package name may be different depending on your operating system. It
is worth noting that the app builds successfully if this package is not
present, but a warning displays during compilation, and the JSON parsing
functionality is not present in the app.

Send a request or policy to the VM Power Manager by simply opening a
fifo file at ``/tmp/powermonitor/fifo``, writing a JSON string to that file,
and closing the file.

The JSON string can be a power management request or a policy, and takes
the following format:

.. code-block:: javascript

   {"packet_type": {
   "pair_1": value,
   "pair_2": value
   }}

The ``packet_type`` header can contain one of two values, depending on
whether a power management request or policy is being sent. The two
possible values are ``instruction`` and ``policy`` and the expected name-value
pairs are different depending on which type is sent.

The pairs are in the format of standard JSON name-value pairs. The value
type varies between the different name-value pairs, and may be integers,
strings, arrays, and so on. See :ref:`json_interface_ex`
for examples of policies and instructions and
:ref:`json_name_value_pair` for the supported names and value types.

.. _json_interface_ex:

JSON Interface Examples
~~~~~~~~~~~~~~~~~~~~~~~

The following is an example JSON string that creates a time-profile
policy.

.. code-block:: JSON

   {"policy": {
   "name": "ubuntu",
   "command": "create",
   "policy_type": "TIME",
   "busy_hours":[ 17, 18, 19, 20, 21, 22, 23 ],
   "quiet_hours":[ 2, 3, 4, 5, 6 ],
   "core_list":[ 11 ]
   }}

The following is an example JSON string that removes the named policy.

.. code-block:: JSON

   {"policy": {
   "name": "ubuntu",
   "command": "destroy",
   }}

The following is an example JSON string for a power management request.

.. code-block:: JSON

   {"instruction": {
   "name": "ubuntu",
   "command": "power",
   "unit": "SCALE_MAX",
   "resource_id": 10
   }}

To query the available frequences of an lcore, use the query_cpu_freq command.
Where {core_num} is the lcore to query.
Before using this command, please enable responses via the set_query command on the host.

.. code-block:: console

  query_cpu_freq {core_num}|all

To query the capabilities of an lcore, use the query_cpu_caps command.
Where {core_num} is the lcore to query.
Before using this command, please enable responses via the set_query command on the host.

.. code-block:: console

  query_cpu_caps {core_num}|all

To start the application and configure the power policy, and send it to the host:

.. code-block:: console

 ./<build_dir>/examples/dpdk-guest_vm_power_mgr -l 0-3 -n 4 -- --vm-name=ubuntu --policy=BRANCH_RATIO --vcpu-list=2-4

Once the VM Power Manager Guest CLI appears, issuing the 'send_policy now' command
will send the policy to the host:

.. code-block:: console

  send_policy now

Once the policy is sent to the host, the host application takes over the power monitoring
of the specified cores in the policy.

.. _json_name_value_pair:

JSON Name-value Pairs
~~~~~~~~~~~~~~~~~~~~~

The following are the name-value pairs supported by the JSON interface:

-  `avg_packet_thresh`_
-  `busy_hours`_
-  `command`_
-  `core_list`_
-  `mac_list`_
-  `max_packet_thresh`_
-  `name`_
-  `policy_type`_
-  `quiet_hours`_
-  `resource_id`_
-  `unit`_
-  `workload`_

avg_packet_thresh
^^^^^^^^^^^^^^^^^

Description
   The threshold below which the frequency is set to the minimum value
   for the TRAFFIC policy.
   If the traffic rate is above this value and below the maximum value,
   the frequency is set to medium.
Type
   integer
Values
   The number of packets below which the TRAFFIC policy applies
   the minimum frequency, or the medium frequency
   if between the average and maximum thresholds.
Required
   Yes
Example
   ``"avg_packet_thresh": 100000``

busy_hours
^^^^^^^^^^

Description
   The hours of the day in which we scale up the cores for busy times.
Type
   array of integers
Values
   An array with a list of hour values (0-23).
Required
   For the TIME policy only.
Example
   ``"busy_hours":[ 17, 18, 19, 20, 21, 22, 23 ]``

command
^^^^^^^

Description
   The type of packet to send to the VM Power Manager.
   It is possible to create or destroy a policy or send a direct command
   to adjust the frequency of a core,
   as is possible on the command line interface.
Type
   string
Values
   Possible values are:
   - CREATE: Create a new policy.
   - DESTROY: Remove an existing policy.
   - POWER: Send an immediate command, max, min, and so on.
Required
   Yes
Example
   ``"command": "CREATE"``

core_list
^^^^^^^^^

Description
   The cores to which to apply a policy.
Type
   array of integers
Values
   An array with a list of virtual CPUs.
Required
   For CREATE/DESTROY policy requests only.
Example
   ``"core_list":[ 10, 11 ]``

mac_list
^^^^^^^^

Description
   When the policy is of type TRAFFIC,
   it is necessary to specify the MAC addresses that the host must monitor.
Type
   array of strings
Values
   An array with a list of MAC address strings.
Required
   For TRAFFIC policy types only.
Example
   ``"mac_list":[ "de:ad:be:ef:01:01","de:ad:be:ef:01:02" ]``

max_packet_thresh
^^^^^^^^^^^^^^^^^

Description
   In a policy of type TRAFFIC,
   the threshold value above which the frequency is set to a maximum.
Type
   integer
Values
   The number of packets per interval above which
   the TRAFFIC policy applies the maximum frequency.
Required
   For the TRAFFIC policy only.
Example
   ``"max_packet_thresh": 500000``

name
^^^^

Description
   The name of the VM or host.
   Allows the parser to associate the policy with the relevant VM or host OS.
Type
   string
Values
   Any valid string.
Required
   Yes
Example
   ``"name": "ubuntu2"``

policy_type
^^^^^^^^^^^

Description
   The type of policy to apply.
   See the ``--policy`` option description for more information.
Type
   string
Values
   Possible values are:

   - TIME: Time-of-day policy.
     Scale the frequencies of the relevant cores up/down
     depending on busy and quiet hours.
   - TRAFFIC: Use statistics from the NIC and scale up and down accordingly.
   - WORKLOAD: Determine how heavily loaded the cores are
     and scale up and down accordingly.
   - BRANCH_RATIO: An out-of-band policy that looks at the ratio
     between branch hits and misses on a core
     and uses that information to determine how much packet processing
     a core is doing.

Required
   For ``CREATE`` and ``DESTROY`` policy requests only.
Example
   ``"policy_type": "TIME"``

quiet_hours
^^^^^^^^^^^

Description
   The hours of the day to scale down the cores for quiet times.
Type
   array of integers
Values
   An array with a list of hour numbers with values in the range 0 to 23.
Required
   For the TIME policy only.
Example
   ``"quiet_hours":[ 2, 3, 4, 5, 6 ]``

resource_id
^^^^^^^^^^^

Description
   The core to which to apply a power command.
Type
   integer
Values
   A valid core ID for the VM or host OS.
Required
   For the ``POWER`` instruction only.
Example
   ``"resource_id": 10``

unit
^^^^

Description
   The type of power operation to apply in the command.
Type
   string
Values
   - SCALE_MAX: Scale the frequency of this core to the maximum.
   - SCALE_MIN: Scale the frequency of this core to the minimum.
   - SCALE_UP: Scale up the frequency of this core.
   - SCALE_DOWN: Scale down the frequency of this core.
   - ENABLE_TURBO: Enable Intel® Turbo Boost Technology for this core.
   - DISABLE_TURBO: Disable Intel® Turbo Boost Technology for this core.
Required
   For the ``POWER`` instruction only.
Example
   ``"unit": "SCALE_MAX"``

workload
^^^^^^^^

Description
   In a policy of type WORKLOAD,
   it is necessary to specify how heavy the workload is.
Type
   string
Values
   - HIGH: Scale the frequency of this core to maximum.
   - MEDIUM: Scale the frequency of this core to minimum.
   - LOW: Scale up the frequency of this core.
Required
   For the ``WORKLOAD`` policy only.
Example
   ``"workload": "MEDIUM"``
