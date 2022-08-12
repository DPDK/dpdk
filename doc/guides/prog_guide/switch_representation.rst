..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 6WIND S.A.

.. _switch_representation:

Switch Representation within DPDK Applications
==============================================

.. contents:: :local:

Introduction
------------

Network adapters with multiple physical ports and/or SR-IOV capabilities
usually support the offload of traffic steering rules between their virtual
functions (VFs), sub functions (SFs), physical functions (PFs) and ports.

Like for standard Ethernet switches, this involves a combination of
automatic MAC learning and manual configuration. For most purposes it is
managed by the host system and fully transparent to users and applications.

On the other hand, applications typically found on hypervisors that process
layer 2 (L2) traffic (such as OVS) need to steer traffic themselves
according on their own criteria.

Without a standard software interface to manage traffic steering rules
between VFs, SFs, PFs and the various physical ports of a given device,
applications cannot take advantage of these offloads; software processing is
mandatory even for traffic which ends up re-injected into the device it
originates from.

This document describes how such steering rules can be configured through
the DPDK flow API (**rte_flow**), with emphasis on the SR-IOV use case
(PF/VF steering) using a single physical port for clarity, however the same
logic applies to any number of ports without necessarily involving SR-IOV.

Sub Function
------------
Besides SR-IOV, Sub function is a portion of the PCI device, a SF netdev
has its own dedicated queues(txq, rxq). A SF netdev supports E-Switch
representation offload similar to existing PF and VF representors.
A SF shares PCI level resources with other SFs and/or with its parent PCI
function.

Sub function is created on-demand, coexists with VFs. Number of SFs is
limited by hardware resources.

Port Representors
-----------------

In many cases, traffic steering rules cannot be determined in advance;
applications usually have to process a bit of traffic in software before
thinking about offloading specific flows to hardware.

Applications therefore need the ability to receive and inject traffic to
various device endpoints (other VFs, SFs, PFs or physical ports) before
connecting them together. Device drivers must provide means to hook the
"other end" of these endpoints and to refer them when configuring flow
rules.

This role is left to so-called "port representors" (also known as "VF
representors" in the specific context of VFs, "SF representors" in the
specific context of SFs), which are to DPDK what the Ethernet switch
device driver model (**switchdev**) [1]_ is to Linux, and which can be
thought as a software "patch panel" front-end for applications.

- DPDK port representors are implemented as additional virtual Ethernet
  device (**ethdev**) instances, spawned on an as needed basis through
  configuration parameters passed to the driver of the underlying
  device using devargs.

::

   -a pci:dbdf,representor=vf0
   -a pci:dbdf,representor=vf[0-3]
   -a pci:dbdf,representor=vf[0,5-11]
   -a pci:dbdf,representor=sf1
   -a pci:dbdf,representor=sf[0-1023]
   -a pci:dbdf,representor=sf[0,2-1023]

- As virtual devices, they may be more limited than their physical
  counterparts, for instance by exposing only a subset of device
  configuration callbacks and/or by not necessarily having Rx/Tx capability.

- Among other things, they can be used to assign MAC addresses to the
  resource they represent.

- Applications can tell port representors apart from other physical of virtual
  port by checking the dev_flags field within their device information
  structure for the RTE_ETH_DEV_REPRESENTOR bit-field.

.. code-block:: c

  struct rte_eth_dev_info {
      ...
      uint32_t dev_flags; /**< Device flags */
      ...
  };

- The device or group relationship of ports can be discovered using the
  switch ``domain_id`` field within the devices switch information structure. By
  default the switch ``domain_id`` of a port will be
  ``RTE_ETH_DEV_SWITCH_DOMAIN_ID_INVALID`` to indicate that the port doesn't
  support the concept of a switch domain, but ports which do support the concept
  will be allocated a unique switch ``domain_id``, ports within the same switch
  domain will share the same ``domain_id``. The switch ``port_id`` is used to
  specify the port_id in terms of the switch, so in the case of SR-IOV devices
  the switch ``port_id`` would represent the virtual function identifier of the
  port.

