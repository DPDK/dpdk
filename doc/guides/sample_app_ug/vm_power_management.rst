..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

VM Power Management Application
===============================

Introduction
------------

Applications running in Virtual Environments have an abstract view of
the underlying hardware on the Host, in particular applications cannot see
the binding of virtual to physical hardware.
When looking at CPU resourcing, the pinning of Virtual CPUs(vCPUs) to
Host Physical CPUs(pCPUS) is not apparent to an application
and this pinning may change over time.
Furthermore, Operating Systems on virtual machines do not have the ability
to govern their own power policy; the Machine Specific Registers (MSRs)
for enabling P-State transitions are not exposed to Operating Systems
running on Virtual Machines(VMs).

The Virtual Machine Power Management solution shows an example of
how a DPDK application can indicate its processing requirements using VM local
only information(vCPU/lcore, etc.) to a Host based Monitor which is responsible
for accepting requests for frequency changes for a vCPU, translating the vCPU
to a pCPU via libvirt and affecting the change in frequency.

The solution is comprised of two high-level components:

#. Example Host Application

   Using a Command Line Interface(CLI) for VM->Host communication channel management
   allows adding channels to the Monitor, setting and querying the vCPU to pCPU pinning,
   inspecting and manually changing the frequency for each CPU.
   The CLI runs on a single lcore while the thread responsible for managing
   VM requests runs on a second lcore.

   VM requests arriving on a channel for frequency changes are passed
   to the librte_power ACPI cpufreq sysfs based library.
   The Host Application relies on both qemu-kvm and libvirt to function.

   This monitoring application is responsible for:

   - Accepting requests from client applications: Client applications can
     request frequency changes for a vCPU, translating
     the vCPU to a pCPU via libvirt and affecting the change in frequency.

   - Accepting policies from client applications: Client application can
     send a policy to the host application. The
     host application will then apply the rules of the policy independent
     of the application. For example, the policy can contain time-of-day
     information for busy/quiet periods, and the host application can scale
     up/down the relevant cores when required. See the details of the guest
     application below for more information on setting the policy values.

   - Out-of-band monitoring of workloads via cores hardware event counters:
     The host application can manage power for an application in a virtualised
     OR non-virtualised environment by looking at the event counters of the
     cores and taking action based on the branch hit/miss ratio. See the host
     application '--core-list' command line parameter below.

#. librte_power for Virtual Machines

   Using an alternate implementation for the librte_power API, requests for
   frequency changes are forwarded to the host monitor rather than
   the APCI cpufreq sysfs interface used on the host.

   The l3fwd-power application will use this implementation when deployed on a VM
   (see :doc:`l3_forward_power_man`).

.. _figure_vm_power_mgr_highlevel:

.. figure:: img/vm_power_mgr_highlevel.*

   Highlevel Solution


Overview
--------

VM Power Management employs qemu-kvm to provide communications channels
between the host and VMs in the form of Virtio-Serial which appears as
a paravirtualized serial device on a VM and can be configured to use
various backends on the host. For this example each Virtio-Serial endpoint
on the host is configured as AF_UNIX file socket, supporting poll/select
and epoll for event notification.
In this example each channel endpoint on the host is monitored via
epoll for EPOLLIN events.
Each channel is specified as qemu-kvm arguments or as libvirt XML for each VM,
where each VM can have a number of channels up to a maximum of 64 per VM,
in this example each DPDK lcore on a VM has exclusive access to a channel.

To enable frequency changes from within a VM, a request via the librte_power interface
is forwarded via Virtio-Serial to the host, each request contains the vCPU
and power command(scale up/down/min/max).
The API for host and guest librte_power is consistent across environments,
with the selection of VM or Host Implementation determined at automatically
at runtime based on the environment.

Upon receiving a request, the host translates the vCPU to a pCPU via
the libvirt API before forwarding to the host librte_power.

.. _figure_vm_power_mgr_vm_request_seq:

.. figure:: img/vm_power_mgr_vm_request_seq.*

   VM request to scale frequency