.. code-block:: c

   /**
    * Ethernet device associated switch information
    */
   struct rte_eth_switch_info {
       const char *name; /**< switch name */
       uint16_t domain_id; /**< switch domain id */
       uint16_t port_id; /**< switch port id */
   };


.. [1] `Ethernet switch device driver model (switchdev)
       <https://www.kernel.org/doc/Documentation/networking/switchdev.txt>`_

- For some PMDs, memory usage of representors is huge when number of
  representor grows, mbufs are allocated for each descriptor of Rx queue.
  Polling large number of ports brings more CPU load, cache miss and
  latency. Shared Rx queue can be used to share Rx queue between PF and
  representors among same Rx domain. ``RTE_ETH_DEV_CAPA_RXQ_SHARE`` in
  device info is used to indicate the capability. Setting non-zero share
  group in Rx queue configuration to enable share, share_qid is used to
  identify the shared Rx queue in group. Polling any member port can
  receive packets of all member ports in the group, port ID is saved in
  ``mbuf.port``.

Basic SR-IOV
------------

"Basic" in the sense that it is not managed by applications, which
nonetheless expect traffic to flow between the various endpoints and the
outside as if everything was linked by an Ethernet hub.

The following diagram pictures a setup involving a device with one PF, two
VFs and one shared physical port

::

       .-------------.                 .-------------. .-------------.
       | hypervisor  |                 |    VM 1     | |    VM 2     |
       | application |                 | application | | application |
       `--+----------'                 `----------+--' `--+----------'
          |                                       |       |
    .-----+-----.                                 |       |
    | port_id 3 |                                 |       |
    `-----+-----'                                 |       |
          |                                       |       |
        .-+--.                                .---+--. .--+---.
        | PF |                                | VF 1 | | VF 2 |
        `-+--'                                `---+--' `--+---'
          |                                       |       |
          `---------.     .-----------------------'       |
                    |     |     .-------------------------'
                    |     |     |
                 .--+-----+-----+--.
                 | interconnection |
                 `--------+--------'
                          |
                     .----+-----.
                     | physical |
                     |  port 0  |
                     `----------'

- A DPDK application running on the hypervisor owns the PF device, which is
  arbitrarily assigned port index 3.

- Both VFs are assigned to VMs and used by unknown applications; they may be
  DPDK-based or anything else.

- Interconnection is not necessarily done through a true Ethernet switch and
  may not even exist as a separate entity. The role of this block is to show
  that something brings PF, VFs and physical ports together and enables
  communication between them, with a number of built-in restrictions.

Subsequent sections in this document describe means for DPDK applications
running on the hypervisor to freely assign specific flows between PF, VFs
and physical ports based on traffic properties, by managing this
interconnection.

Controlled SR-IOV
-----------------

Initialization
~~~~~~~~~~~~~~

When a DPDK application gets assigned a PF device and is deliberately not
started in `basic SR-IOV`_ mode, any traffic coming from physical ports is
received by PF according to default rules, while VFs remain isolated.

::

       .-------------.                 .-------------. .-------------.
       | hypervisor  |                 |    VM 1     | |    VM 2     |
       | application |                 | application | | application |
       `--+----------'                 `----------+--' `--+----------'
          |                                       |       |
    .-----+-----.                                 |       |
    | port_id 3 |                                 |       |
    `-----+-----'                                 |       |
          |                                       |       |
        .-+--.                                .---+--. .--+---.
        | PF |                                | VF 1 | | VF 2 |
        `-+--'                                `------' `------'
          |
          `-----.
                |
             .--+----------------------.
             | managed interconnection |
             `------------+------------'
                          |
                     .----+-----.
                     | physical |
                     |  port 0  |
                     `----------'

In this mode, interconnection must be configured by the application to
enable VF communication, for instance by explicitly directing traffic with a
given destination MAC address to VF 1 and allowing that with the same source
MAC address to come out of it.

For this to work, hypervisor applications need a way to refer to either VF 1
or VF 2 in addition to the PF. This is addressed by `VF representors`_.

VF Representors
~~~~~~~~~~~~~~~

VF representors are virtual but standard DPDK network devices (albeit with
limited capabilities) created by PMDs when managing a PF device.

Since they represent VF instances used by other applications, configuring
them (e.g. assigning a MAC address or setting up promiscuous mode) affects
interconnection accordingly. If supported, they may also be used as two-way
communication ports with VFs (assuming **switchdev** topology)


::

       .-------------.                 .-------------. .-------------.
       | hypervisor  |                 |    VM 1     | |    VM 2     |
       | application |                 | application | | application |
       `--+---+---+--'                 `----------+--' `--+----------'
          |   |   |                               |       |
          |   |   `-------------------.           |       |
          |   `---------.             |           |       |
          |             |             |           |       |
    .-----+-----. .-----+-----. .-----+-----.     |       |
    | port_id 3 | | port_id 4 | | port_id 5 |     |       |
    `-----+-----' `-----+-----' `-----+-----'     |       |
          |             |             |           |       |
        .-+--.    .-----+-----. .-----+-----. .---+--. .--+---.
        | PF |    | VF 1 rep. | | VF 2 rep. | | VF 1 | | VF 2 |
        `-+--'    `-----+-----' `-----+-----' `---+--' `--+---'
          |             |             |           |       |
          |             |   .---------'           |       |
          `-----.       |   |   .-----------------'       |
                |       |   |   |   .---------------------'
                |       |   |   |   |
             .--+-------+---+---+---+--.
             | managed interconnection |
             `------------+------------'
                          |
                     .----+-----.
                     | physical |
                     |  port 0  |
                     `----------'

- VF representors are assigned arbitrary port indices 4 and 5 in the
  hypervisor application and are respectively associated with VF 1 and VF 2.

- They can't be dissociated; even if VF 1 and VF 2 were not connected,
  representors could still be used for configuration.

- In this context, port index 3 can be thought as a representor for physical
  port 0.

As previously described, the "interconnection" block represents a logical
concept. Interconnection occurs when hardware configuration enables traffic
flows from one place to another (e.g. physical port 0 to VF 1) according to
some criteria.

This is discussed in more detail in `traffic steering`_.

Traffic Steering
~~~~~~~~~~~~~~~~

In the following diagram, each meaningful traffic origin or endpoint as seen
by the hypervisor application is tagged with a unique letter from A to F.

::

       .-------------.                 .-------------. .-------------.
       | hypervisor  |                 |    VM 1     | |    VM 2     |
       | application |                 | application | | application |
       `--+---+---+--'                 `----------+--' `--+----------'
          |   |   |                               |       |
          |   |   `-------------------.           |       |
          |   `---------.             |           |       |
          |             |             |           |       |
    .----(A)----. .----(B)----. .----(C)----.     |       |
    | port_id 3 | | port_id 4 | | port_id 5 |     |       |
    `-----+-----' `-----+-----' `-----+-----'     |       |
          |             |             |           |       |
        .-+--.    .-----+-----. .-----+-----. .---+--. .--+---.
        | PF |    | VF 1 rep. | | VF 2 rep. | | VF 1 | | VF 2 |
        `-+--'    `-----+-----' `-----+-----' `--(D)-' `-(E)--'
          |             |             |           |       |
          |             |   .---------'           |       |
          `-----.       |   |   .-----------------'       |
                |       |   |   |   .---------------------'
                |       |   |   |   |
             .--+-------+---+---+---+--.
             | managed interconnection |
             `------------+------------'
                          |
                     .---(F)----.
                     | physical |
                     |  port 0  |
                     `----------'

- **A**: PF device.
- **B**: port representor for VF 1.
- **C**: port representor for VF 2.
- **D**: VF 1 proper.
- **E**: VF 2 proper.
- **F**: physical port.

Although uncommon, some devices do not enforce a one to one mapping between
PF and physical ports. For instance, by default all ports of **mlx4**
adapters are available to all their PF/VF instances, in which case
additional ports appear next to **F** in the above diagram.

Assuming no interconnection is provided by default in this mode, setting up
a `basic SR-IOV`_ configuration involving physical port 0 could be broken
down as:

PF:

- **A to F**: let everything through.
- **F to A**: PF MAC as destination.

VF 1:

- **A to D**, **E to D** and **F to D**: VF 1 MAC as destination.
- **D to A**: VF 1 MAC as source and PF MAC as destination.
- **D to E**: VF 1 MAC as source and VF 2 MAC as destination.
- **D to F**: VF 1 MAC as source.

VF 2:

- **A to E**, **D to E** and **F to E**: VF 2 MAC as destination.
- **E to A**: VF 2 MAC as source and PF MAC as destination.
- **E to D**: VF 2 MAC as source and VF 1 MAC as destination.
- **E to F**: VF 2 MAC as source.

Devices may additionally support advanced matching criteria such as
IPv4/IPv6 addresses or TCP/UDP ports.

The combination of matching criteria with target endpoints fits well with
**rte_flow** [6]_, which expresses flow rules as combinations of patterns
and actions.

Enhancing **rte_flow** with the ability to make flow rules match and target
these endpoints provides a standard interface to manage their
interconnection without introducing new concepts and whole new API to
implement them. This is described in `flow API (rte_flow)`_.

.. [6] :doc:`Generic flow API (rte_flow) <rte_flow>`

Flow API (rte_flow)
-------------------

Extensions
~~~~~~~~~~

Compared to creating a brand new dedicated interface, **rte_flow** was
deemed flexible enough to manage representor traffic only with minor
extensions:

- Using physical ports, PF, SF, VF or port representors as targets.

- Affecting traffic that is not necessarily addressed to the DPDK port ID a
  flow rule is associated with (e.g. forcing VF traffic redirection to PF).

For advanced uses:

- Rule-based packet counters.

- The ability to combine several identical actions for traffic duplication
  (e.g. VF representor in addition to a physical port).

- Dedicated actions for traffic encapsulation / decapsulation before
  reaching an endpoint.

Traffic Direction
~~~~~~~~~~~~~~~~~

From an application standpoint, "ingress" and "egress" flow rule attributes
apply to the DPDK port ID they are associated with. They select a traffic
direction for matching patterns, but have no impact on actions.

When matching traffic coming from or going to a different place than the
immediate port ID a flow rule is associated with, these attributes keep
their meaning while applying to the chosen origin, as highlighted by the
following diagram

::

       .-------------.                 .-------------. .-------------.
       | hypervisor  |                 |    VM 1     | |    VM 2     |
       | application |                 | application | | application |
       `--+---+---+--'                 `----------+--' `--+----------'
          |   |   |                               |       |
          |   |   `-------------------.           |       |
          |   `---------.             |           |       |
          | ^           | ^           | ^         |       |
          | | ingress   | | ingress   | | ingress |       |
          | | egress    | | egress    | | egress  |       |
          | v           | v           | v         |       |
    .----(A)----. .----(B)----. .----(C)----.     |       |
    | port_id 3 | | port_id 4 | | port_id 5 |     |       |
    `-----+-----' `-----+-----' `-----+-----'     |       |
          |             |             |           |       |
        .-+--.    .-----+-----. .-----+-----. .---+--. .--+---.
        | PF |    | VF 1 rep. | | VF 2 rep. | | VF 1 | | VF 2 |
        `-+--'    `-----+-----' `-----+-----' `--(D)-' `-(E)--'
          |             |             |         ^ |       | ^
          |             |             |  egress | |       | | egress
          |             |             | ingress | |       | | ingress
          |             |   .---------'         v |       | v
          `-----.       |   |   .-----------------'       |
                |       |   |   |   .---------------------'
                |       |   |   |   |
             .--+-------+---+---+---+--.
             | managed interconnection |
             `------------+------------'
                        ^ |
                ingress | |
                 egress | |
                        v |
                     .---(F)----.
                     | physical |
                     |  port 0  |
                     `----------'

Ingress and egress are defined as relative to the application creating the
flow rule.

For instance, matching traffic sent by VM 2 would be done through an ingress
flow rule on VF 2 (**E**). Likewise for incoming traffic on physical port
(**F**). This also applies to **C** and **A** respectively.

Transferring Traffic
~~~~~~~~~~~~~~~~~~~~

Without Port Representors
^^^^^^^^^^^^^^^^^^^^^^^^^

`Traffic direction`_ describes how an application could match traffic coming
from or going to a specific place reachable from a DPDK port ID. This makes
sense when the traffic in question is normally seen (i.e. sent or received)
by the application creating the flow rule.

However, if there is an entity (VF **D**, for instance) not associated with
a DPDK port (representor), the application (**A**) won't be able to match
traffic generated by such entity. The traffic goes directly to its
default destination (to physical port **F**, for instance).

::

    .-------------. .-------------.
    | hypervisor  | |    VM 1     |
    | application | | application |
    `------+------' `--+----------'
           |           | | traffic
     .----(A)----.     | v
     | port_id 3 |     |
     `-----+-----'     |
           |           |
           |           |
         .-+--.    .---+--.
         | PF |    | VF 1 |
         `-+--'    `--(D)-'
           |           | | traffic
           |           | v
        .--+-----------+--.
        | interconnection |
        `--------+--------'
                 | | traffic
                 | v
            .---(F)----.
            | physical |
            |  port 0  |
            `----------'