Performance Considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

While Haswell Microarchitecture allows for independent power control for each core,
earlier Microarchtectures do not offer such fine grained control.
When deployed on pre-Haswell platforms greater care must be taken in selecting
which cores are assigned to a VM, for instance a core will not scale down
until its sibling is similarly scaled.

Configuration
-------------

BIOS
~~~~

Enhanced Intel SpeedStep® Technology must be enabled in the platform BIOS
if the power management feature of DPDK is to be used.
Otherwise, the sys file folder /sys/devices/system/cpu/cpu0/cpufreq will not exist,
and the CPU frequency-based power management cannot be used.
Consult the relevant BIOS documentation to determine how these settings
can be accessed.

Host Operating System
~~~~~~~~~~~~~~~~~~~~~

The DPDK Power Library can use either the *acpi_cpufreq* or *intel_pstate*
kernel driver for the management of core frequencies. In many cases
the *intel_pstate* driver is the default Power Management environment.

Should the *acpi-cpufreq* driver be required, the *intel_pstate* module must
be disabled, and *apci_cpufreq* module loaded in its place.

To disable *intel_pstate* driver, add the following to the grub Linux
command line:

.. code-block:: console

  intel_pstate=disable

Upon rebooting, load the *acpi_cpufreq* module:

.. code-block:: console

  modprobe acpi_cpufreq

Hypervisor Channel Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Virtio-Serial channels are configured via libvirt XML:


.. code-block:: xml

  <name>{vm_name}</name>
  <controller type='virtio-serial' index='0'>
    <address type='pci' domain='0x0000' bus='0x00' slot='0x06' function='0x0'/>
  </controller>
  <channel type='unix'>
    <source mode='bind' path='/tmp/powermonitor/{vm_name}.{channel_num}'/>
    <target type='virtio' name='virtio.serial.port.poweragent.{vm_channel_num}'/>
    <address type='virtio-serial' controller='0' bus='0' port='{N}'/>
  </channel>


Where a single controller of type *virtio-serial* is created and up to 32 channels
can be associated with a single controller and multiple controllers can be specified.
The convention is to use the name of the VM in the host path *{vm_name}* and
to increment *{channel_num}* for each channel, likewise the port value *{N}*
must be incremented for each channel.

Each channel on the host will appear in *path*, the directory */tmp/powermonitor/*
must first be created and given qemu permissions

.. code-block:: console

  mkdir /tmp/powermonitor/
  chown qemu:qemu /tmp/powermonitor

Note that files and directories within /tmp are generally removed upon
rebooting the host and the above steps may need to be carried out after each reboot.

The serial device as it appears on a VM is configured with the *target* element attribute *name*
and must be in the form of *virtio.serial.port.poweragent.{vm_channel_num}*,
where *vm_channel_num* is typically the lcore channel to be used in DPDK VM applications.

Each channel on a VM will be present at */dev/virtio-ports/virtio.serial.port.poweragent.{vm_channel_num}*

Compiling and Running the Host Application
------------------------------------------

Compiling
~~~~~~~~~

For information on compiling DPDK and the sample applications
see :doc:`compiling`.

The application is located in the ``vm_power_manager`` sub-directory.

To build just the ``vm_power_manager`` application using ``make``:

.. code-block:: console

  export RTE_SDK=/path/to/rte_sdk
  export RTE_TARGET=build
  cd ${RTE_SDK}/examples/vm_power_manager/
  make

The resulting binary will be ${RTE_SDK}/build/examples/vm_power_manager

To build just the ``vm_power_manager`` application using ``meson/ninja``:

.. code-block:: console

  export RTE_SDK=/path/to/rte_sdk
  cd ${RTE_SDK}
  meson build
  cd build
  ninja
  meson configure -Dexamples=vm_power_manager
  ninja

The resulting binary will be ${RTE_SDK}/build/examples/dpdk-vm_power_manager

Running
~~~~~~~

The application does not have any specific command line options other than *EAL*:

.. code-block:: console

 ./build/vm_power_mgr [EAL options]

The application requires exactly two cores to run, one core is dedicated to the CLI,
while the other is dedicated to the channel endpoint monitor, for example to run
on cores 0 & 1 on a system with 4 memory channels:

.. code-block:: console

 ./build/vm_power_mgr -l 0-1 -n 4

After successful initialization the user is presented with VM Power Manager CLI:

.. code-block:: console

  vm_power>

Virtual Machines can now be added to the VM Power Manager:

.. code-block:: console

  vm_power> add_vm {vm_name}

When a {vm_name} is specified with the *add_vm* command a lookup is performed
with libvirt to ensure that the VM exists, {vm_name} is used as an unique identifier
to associate channels with a particular VM and for executing operations on a VM within the CLI.
VMs do not have to be running in order to add them.

A number of commands can be issued via the CLI in relation to VMs:

  Remove a Virtual Machine identified by {vm_name} from the VM Power Manager.

  .. code-block:: console

    rm_vm {vm_name}

  Add communication channels for the specified VM, the virtio channels must be enabled
  in the VM configuration(qemu/libvirt) and the associated VM must be active.
  {list} is a comma-separated list of channel numbers to add, using the keyword 'all'
  will attempt to add all channels for the VM:

  .. code-block:: console

    add_channels {vm_name} {list}|all

  Enable or disable the communication channels in {list}(comma-separated)
  for the specified VM, alternatively list can be replaced with keyword 'all'.
  Disabled channels will still receive packets on the host, however the commands
  they specify will be ignored. Set status to 'enabled' to begin processing requests again:

  .. code-block:: console

    set_channel_status {vm_name} {list}|all enabled|disabled

  Print to the CLI the information on the specified VM, the information
  lists the number of vCPUS, the pinning to pCPU(s) as a bit mask, along with
  any communication channels associated with each VM, along with the status of each channel:

  .. code-block:: console

    show_vm {vm_name}

  Set the binding of Virtual CPU on VM with name {vm_name}  to the Physical CPU mask:

  .. code-block:: console

    set_pcpu_mask {vm_name} {vcpu} {pcpu}

  Set the binding of Virtual CPU on VM to the Physical CPU:

  .. code-block:: console

    set_pcpu {vm_name} {vcpu} {pcpu}

Manual control and inspection can also be carried in relation CPU frequency scaling:

  Get the current frequency for each core specified in the mask:

  .. code-block:: console

    show_cpu_freq_mask {mask}

  Set the current frequency for the cores specified in {core_mask} by scaling each up/down/min/max:

  .. code-block:: console

    set_cpu_freq {core_mask} up|down|min|max

  Get the current frequency for the specified core:

  .. code-block:: console

    show_cpu_freq {core_num}

  Set the current frequency for the specified core by scaling up/down/min/max:

  .. code-block:: console

    set_cpu_freq {core_num} up|down|min|max

There are also some command line parameters for enabling the out-of-band
monitoring of branch ratio on cores doing busy polling via PMDs.

  .. code-block:: console

    --core-list {list of cores}

  When this parameter is used, the list of cores specified will monitor the ratio
  between branch hits and branch misses. A tightly polling PMD thread will have a
  very low branch ratio, so the core frequency will be scaled down to the minimum
  allowed value. When packets are received, the code path will alter, causing the
  branch ratio to increase. When the ratio goes above the ratio threshold, the
  core frequency will be scaled up to the maximum allowed value.

  .. code-block:: console

    --branch-ratio {ratio}

  The branch ratio is a floating point number that specifies the threshold at which
  to scale up or down for the given workload. The default branch ratio is 0.01,
  and will need to be adjusted for different workloads.



JSON API
~~~~~~~~

In addition to the command line interface for host command and a virtio-serial
interface for VM power policies, there is also a JSON interface through which
power commands and policies can be sent. This functionality adds a dependency
on the Jansson library, and the Jansson development package must be installed
on the system before the JSON parsing functionality is included in the app.
This is achieved by:

  .. code-block:: javascript

    apt-get install libjansson-dev

The command and package name may be different depending on your operating
system. It's worth noting that the app will successfully build without this
package present, but a warning is shown during compilation, and the JSON
parsing functionality will not be present in the app.

Sending a command or policy to the power manager application is achieved by
simply opening a fifo file, writing a JSON string to that fifo, and closing
the file.

The fifo is at /tmp/powermonitor/fifo

The JSON string can be a policy or instruction, and takes the following
format:

  .. code-block:: javascript

    {"packet_type": {
      "pair_1": value,
      "pair_2": value
    }}

The 'packet_type' header can contain one of two values, depending on
whether a policy or power command is being sent. The two possible values are
"policy" and "instruction", and the expected name-value pairs is different
depending on which type is being sent.

The pairs are the format of standard JSON name-value pairs. The value type
varies between the different name/value pairs, and may be integers, strings,
arrays, etc. Examples of policies follow later in this document. The allowed
names and value types are as follows:


:Pair Name: "name"
:Description: Name of the VM or Host. Allows the parser to associate the
  policy with the relevant VM or Host OS.
:Type: string
:Values: any valid string
:Required: yes
:Example:

    .. code-block:: javascript

      "name", "ubuntu2"


:Pair Name: "command"
:Description: The type of packet we're sending to the power manager. We can be
  creating or destroying a policy, or sending a direct command to adjust
  the frequency of a core, similar to the command line interface.
:Type: string
:Values:

  :CREATE: used when creating a new policy,
  :DESTROY: used when removing a policy,
  :POWER: used when sending an immediate command, max, min, etc.
:Required: yes
:Example:

    .. code-block:: javascript

      "command", "CREATE"


:Pair Name: "policy_type"
:Description: Type of policy to apply. Please see vm_power_manager documentation
  for more information on the types of policies that may be used.
:Type: string
:Values:

  :TIME: Time-of-day policy. Frequencies of the relevant cores are
    scaled up/down depending on busy and quiet hours.
  :TRAFFIC: This policy takes statistics from the NIC and scales up
    and down accordingly.
  :WORKLOAD: This policy looks at how heavily loaded the cores are,
    and scales up and down accordingly.
  :BRANCH_RATIO: This out-of-band policy can look at the ratio between
    branch hits and misses on a core, and is useful for detecting
    how much packet processing a core is doing.
:Required: only for CREATE/DESTROY command
:Example:

  .. code-block:: javascript

    "policy_type", "TIME"

:Pair Name: "busy_hours"
:Description: The hours of the day in which we scale up the cores for busy
  times.
:Type: array of integers
:Values: array with list of hour numbers, (0-23)
:Required: only for TIME policy
:Example:

  .. code-block:: javascript

    "busy_hours":[ 17, 18, 19, 20, 21, 22, 23 ]

:Pair Name: "quiet_hours"
:Description: The hours of the day in which we scale down the cores for quiet
  times.
:Type: array of integers
:Values: array with list of hour numbers, (0-23)
:Required: only for TIME policy
:Example:

  .. code-block:: javascript

    "quiet_hours":[ 2, 3, 4, 5, 6 ]

:Pair Name: "avg_packet_thresh"
:Description: Threshold below which the frequency will be set to min for
  the TRAFFIC policy. If the traffic rate is above this and below max, the
  frequency will be set to medium.
:Type: integer
:Values: The number of packets below which the TRAFFIC policy applies the
  minimum frequency, or medium frequency if between avg and max thresholds.
:Required: only for TRAFFIC policy
:Example:

  .. code-block:: javascript

    "avg_packet_thresh": 100000

:Pair Name: "max_packet_thresh"
:Description: Threshold above which the frequency will be set to max for
  the TRAFFIC policy
:Type: integer
:Values: The number of packets per interval above which the TRAFFIC policy
  applies the maximum frequency
:Required: only for TRAFFIC policy
:Example:

  .. code-block:: javascript

    "max_packet_thresh": 500000