With Port Representors
^^^^^^^^^^^^^^^^^^^^^^

When port representors exist, implicit flow rules with the "transfer"
attribute (described in `without port representors`_) are be assumed to
exist between them and their represented resources. These may be immutable.

In this case, traffic is received by default through the representor and
neither the "transfer" attribute nor traffic origin in flow rule patterns
are necessary. They simply have to be created on the representor port
directly and may target a different representor as described
in `PORT_REPRESENTOR Action`_.

Implicit traffic flow with port representor

::

       .-------------.   .-------------.
       | hypervisor  |   |    VM 1     |
       | application |   | application |
       `--+-------+--'   `----------+--'
          |       | ^               | | traffic
          |       | | traffic       | v
          |       `-----.           |
          |             |           |
    .----(A)----. .----(B)----.     |
    | port_id 3 | | port_id 4 |     |
    `-----+-----' `-----+-----'     |
          |             |           |
        .-+--.    .-----+-----. .---+--.
        | PF |    | VF 1 rep. | | VF 1 |
        `-+--'    `-----+-----' `--(D)-'
          |             |           |
       .--|-------------|-----------|--.
       |  |             |           |  |
       |  |             `-----------'  |
       |  |              <-- traffic   |
       `--|----------------------------'
          |
     .---(F)----.
     | physical |
     |  port 0  |
     `----------'

Pattern Items And Actions
~~~~~~~~~~~~~~~~~~~~~~~~~

PORT_REPRESENTOR Pattern Item
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches traffic entering the embedded switch from the given ethdev.

- Matches **A**, **B** or **C** in `traffic steering`_.

PORT_REPRESENTOR Action
^^^^^^^^^^^^^^^^^^^^^^^

At embedded switch level, sends matching traffic to the given ethdev.

- Targets **A**, **B** or **C** in `traffic steering`_.

REPRESENTED_PORT Pattern Item
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches traffic entering the embedded switch from
the entity represented by the given ethdev.

- Matches **D**, **E** or **F** in `traffic steering`_.

REPRESENTED_PORT Action
^^^^^^^^^^^^^^^^^^^^^^^

At embedded switch level, send matching traffic to
the entity represented by the given ethdev.

- Targets **D**, **E** or **F** in `traffic steering`_.

PORT Pattern Item
^^^^^^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) a physical
port of the underlying device.

Using this pattern item without specifying a port index matches the physical
port associated with the current DPDK port ID by default. As described in
`traffic steering`_, specifying it should be rarely needed.

- Matches **F** in `traffic steering`_.

PORT Action
^^^^^^^^^^^

Directs matching traffic to a given physical port index.

- Targets **F** in `traffic steering`_.

PORT_ID Pattern Item
^^^^^^^^^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) a given DPDK
port ID.

Normally only supported if the port ID in question is known by the
underlying PMD and related to the device the flow rule is created against.

This must not be confused with the `PORT pattern item`_ which refers to the
physical port of a device. ``PORT_ID`` refers to a ``struct rte_eth_dev``
object on the application side (also known as "port representor" depending
on the kind of underlying device).

- Matches **A**, **B** or **C** in `traffic steering`_.

PORT_ID Action
^^^^^^^^^^^^^^

Directs matching traffic to a given DPDK port ID.

Same restrictions as `PORT_ID pattern item`_.

- Targets **A**, **B** or **C** in `traffic steering`_.

PF Action
^^^^^^^^^

Directs matching traffic to the physical function of the current device.

- Targets **A** in `traffic steering`_.

VF Pattern Item
^^^^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) a given
virtual function of the current device.

If supported, should work even if the virtual function is not managed by
the application and thus not associated with a DPDK port ID. Its behavior is
otherwise similar to `PORT_ID pattern item`_ using VF port ID.

Note this pattern item does not match VF representors traffic which, as
separate entities, should be addressed through their own port IDs.

- Matches **D** or **E** in `traffic steering`_.

VF Action
^^^^^^^^^

Directs matching traffic to a given virtual function of the current device.

Same restrictions as `VF pattern item`_.

- Targets **D** or **E** in `traffic steering`_.

\*_ENCAP actions
^^^^^^^^^^^^^^^^

These actions are named according to the protocol they encapsulate traffic
with (e.g. ``VXLAN_ENCAP``) and using specific parameters (e.g. VNI for
VXLAN).

While they modify traffic and can be used multiple times (order matters),
unlike `PORT_REPRESENTOR Action`_ and friends, they don't impact on steering.

As described in `actions order and repetition`_ this means they are useless
if used alone in an action list, the resulting traffic gets dropped unless
combined with either ``PASSTHRU`` or other endpoint-targeting actions.

\*_DECAP actions
^^^^^^^^^^^^^^^^

They perform the reverse of `\*_ENCAP actions`_ by popping protocol headers
from traffic instead of pushing them. They can be used multiple times as
well.

Note that using these actions on non-matching traffic results in undefined
behavior. It is recommended to match the protocol headers to decapsulate on
the pattern side of a flow rule in order to use these actions or otherwise
make sure only matching traffic goes through.

Actions Order and Repetition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Flow rules are currently restricted to at most a single action of each
supported type, performed in an unpredictable order (or all at once). To
repeat actions in a predictable fashion, applications have to make rules
pass-through and use priority levels.

It's now clear that PMD support for chaining multiple non-terminating flow
rules of varying priority levels is prohibitively difficult to implement
compared to simply allowing multiple identical actions performed in a
defined order by a single flow rule.

- This change is required to support protocol encapsulation offloads and the
  ability to perform them multiple times (e.g. VLAN then VXLAN).

- It makes the ``DUP`` action redundant since multiple ``QUEUE`` actions can
  be combined for duplication.

- The (non-)terminating property of actions must be discarded. Instead, flow
  rules themselves must be considered terminating by default (i.e. dropping
  traffic if there is no specific target) unless a ``PASSTHRU`` action is
  also specified.

Switching Examples
------------------

This section provides practical examples based on the established testpmd
flow command syntax [2]_, in the context described in `traffic steering`_

::

      .-------------.                 .-------------. .-------------.
      | hypervisor  |                 |    VM 1     | |    VM 2     |
      | application |                 | application | | application |
      `--+---+---+--'                 `----------+--' `--+----------'
         |   |   |                               |       |
         |   |   `-------------------.           |       |
         |   `---------.             |           |       |
         |             |             |           |       |
   .----(A)----. .----(B)----. .----(C)----.     |       |
   | port_id 3 | | port_id 4 | | port_id 5 |     |       |
   `-----+-----' `-----+-----' `-----+-----'     |       |
         |             |             |           |       |
       .-+--.    .-----+-----. .-----+-----. .---+--. .--+---.
       | PF |    | VF 1 rep. | | VF 2 rep. | | VF 1 | | VF 2 |
       `-+--'    `-----+-----' `-----+-----' `--(D)-' `-(E)--'
         |             |             |           |       |
         |             |   .---------'           |       |
         `-----.       |   |   .-----------------'       |
               |       |   |   |   .---------------------'
               |       |   |   |   |
            .--|-------|---|---|---|--.
            |  |       |   `---|---'  |
            |  |       `-------'      |
            |  `---------.            |
            `------------|------------'
                         |
                    .---(F)----.
                    | physical |
                    |  port 0  |
                    `----------'