:Pair Name: "core_list"
:Description: The cores to which to apply the policy.
:Type: array of integers
:Values: array with list of virtual CPUs.
:Required: only policy CREATE/DESTROY
:Example:

  .. code-block:: javascript

    "core_list":[ 10, 11 ]

:Pair Name: "workload"
:Description: When our policy is of type WORKLOAD, we need to specify how
  heavy our workload is.
:Type: string
:Values:

  :HIGH: For cores running workloads that require high frequencies
  :MEDIUM: For cores running workloads that require medium frequencies
  :LOW: For cores running workloads that require low frequencies
:Required: only for WORKLOAD policy types
:Example:

  .. code-block:: javascript

    "workload", "MEDIUM"

:Pair Name: "mac_list"
:Description: When our policy is of type TRAFFIC, we need to specify the
  MAC addresses that the host needs to monitor
:Type: string
:Values: array with a list of mac address strings.
:Required: only for TRAFFIC policy types
:Example:

  .. code-block:: javascript

    "mac_list":[ "de:ad:be:ef:01:01", "de:ad:be:ef:01:02" ]

:Pair Name: "unit"
:Description: the type of power operation to apply in the command
:Type: string
:Values:

  :SCALE_MAX: Scale frequency of this core to maximum
  :SCALE_MIN: Scale frequency of this core to minimum
  :SCALE_UP: Scale up frequency of this core
  :SCALE_DOWN: Scale down frequency of this core
  :ENABLE_TURBO: Enable Turbo Boost for this core
  :DISABLE_TURBO: Disable Turbo Boost for this core
:Required: only for POWER instruction
:Example:

  .. code-block:: javascript

    "unit", "SCALE_MAX"

:Pair Name: "resource_id"
:Description: The core to which to apply the power command.
:Type: integer
:Values: valid core id for VM or host OS.
:Required: only POWER instruction
:Example:

  .. code-block:: javascript

    "resource_id": 10

JSON API Examples
~~~~~~~~~~~~~~~~~

Profile create example:

  .. code-block:: javascript

    {"policy": {
      "name": "ubuntu",
      "command": "create",
      "policy_type": "TIME",
      "busy_hours":[ 17, 18, 19, 20, 21, 22, 23 ],
      "quiet_hours":[ 2, 3, 4, 5, 6 ],
      "core_list":[ 11 ]
    }}

Profile destroy example:

  .. code-block:: javascript

    {"policy": {
      "name": "ubuntu",
      "command": "destroy",
    }}

Power command example:

  .. code-block:: javascript

    {"instruction": {
      "name": "ubuntu",
      "command": "power",
      "unit": "SCALE_MAX",
      "resource_id": 10
    }}

To send a JSON string to the Power Manager application, simply paste the
example JSON string into a text file and cat it into the fifo:

  .. code-block:: console

    cat file.json >/tmp/powermonitor/fifo

The console of the Power Manager application should indicate the command that
was just received via the fifo.

Compiling and Running the Guest Applications
--------------------------------------------

l3fwd-power is one sample application that can be used with vm_power_manager.

A guest CLI is also provided for validating the setup.

For both l3fwd-power and guest CLI, the channels for the VM must be monitored by the
host application using the *add_channels* command on the host. This typically uses
the following commands in the host application:

.. code-block:: console

  vm_power> add_vm vmname
  vm_power> add_channels vmname all
  vm_power> set_channel_status vmname all enabled
  vm_power> show_vm vmname


Compiling
~~~~~~~~~

For information on compiling DPDK and the sample applications
see :doc:`compiling`.

For compiling and running l3fwd-power, see :doc:`l3_forward_power_man`.

The application is located in the ``guest_cli`` sub-directory under ``vm_power_manager``.

To build just the ``guest_vm_power_manager`` application using ``make``:

.. code-block:: console

  export RTE_SDK=/path/to/rte_sdk
  export RTE_TARGET=build
  cd ${RTE_SDK}/examples/vm_power_manager/guest_cli/
  make

The resulting binary will be ${RTE_SDK}/build/examples/guest_cli

.. Note::
  This sample application conditionally links in the Jansson JSON
  library, so if you are using a multilib or cross compile environment you
  may need to set the ``PKG_CONFIG_LIBDIR`` environmental variable to point to
  the relevant pkgconfig folder so that the correct library is linked in.

  For example, if you are building for a 32-bit target, you could find the
  correct directory using the following ``find`` command:

  .. code-block:: console

      # find /usr -type d -name pkgconfig
      /usr/lib/i386-linux-gnu/pkgconfig
      /usr/lib/x86_64-linux-gnu/pkgconfig

  Then use:

  .. code-block:: console

      export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig

  You then use the make command as normal, which should find the 32-bit
  version of the library, if it installed. If not, the application will
  be built without the JSON interface functionality.

To build just the ``vm_power_manager`` application using ``meson/ninja``:

.. code-block:: console

  export RTE_SDK=/path/to/rte_sdk
  cd ${RTE_SDK}
  meson build
  cd build
  ninja
  meson configure -Dexamples=vm_power_manager/guest_cli
  ninja

The resulting binary will be ${RTE_SDK}/build/examples/guest_cli

Running
~~~~~~~

The standard *EAL* command line parameters are required:

.. code-block:: console

 ./build/guest_vm_power_mgr [EAL options] -- [guest options]

The guest example uses a channel for each lcore enabled. For example,
to run on cores 0,1,2,3:

.. code-block:: console

 ./build/guest_vm_power_mgr -l 0-3

Optionally, there is a list of command line parameter should the user wish to send a power
policy down to the host application. These parameters are as follows:

  .. code-block:: console

    --vm-name {name of guest vm}

  This parameter allows the user to change the Virtual Machine name passed down to the
  host application via the power policy. The default is "ubuntu2"

  .. code-block:: console

    --vcpu-list {list vm cores}

  A comma-separated list of cores in the VM that the user wants the host application to
  monitor. The list of cores in any vm starts at zero, and these are mapped to the
  physical cores by the host application once the policy is passed down.
  Valid syntax includes individual cores '2,3,4', or a range of cores '2-4', or a
  combination of both '1,3,5-7'

  .. code-block:: console

    --busy-hours {list of busy hours}

  A comma-separated list of hours within which to set the core frequency to maximum.
  Valid syntax includes individual hours '2,3,4', or a range of hours '2-4', or a
  combination of both '1,3,5-7'. Valid hours are 0 to 23.

  .. code-block:: console

    --quiet-hours {list of quiet hours}

  A comma-separated list of hours within which to set the core frequency to minimum.
  Valid syntax includes individual hours '2,3,4', or a range of hours '2-4', or a
  combination of both '1,3,5-7'. Valid hours are 0 to 23.

  .. code-block:: console

    --policy {policy type}

  The type of policy. This can be one of the following values:
  TRAFFIC - based on incoming traffic rates on the NIC.
  TIME - busy/quiet hours policy.
  BRANCH_RATIO - uses branch ratio counters to determine core busyness.
  Not all parameters are needed for all policy types. For example, BRANCH_RATIO
  only needs the vcpu-list parameter, not any of the hours.


After successful initialization the user is presented with VM Power Manager Guest CLI:

.. code-block:: console

  vm_power(guest)>

To change the frequency of a lcore, use the set_cpu_freq command.
Where {core_num} is the lcore and channel to change frequency by scaling up/down/min/max.

.. code-block:: console

  set_cpu_freq {core_num} up|down|min|max

To start the application and configure the power policy, and send it to the host:

.. code-block:: console

 ./build/guest_vm_power_mgr -l 0-3 -n 4 -- --vm-name=ubuntu --policy=BRANCH_RATIO --vcpu-list=2-4

Once the VM Power Manager Guest CLI appears, issuing the 'send_policy now' command
will send the policy to the host:

.. code-block:: console

  send_policy now

Once the policy is sent to the host, the host application takes over the power monitoring
of the specified cores in the policy.