By default, PF (**A**) can communicate with the physical port it is
associated with (**F**), while VF 1 (**D**) and VF 2 (**E**) are isolated
and restricted to communicate with the hypervisor application through their
respective representors (**B** and **C**) if supported.

Examples in subsequent sections apply to hypervisor applications only and
are based on port representors **A**, **B** and **C**.

.. [2] :ref:`Flow syntax <testpmd_rte_flow>`

Associating VF 1 with Physical Port 0
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Assign all port traffic (**F**) to VF 1 (**D**) indiscriminately through
their representors

::

   flow create 3 transfer
      pattern represented_port ethdev_port_id is 3 / end
      actions represented_port ethdev_port_id 4 / end

::

   flow create 3 transfer
      pattern represented_port ethdev_port_id is 4 / end
      actions represented_port ethdev_port_id 3 / end


Sharing Broadcasts
~~~~~~~~~~~~~~~~~~

From outside to PF and VFs

::

   flow create 3 transfer
      pattern
         represented_port ethdev_port_id is 3 /
         eth dst is ff:ff:ff:ff:ff:ff /
         end
      actions
         port_representor ethdev_port_id 3 /
         represented_port ethdev_port_id 4 /
         represented_port ethdev_port_id 5 /
         end

Note ``port_representor ethdev_port_id 3`` is necessary otherwise only VFs would receive matching
traffic.

From PF to outside and VFs

::

   flow create 3 transfer
      pattern
         port_representor ethdev_port_id is 3 /
         eth dst is ff:ff:ff:ff:ff:ff /
         end
      actions
         represented_port ethdev_port_id 3 /
         represented_port ethdev_port_id 4 /
         represented_port ethdev_port_id 5 /
         end

From VFs to outside and PF

::

   flow create 3 transfer
      pattern
         represented_port ethdev_port_id is 4 /
         eth dst is ff:ff:ff:ff:ff:ff /
         end
      actions
         represented_port ethdev_port_id 3 /
         port_representor ethdev_port_id 3 /
         end

   flow create 3 transfer
      pattern
         represented_port ethdev_port_id is 5 /
         eth dst is ff:ff:ff:ff:ff:ff /
         end
      actions
         represented_port ethdev_port_id 3 /
         port_representor ethdev_port_id 3 /
         end

Similar ``33:33:*`` rules based on known MAC addresses should be added for
IPv6 traffic.

Encapsulating VF 2 Traffic in VXLAN
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Assuming pass-through flow rules are supported

::

   flow create 5 ingress
      pattern eth / end
      actions vxlan_encap vni 42 / passthru / end

::

   flow create 5 egress
      pattern vxlan vni is 42 / end
      actions vxlan_decap / passthru / end

Here ``passthru`` is needed since as described in `actions order and
repetition`_, flow rules are otherwise terminating; if supported, a rule
without a target endpoint will drop traffic.

Without pass-through support, ingress encapsulation on the destination
endpoint might not be supported and action list must provide one

::

   flow create 3 transfer
      pattern represented_port ethdev_port_id is 5 / end
      actions vxlan_encap vni 42 / represented_port ethdev_port_id 3 / end

   flow create 3 transfer
      pattern
         represented_port ethdev_port_id is 3 /
         vxlan vni is 42 /
         end
      actions vxlan_decap / represented_port ethdev_port_id 5 / end
