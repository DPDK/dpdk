..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 6WIND S.A.
    Copyright 2016 Mellanox Technologies, Ltd

Generic flow API (rte_flow)
===========================

Overview
--------

This API provides a generic means to configure hardware to match specific
ingress or egress traffic, alter its fate and query related counters
according to any number of user-defined rules.

It is named *rte_flow* after the prefix used for all its symbols, and is
defined in ``rte_flow.h``.

- Matching can be performed on packet data (protocol headers, payload) and
  properties (e.g. associated physical port, virtual device function ID).

- Possible operations include dropping traffic, diverting it to specific
  queues, to virtual/physical device functions or ports, performing tunnel
  offloads, adding marks and so on.

It is slightly higher-level than the legacy filtering framework which it
encompasses and supersedes (including all functions and filter types) in
order to expose a single interface with an unambiguous behavior that is
common to all poll-mode drivers (PMDs).

Flow rule
---------

Description
~~~~~~~~~~~

A flow rule is the combination of attributes with a matching pattern and a
list of actions. Flow rules form the basis of this API.

Flow rules can have several distinct actions (such as counting,
encapsulating, decapsulating before redirecting packets to a particular
queue, etc.), instead of relying on several rules to achieve this and having
applications deal with hardware implementation details regarding their
order.

Support for different priority levels on a rule basis is provided, for
example in order to force a more specific rule to come before a more generic
one for packets matched by both. However hardware support for more than a
single priority level cannot be guaranteed. When supported, the number of
available priority levels is usually low, which is why they can also be
implemented in software by PMDs (e.g. missing priority levels may be
emulated by reordering rules).

In order to remain as hardware-agnostic as possible, by default all rules
are considered to have the same priority, which means that the order between
overlapping rules (when a packet is matched by several filters) is
undefined.

PMDs may refuse to create overlapping rules at a given priority level when
they can be detected (e.g. if a pattern matches an existing filter).

Thus predictable results for a given priority level can only be achieved
with non-overlapping rules, using perfect matching on all protocol layers.

Flow rules can also be grouped, the flow rule priority is specific to the
group they belong to. All flow rules in a given group are thus processed within
the context of that group. Groups are not linked by default, so the logical
hierarchy of groups must be explicitly defined by flow rules themselves in each
group using the JUMP action to define the next group to redirect too. Only flow
rules defined in the default group 0 are guarantee to be matched against, this
makes group 0 the origin of any group hierarchy defined by an application.

Support for multiple actions per rule may be implemented internally on top
of non-default hardware priorities, as a result both features may not be
simultaneously available to applications.

Considering that allowed pattern/actions combinations cannot be known in
advance and would result in an impractically large number of capabilities to
expose, a method is provided to validate a given rule from the current
device configuration state.

This enables applications to check if the rule types they need is supported
at initialization time, before starting their data path. This method can be
used anytime, its only requirement being that the resources needed by a rule
should exist (e.g. a target RX queue should be configured first).

Each defined rule is associated with an opaque handle managed by the PMD,
applications are responsible for keeping it. These can be used for queries
and rules management, such as retrieving counters or other data and
destroying them.

To avoid resource leaks on the PMD side, handles must be explicitly
destroyed by the application before releasing associated resources such as
queues and ports.

The following sections cover:

- **Attributes** (represented by ``struct rte_flow_attr``): properties of a
  flow rule such as its direction (ingress or egress) and priority.

- **Pattern item** (represented by ``struct rte_flow_item``): part of a
  matching pattern that either matches specific packet data or traffic
  properties. It can also describe properties of the pattern itself, such as
  inverted matching.

- **Matching pattern**: traffic properties to look for, a combination of any
  number of items.

- **Actions** (represented by ``struct rte_flow_action``): operations to
  perform whenever a packet is matched by a pattern.

Attributes
~~~~~~~~~~

Attribute: Group
^^^^^^^^^^^^^^^^

Flow rules can be grouped by assigning them a common group number. Groups
allow a logical hierarchy of flow rule groups (tables) to be defined. These
groups can be supported virtually in the PMD or in the physical device.
Group 0 is the default group and this is the only group which flows are
guarantee to matched against, all subsequent groups can only be reached by
way of the JUMP action from a matched flow rule.

Although optional, applications are encouraged to group similar rules as
much as possible to fully take advantage of hardware capabilities
(e.g. optimized matching) and work around limitations (e.g. a single pattern
type possibly allowed in a given group), while being aware that the groups
hierarchies must be programmed explicitly.

Note that support for more than a single group is not guaranteed.

Attribute: Priority
^^^^^^^^^^^^^^^^^^^

A priority level can be assigned to a flow rule, lower values
denote higher priority, with 0 as the maximum.

Priority levels are arbitrary and up to the application, they do
not need to be contiguous nor start from 0, however the maximum number
varies between devices and may be affected by existing flow rules.

A flow which matches multiple rules in the same group will always matched by
the rule with the highest priority in that group.

If a packet is matched by several rules of a given group for a given
priority level, the outcome is undefined. It can take any path, may be
duplicated or even cause unrecoverable errors.

Note that support for more than a single priority level is not guaranteed.

Attribute: Traffic direction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Flow rule patterns apply to inbound and/or outbound traffic.

In the context of this API, **ingress** and **egress** respectively stand
for **inbound** and **outbound** based on the standpoint of the application
creating a flow rule.

There are no exceptions to this definition.

Several pattern items and actions are valid and can be used in both
directions. At least one direction must be specified.

Specifying both directions at once for a given rule is not recommended but
may be valid in a few cases (e.g. shared counters).

Attribute: Transfer
^^^^^^^^^^^^^^^^^^^

Instead of simply matching the properties of traffic as it would appear on a
given DPDK port ID, enabling this attribute transfers a flow rule to the
lowest possible level of any device endpoints found in the pattern.

When supported, this effectively enables an application to reroute traffic
not necessarily intended for it (e.g. coming from or addressed to different
physical ports, VFs or applications) at the device level.

It complements the behavior of some pattern items such as `Item: PHY_PORT`_
and is meaningless without them.

When transferring flow rules, **ingress** and **egress** attributes
(`Attribute: Traffic direction`_) keep their original meaning, as if
processing traffic emitted or received by the application.

Pattern item
~~~~~~~~~~~~

Pattern items fall in two categories:

- Matching protocol headers and packet data, usually associated with a
  specification structure. These must be stacked in the same order as the
  protocol layers to match inside packets, starting from the lowest.

- Matching meta-data or affecting pattern processing, often without a
  specification structure. Since they do not match packet contents, their
  position in the list is usually not relevant.

Item specification structures are used to match specific values among
protocol fields (or item properties). Documentation describes for each item
whether they are associated with one and their type name if so.

Up to three structures of the same type can be set for a given item:

- ``spec``: values to match (e.g. a given IPv4 address).

- ``last``: upper bound for an inclusive range with corresponding fields in
  ``spec``.

- ``mask``: bit-mask applied to both ``spec`` and ``last`` whose purpose is
  to distinguish the values to take into account and/or partially mask them
  out (e.g. in order to match an IPv4 address prefix).

Usage restrictions and expected behavior:

- Setting either ``mask`` or ``last`` without ``spec`` is an error.

- Field values in ``last`` which are either 0 or equal to the corresponding
  values in ``spec`` are ignored; they do not generate a range. Nonzero
  values lower than those in ``spec`` are not supported.

- Setting ``spec`` and optionally ``last`` without ``mask`` causes the PMD
  to use the default mask defined for that item (defined as
  ``rte_flow_item_{name}_mask`` constants).

- Not setting any of them (assuming item type allows it) is equivalent to
  providing an empty (zeroed) ``mask`` for broad (nonspecific) matching.

- ``mask`` is a simple bit-mask applied before interpreting the contents of
  ``spec`` and ``last``, which may yield unexpected results if not used
  carefully. For example, if for an IPv4 address field, ``spec`` provides
  *10.1.2.3*, ``last`` provides *10.3.4.5* and ``mask`` provides
  *255.255.0.0*, the effective range becomes *10.1.0.0* to *10.3.255.255*.

Example of an item specification matching an Ethernet header:

.. _table_rte_flow_pattern_item_example:

.. table:: Ethernet item

   +----------+----------+--------------------+
   | Field    | Subfield | Value              |
   +==========+==========+====================+
   | ``spec`` | ``src``  | ``00:01:02:03:04`` |
   |          +----------+--------------------+
   |          | ``dst``  | ``00:2a:66:00:01`` |
   |          +----------+--------------------+
   |          | ``type`` | ``0x22aa``         |
   +----------+----------+--------------------+
   | ``last`` | unspecified                   |
   +----------+----------+--------------------+
   | ``mask`` | ``src``  | ``00:ff:ff:ff:00`` |
   |          +----------+--------------------+
   |          | ``dst``  | ``00:00:00:00:ff`` |
   |          +----------+--------------------+
   |          | ``type`` | ``0x0000``         |
   +----------+----------+--------------------+

Non-masked bits stand for any value (shown as ``?`` below), Ethernet headers
with the following properties are thus matched:

- ``src``: ``??:01:02:03:??``
- ``dst``: ``??:??:??:??:01``
- ``type``: ``0x????``

Matching pattern
~~~~~~~~~~~~~~~~

A pattern is formed by stacking items starting from the lowest protocol
layer to match. This stacking restriction does not apply to meta items which
can be placed anywhere in the stack without affecting the meaning of the
resulting pattern.

Patterns are terminated by END items.

Examples:

.. _table_rte_flow_tcpv4_as_l4:

.. table:: TCPv4 as L4

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | Ethernet |
   +-------+----------+
   | 1     | IPv4     |
   +-------+----------+
   | 2     | TCP      |
   +-------+----------+
   | 3     | END      |
   +-------+----------+

|

.. _table_rte_flow_tcpv6_in_vxlan:

.. table:: TCPv6 in VXLAN

   +-------+------------+
   | Index | Item       |
   +=======+============+
   | 0     | Ethernet   |
   +-------+------------+
   | 1     | IPv4       |
   +-------+------------+
   | 2     | UDP        |
   +-------+------------+
   | 3     | VXLAN      |
   +-------+------------+
   | 4     | Ethernet   |
   +-------+------------+
   | 5     | IPv6       |
   +-------+------------+
   | 6     | TCP        |
   +-------+------------+
   | 7     | END        |
   +-------+------------+

|

.. _table_rte_flow_tcpv4_as_l4_meta:

.. table:: TCPv4 as L4 with meta items

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | VOID     |
   +-------+----------+
   | 1     | Ethernet |
   +-------+----------+
   | 2     | VOID     |
   +-------+----------+
   | 3     | IPv4     |
   +-------+----------+
   | 4     | TCP      |
   +-------+----------+
   | 5     | VOID     |
   +-------+----------+
   | 6     | VOID     |
   +-------+----------+
   | 7     | END      |
   +-------+----------+

The above example shows how meta items do not affect packet data matching
items, as long as those remain stacked properly. The resulting matching
pattern is identical to "TCPv4 as L4".

.. _table_rte_flow_udpv6_anywhere:

.. table:: UDPv6 anywhere

   +-------+------+
   | Index | Item |
   +=======+======+
   | 0     | IPv6 |
   +-------+------+
   | 1     | UDP  |
   +-------+------+
   | 2     | END  |
   +-------+------+

If supported by the PMD, omitting one or several protocol layers at the
bottom of the stack as in the above example (missing an Ethernet
specification) enables looking up anywhere in packets.

It is unspecified whether the payload of supported encapsulations
(e.g. VXLAN payload) is matched by such a pattern, which may apply to inner,
outer or both packets.

.. _table_rte_flow_invalid_l3:

.. table:: Invalid, missing L3

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | Ethernet |
   +-------+----------+
   | 1     | UDP      |
   +-------+----------+
   | 2     | END      |
   +-------+----------+

The above pattern is invalid due to a missing L3 specification between L2
(Ethernet) and L4 (UDP). Doing so is only allowed at the bottom and at the
top of the stack.

Meta item types
~~~~~~~~~~~~~~~

They match meta-data or affect pattern processing instead of matching packet
data directly, most of them do not need a specification structure. This
particularity allows them to be specified anywhere in the stack without
causing any side effect.

Item: ``END``
^^^^^^^^^^^^^

End marker for item lists. Prevents further processing of items, thereby
ending the pattern.

- Its numeric value is 0 for convenience.
- PMD support is mandatory.
- ``spec``, ``last`` and ``mask`` are ignored.

.. _table_rte_flow_item_end:

.. table:: END

   +----------+---------+
   | Field    | Value   |
   +==========+=========+
   | ``spec`` | ignored |
   +----------+---------+
   | ``last`` | ignored |
   +----------+---------+
   | ``mask`` | ignored |
   +----------+---------+

Item: ``VOID``
^^^^^^^^^^^^^^

Used as a placeholder for convenience. It is ignored and simply discarded by
PMDs.

- PMD support is mandatory.
- ``spec``, ``last`` and ``mask`` are ignored.

.. _table_rte_flow_item_void:

.. table:: VOID

   +----------+---------+
   | Field    | Value   |
   +==========+=========+
   | ``spec`` | ignored |
   +----------+---------+
   | ``last`` | ignored |
   +----------+---------+
   | ``mask`` | ignored |
   +----------+---------+

One usage example for this type is generating rules that share a common
prefix quickly without reallocating memory, only by updating item types:

.. _table_rte_flow_item_void_example:

.. table:: TCP, UDP or ICMP as L4

   +-------+--------------------+
   | Index | Item               |
   +=======+====================+
   | 0     | Ethernet           |
   +-------+--------------------+
   | 1     | IPv4               |
   +-------+------+------+------+
   | 2     | UDP  | VOID | VOID |
   +-------+------+------+------+
   | 3     | VOID | TCP  | VOID |
   +-------+------+------+------+
   | 4     | VOID | VOID | ICMP |
   +-------+------+------+------+
   | 5     | END                |
   +-------+--------------------+

Item: ``INVERT``
^^^^^^^^^^^^^^^^

Inverted matching, i.e. process packets that do not match the pattern.

- ``spec``, ``last`` and ``mask`` are ignored.

.. _table_rte_flow_item_invert:

.. table:: INVERT

   +----------+---------+
   | Field    | Value   |
   +==========+=========+
   | ``spec`` | ignored |
   +----------+---------+
   | ``last`` | ignored |
   +----------+---------+
   | ``mask`` | ignored |
   +----------+---------+

Usage example, matching non-TCPv4 packets only:

.. _table_rte_flow_item_invert_example:

.. table:: Anything but TCPv4

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | INVERT   |
   +-------+----------+
   | 1     | Ethernet |
   +-------+----------+
   | 2     | IPv4     |
   +-------+----------+
   | 3     | TCP      |
   +-------+----------+
   | 4     | END      |
   +-------+----------+

Item: ``PF``
^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) the physical
function of the current device.

If supported, should work even if the physical function is not managed by
the application and thus not associated with a DPDK port ID.

- Can be combined with any number of `Item: VF`_ to match both PF and VF
  traffic.
- ``spec``, ``last`` and ``mask`` must not be set.

.. _table_rte_flow_item_pf:

.. table:: PF

   +----------+-------+
   | Field    | Value |
   +==========+=======+
   | ``spec`` | unset |
   +----------+-------+
   | ``last`` | unset |
   +----------+-------+
   | ``mask`` | unset |
   +----------+-------+

Item: ``VF``
^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) a given
virtual function of the current device.

If supported, should work even if the virtual function is not managed by the
application and thus not associated with a DPDK port ID.

Note this pattern item does not match VF representors traffic which, as
separate entities, should be addressed through their own DPDK port IDs.

- Can be specified multiple times to match traffic addressed to several VF
  IDs.
- Can be combined with a PF item to match both PF and VF traffic.
- Default ``mask`` matches any VF ID.

.. _table_rte_flow_item_vf:

.. table:: VF

   +----------+----------+---------------------------+
   | Field    | Subfield | Value                     |
   +==========+==========+===========================+
   | ``spec`` | ``id``   | destination VF ID         |
   +----------+----------+---------------------------+
   | ``last`` | ``id``   | upper range value         |
   +----------+----------+---------------------------+
   | ``mask`` | ``id``   | zeroed to match any VF ID |
   +----------+----------+---------------------------+

Item: ``PHY_PORT``
^^^^^^^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) a physical
port of the underlying device.

The first PHY_PORT item overrides the physical port normally associated with
the specified DPDK input port (port_id). This item can be provided several
times to match additional physical ports.

Note that physical ports are not necessarily tied to DPDK input ports
(port_id) when those are not under DPDK control. Possible values are
specific to each device, they are not necessarily indexed from zero and may
not be contiguous.

As a device property, the list of allowed values as well as the value
associated with a port_id should be retrieved by other means.

- Default ``mask`` matches any port index.

.. _table_rte_flow_item_phy_port:

.. table:: PHY_PORT

   +----------+-----------+--------------------------------+
   | Field    | Subfield  | Value                          |
   +==========+===========+================================+
   | ``spec`` | ``index`` | physical port index            |
   +----------+-----------+--------------------------------+
   | ``last`` | ``index`` | upper range value              |
   +----------+-----------+--------------------------------+
   | ``mask`` | ``index`` | zeroed to match any port index |
   +----------+-----------+--------------------------------+

Item: ``PORT_ID``
^^^^^^^^^^^^^^^^^

Matches traffic originating from (ingress) or going to (egress) a given DPDK
port ID.

Normally only supported if the port ID in question is known by the
underlying PMD and related to the device the flow rule is created against.

This must not be confused with `Item: PHY_PORT`_ which refers to the
physical port of a device, whereas `Item: PORT_ID`_ refers to a ``struct
rte_eth_dev`` object on the application side (also known as "port
representor" depending on the kind of underlying device).

- Default ``mask`` matches the specified DPDK port ID.

.. _table_rte_flow_item_port_id:

.. table:: PORT_ID

   +----------+----------+-----------------------------+
   | Field    | Subfield | Value                       |
   +==========+==========+=============================+
   | ``spec`` | ``id``   | DPDK port ID                |
   +----------+----------+-----------------------------+
   | ``last`` | ``id``   | upper range value           |
   +----------+----------+-----------------------------+
   | ``mask`` | ``id``   | zeroed to match any port ID |
   +----------+----------+-----------------------------+

Item: ``MARK``
^^^^^^^^^^^^^^

Matches an arbitrary integer value which was set using the ``MARK`` action in
a previously matched rule.

This item can only specified once as a match criteria as the ``MARK`` action can
only be specified once in a flow action.

Note the value of MARK field is arbitrary and application defined.

Depending on the underlying implementation the MARK item may be supported on
the physical device, with virtual groups in the PMD or not at all.

- Default ``mask`` matches any integer value.

.. _table_rte_flow_item_mark:

.. table:: MARK

   +----------+----------+---------------------------+
   | Field    | Subfield | Value                     |
   +==========+==========+===========================+
   | ``spec`` | ``id``   | integer value             |
   +----------+--------------------------------------+
   | ``last`` | ``id``   | upper range value         |
   +----------+----------+---------------------------+
   | ``mask`` | ``id``   | zeroed to match any value |
   +----------+----------+---------------------------+

Data matching item types
~~~~~~~~~~~~~~~~~~~~~~~~

Most of these are basically protocol header definitions with associated
bit-masks. They must be specified (stacked) from lowest to highest protocol
layer to form a matching pattern.

The following list is not exhaustive, new protocols will be added in the
future.

Item: ``ANY``
^^^^^^^^^^^^^

Matches any protocol in place of the current layer, a single ANY may also
stand for several protocol layers.

This is usually specified as the first pattern item when looking for a
protocol anywhere in a packet.

- Default ``mask`` stands for any number of layers.

.. _table_rte_flow_item_any:

.. table:: ANY

   +----------+----------+--------------------------------------+
   | Field    | Subfield | Value                                |
   +==========+==========+======================================+
   | ``spec`` | ``num``  | number of layers covered             |
   +----------+----------+--------------------------------------+
   | ``last`` | ``num``  | upper range value                    |
   +----------+----------+--------------------------------------+
   | ``mask`` | ``num``  | zeroed to cover any number of layers |
   +----------+----------+--------------------------------------+

Example for VXLAN TCP payload matching regardless of outer L3 (IPv4 or IPv6)
and L4 (UDP) both matched by the first ANY specification, and inner L3 (IPv4
or IPv6) matched by the second ANY specification:

.. _table_rte_flow_item_any_example:

.. table:: TCP in VXLAN with wildcards

   +-------+------+----------+----------+-------+
   | Index | Item | Field    | Subfield | Value |
   +=======+======+==========+==========+=======+
   | 0     | Ethernet                           |
   +-------+------+----------+----------+-------+
   | 1     | ANY  | ``spec`` | ``num``  | 2     |
   +-------+------+----------+----------+-------+
   | 2     | VXLAN                              |
   +-------+------------------------------------+
   | 3     | Ethernet                           |
   +-------+------+----------+----------+-------+
   | 4     | ANY  | ``spec`` | ``num``  | 1     |
   +-------+------+----------+----------+-------+
   | 5     | TCP                                |
   +-------+------------------------------------+
   | 6     | END                                |
   +-------+------------------------------------+

Item: ``RAW``
^^^^^^^^^^^^^

Matches a byte string of a given length at a given offset.

Offset is either absolute (using the start of the packet) or relative to the
end of the previous matched item in the stack, in which case negative values
are allowed.

If search is enabled, offset is used as the starting point. The search area
can be delimited by setting limit to a nonzero value, which is the maximum
number of bytes after offset where the pattern may start.

Matching a zero-length pattern is allowed, doing so resets the relative
offset for subsequent items.

- This type does not support ranges (``last`` field).
- Default ``mask`` matches all fields exactly.

.. _table_rte_flow_item_raw:

.. table:: RAW

   +----------+--------------+-------------------------------------------------+
   | Field    | Subfield     | Value                                           |
   +==========+==============+=================================================+
   | ``spec`` | ``relative`` | look for pattern after the previous item        |
   |          +--------------+-------------------------------------------------+
   |          | ``search``   | search pattern from offset (see also ``limit``) |
   |          +--------------+-------------------------------------------------+
   |          | ``reserved`` | reserved, must be set to zero                   |
   |          +--------------+-------------------------------------------------+
   |          | ``offset``   | absolute or relative offset for ``pattern``     |
   |          +--------------+-------------------------------------------------+
   |          | ``limit``    | search area limit for start of ``pattern``      |
   |          +--------------+-------------------------------------------------+
   |          | ``length``   | ``pattern`` length                              |
   |          +--------------+-------------------------------------------------+
   |          | ``pattern``  | byte string to look for                         |
   +----------+--------------+-------------------------------------------------+
   | ``last`` | if specified, either all 0 or with the same values as ``spec`` |
   +----------+----------------------------------------------------------------+
   | ``mask`` | bit-mask applied to ``spec`` values with usual behavior        |
   +----------+----------------------------------------------------------------+

Example pattern looking for several strings at various offsets of a UDP
payload, using combined RAW items:

.. _table_rte_flow_item_raw_example:

.. table:: UDP payload matching

   +-------+------+----------+--------------+-------+
   | Index | Item | Field    | Subfield     | Value |
   +=======+======+==========+==============+=======+
   | 0     | Ethernet                               |
   +-------+----------------------------------------+
   | 1     | IPv4                                   |
   +-------+----------------------------------------+
   | 2     | UDP                                    |
   +-------+------+----------+--------------+-------+
   | 3     | RAW  | ``spec`` | ``relative`` | 1     |
   |       |      |          +--------------+-------+
   |       |      |          | ``search``   | 1     |
   |       |      |          +--------------+-------+
   |       |      |          | ``offset``   | 10    |
   |       |      |          +--------------+-------+
   |       |      |          | ``limit``    | 0     |
   |       |      |          +--------------+-------+
   |       |      |          | ``length``   | 3     |
   |       |      |          +--------------+-------+
   |       |      |          | ``pattern``  | "foo" |
   +-------+------+----------+--------------+-------+
   | 4     | RAW  | ``spec`` | ``relative`` | 1     |
   |       |      |          +--------------+-------+
   |       |      |          | ``search``   | 0     |
   |       |      |          +--------------+-------+
   |       |      |          | ``offset``   | 20    |
   |       |      |          +--------------+-------+
   |       |      |          | ``limit``    | 0     |
   |       |      |          +--------------+-------+
   |       |      |          | ``length``   | 3     |
   |       |      |          +--------------+-------+
   |       |      |          | ``pattern``  | "bar" |
   +-------+------+----------+--------------+-------+
   | 5     | RAW  | ``spec`` | ``relative`` | 1     |
   |       |      |          +--------------+-------+
   |       |      |          | ``search``   | 0     |
   |       |      |          +--------------+-------+
   |       |      |          | ``offset``   | -29   |
   |       |      |          +--------------+-------+
   |       |      |          | ``limit``    | 0     |
   |       |      |          +--------------+-------+
   |       |      |          | ``length``   | 3     |
   |       |      |          +--------------+-------+
   |       |      |          | ``pattern``  | "baz" |
   +-------+------+----------+--------------+-------+
   | 6     | END                                    |
   +-------+----------------------------------------+

This translates to:

- Locate "foo" at least 10 bytes deep inside UDP payload.
- Locate "bar" after "foo" plus 20 bytes.
- Locate "baz" after "bar" minus 29 bytes.

Such a packet may be represented as follows (not to scale)::

 0                     >= 10 B           == 20 B
 |                  |<--------->|     |<--------->|
 |                  |           |     |           |
 |-----|------|-----|-----|-----|-----|-----------|-----|------|
 | ETH | IPv4 | UDP | ... | baz | foo | ......... | bar | .... |
 |-----|------|-----|-----|-----|-----|-----------|-----|------|
                          |                             |
                          |<--------------------------->|
                                      == 29 B

Note that matching subsequent pattern items would resume after "baz", not
"bar" since matching is always performed after the previous item of the
stack.

Item: ``ETH``
^^^^^^^^^^^^^

Matches an Ethernet header.

The ``type`` field either stands for "EtherType" or "TPID" when followed by
so-called layer 2.5 pattern items such as ``RTE_FLOW_ITEM_TYPE_VLAN``. In
the latter case, ``type`` refers to that of the outer header, with the inner
EtherType/TPID provided by the subsequent pattern item. This is the same
order as on the wire.

- ``dst``: destination MAC.
- ``src``: source MAC.
- ``type``: EtherType or TPID.
- Default ``mask`` matches destination and source addresses only.

Item: ``VLAN``
^^^^^^^^^^^^^^

Matches an 802.1Q/ad VLAN tag.

The corresponding standard outer EtherType (TPID) values are
``ETHER_TYPE_VLAN`` or ``ETHER_TYPE_QINQ``. It can be overridden by the
preceding pattern item.

- ``tci``: tag control information.
- ``inner_type``: inner EtherType or TPID.
- Default ``mask`` matches the VID part of TCI only (lower 12 bits).

Item: ``IPV4``
^^^^^^^^^^^^^^

Matches an IPv4 header.

Note: IPv4 options are handled by dedicated pattern items.

- ``hdr``: IPv4 header definition (``rte_ip.h``).
- Default ``mask`` matches source and destination addresses only.

Item: ``IPV6``
^^^^^^^^^^^^^^

Matches an IPv6 header.

Note: IPv6 options are handled by dedicated pattern items, see `Item:
IPV6_EXT`_.

- ``hdr``: IPv6 header definition (``rte_ip.h``).
- Default ``mask`` matches source and destination addresses only.

Item: ``ICMP``
^^^^^^^^^^^^^^

Matches an ICMP header.

- ``hdr``: ICMP header definition (``rte_icmp.h``).
- Default ``mask`` matches ICMP type and code only.

Item: ``UDP``
^^^^^^^^^^^^^

Matches a UDP header.

- ``hdr``: UDP header definition (``rte_udp.h``).
- Default ``mask`` matches source and destination ports only.

Item: ``TCP``
^^^^^^^^^^^^^

Matches a TCP header.

- ``hdr``: TCP header definition (``rte_tcp.h``).
- Default ``mask`` matches source and destination ports only.

Item: ``SCTP``
^^^^^^^^^^^^^^

Matches a SCTP header.

- ``hdr``: SCTP header definition (``rte_sctp.h``).
- Default ``mask`` matches source and destination ports only.

Item: ``VXLAN``
^^^^^^^^^^^^^^^

Matches a VXLAN header (RFC 7348).

- ``flags``: normally 0x08 (I flag).
- ``rsvd0``: reserved, normally 0x000000.
- ``vni``: VXLAN network identifier.
- ``rsvd1``: reserved, normally 0x00.
- Default ``mask`` matches VNI only.

Item: ``E_TAG``
^^^^^^^^^^^^^^^

Matches an IEEE 802.1BR E-Tag header.

The corresponding standard outer EtherType (TPID) value is
``ETHER_TYPE_ETAG``. It can be overridden by the preceding pattern item.

- ``epcp_edei_in_ecid_b``: E-Tag control information (E-TCI), E-PCP (3b),
  E-DEI (1b), ingress E-CID base (12b).
- ``rsvd_grp_ecid_b``: reserved (2b), GRP (2b), E-CID base (12b).
- ``in_ecid_e``: ingress E-CID ext.
- ``ecid_e``: E-CID ext.
- ``inner_type``: inner EtherType or TPID.
- Default ``mask`` simultaneously matches GRP and E-CID base.

Item: ``NVGRE``
^^^^^^^^^^^^^^^

Matches a NVGRE header (RFC 7637).

- ``c_k_s_rsvd0_ver``: checksum (1b), undefined (1b), key bit (1b),
  sequence number (1b), reserved 0 (9b), version (3b). This field must have
  value 0x2000 according to RFC 7637.
- ``protocol``: protocol type (0x6558).
- ``tni``: virtual subnet ID.
- ``flow_id``: flow ID.
- Default ``mask`` matches TNI only.

Item: ``MPLS``
^^^^^^^^^^^^^^

Matches a MPLS header.

- ``label_tc_s_ttl``: label, TC, Bottom of Stack and TTL.
- Default ``mask`` matches label only.

Item: ``GRE``
^^^^^^^^^^^^^

Matches a GRE header.

- ``c_rsvd0_ver``: checksum, reserved 0 and version.
- ``protocol``: protocol type.
- Default ``mask`` matches protocol only.

Item: ``FUZZY``
^^^^^^^^^^^^^^^

Fuzzy pattern match, expect faster than default.

This is for device that support fuzzy match option. Usually a fuzzy match is
fast but the cost is accuracy. i.e. Signature Match only match pattern's hash
value, but it is possible two different patterns have the same hash value.

Matching accuracy level can be configured by threshold. Driver can divide the
range of threshold and map to different accuracy levels that device support.

Threshold 0 means perfect match (no fuzziness), while threshold 0xffffffff
means fuzziest match.

.. _table_rte_flow_item_fuzzy:

.. table:: FUZZY

   +----------+---------------+--------------------------------------------------+
   | Field    |   Subfield    | Value                                            |
   +==========+===============+==================================================+
   | ``spec`` | ``threshold`` | 0 as perfect match, 0xffffffff as fuzziest match |
   +----------+---------------+--------------------------------------------------+
   | ``last`` | ``threshold`` | upper range value                                |
   +----------+---------------+--------------------------------------------------+
   | ``mask`` | ``threshold`` | bit-mask apply to "spec" and "last"              |
   +----------+---------------+--------------------------------------------------+

Usage example, fuzzy match a TCPv4 packets:

.. _table_rte_flow_item_fuzzy_example:

.. table:: Fuzzy matching

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | FUZZY    |
   +-------+----------+
   | 1     | Ethernet |
   +-------+----------+
   | 2     | IPv4     |
   +-------+----------+
   | 3     | TCP      |
   +-------+----------+
   | 4     | END      |
   +-------+----------+

Item: ``GTP``, ``GTPC``, ``GTPU``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches a GTPv1 header.

Note: GTP, GTPC and GTPU use the same structure. GTPC and GTPU item
are defined for a user-friendly API when creating GTP-C and GTP-U
flow rules.

- ``v_pt_rsv_flags``: version (3b), protocol type (1b), reserved (1b),
  extension header flag (1b), sequence number flag (1b), N-PDU number
  flag (1b).
- ``msg_type``: message type.
- ``msg_len``: message length.
- ``teid``: tunnel endpoint identifier.
- Default ``mask`` matches teid only.

Item: ``ESP``
^^^^^^^^^^^^^

Matches an ESP header.

- ``hdr``: ESP header definition (``rte_esp.h``).
- Default ``mask`` matches SPI only.

Item: ``GENEVE``
^^^^^^^^^^^^^^^^

Matches a GENEVE header.

- ``ver_opt_len_o_c_rsvd0``: version (2b), length of the options fields (6b),
  OAM packet (1b), critical options present (1b), reserved 0 (6b).
- ``protocol``: protocol type.
- ``vni``: virtual network identifier.
- ``rsvd1``: reserved, normally 0x00.
- Default ``mask`` matches VNI only.

Item: ``VXLAN-GPE``
^^^^^^^^^^^^^^^^^^^

Matches a VXLAN-GPE header (draft-ietf-nvo3-vxlan-gpe-05).

- ``flags``: normally 0x0C (I and P flags).
- ``rsvd0``: reserved, normally 0x0000.
- ``protocol``: protocol type.
- ``vni``: VXLAN network identifier.
- ``rsvd1``: reserved, normally 0x00.
- Default ``mask`` matches VNI only.

Item: ``ARP_ETH_IPV4``
^^^^^^^^^^^^^^^^^^^^^^

Matches an ARP header for Ethernet/IPv4.

- ``hdr``: hardware type, normally 1.
- ``pro``: protocol type, normally 0x0800.
- ``hln``: hardware address length, normally 6.
- ``pln``: protocol address length, normally 4.
- ``op``: opcode (1 for request, 2 for reply).
- ``sha``: sender hardware address.
- ``spa``: sender IPv4 address.
- ``tha``: target hardware address.
- ``tpa``: target IPv4 address.
- Default ``mask`` matches SHA, SPA, THA and TPA.

Item: ``IPV6_EXT``
^^^^^^^^^^^^^^^^^^

Matches the presence of any IPv6 extension header.

- ``next_hdr``: next header.
- Default ``mask`` matches ``next_hdr``.

Normally preceded by any of:

- `Item: IPV6`_
- `Item: IPV6_EXT`_

Item: ``ICMP6``
^^^^^^^^^^^^^^^

Matches any ICMPv6 header.

- ``type``: ICMPv6 type.
- ``code``: ICMPv6 code.
- ``checksum``: ICMPv6 checksum.
- Default ``mask`` matches ``type`` and ``code``.

Item: ``ICMP6_ND_NS``
^^^^^^^^^^^^^^^^^^^^^

Matches an ICMPv6 neighbor discovery solicitation.

- ``type``: ICMPv6 type, normally 135.
- ``code``: ICMPv6 code, normally 0.
- ``checksum``: ICMPv6 checksum.
- ``reserved``: reserved, normally 0.
- ``target_addr``: target address.
- Default ``mask`` matches target address only.

Item: ``ICMP6_ND_NA``
^^^^^^^^^^^^^^^^^^^^^

Matches an ICMPv6 neighbor discovery advertisement.

- ``type``: ICMPv6 type, normally 136.
- ``code``: ICMPv6 code, normally 0.
- ``checksum``: ICMPv6 checksum.
- ``rso_reserved``: route flag (1b), solicited flag (1b), override flag
  (1b), reserved (29b).
- ``target_addr``: target address.
- Default ``mask`` matches target address only.

Item: ``ICMP6_ND_OPT``
^^^^^^^^^^^^^^^^^^^^^^

Matches the presence of any ICMPv6 neighbor discovery option.

- ``type``: ND option type.
- ``length``: ND option length.
- Default ``mask`` matches type only.

Normally preceded by any of:

- `Item: ICMP6_ND_NA`_
- `Item: ICMP6_ND_NS`_
- `Item: ICMP6_ND_OPT`_

Item: ``ICMP6_ND_OPT_SLA_ETH``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches an ICMPv6 neighbor discovery source Ethernet link-layer address
option.

- ``type``: ND option type, normally 1.
- ``length``: ND option length, normally 1.
- ``sla``: source Ethernet LLA.
- Default ``mask`` matches source link-layer address only.

Normally preceded by any of:

- `Item: ICMP6_ND_NA`_
- `Item: ICMP6_ND_OPT`_

Item: ``ICMP6_ND_OPT_TLA_ETH``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches an ICMPv6 neighbor discovery target Ethernet link-layer address
option.

- ``type``: ND option type, normally 2.
- ``length``: ND option length, normally 1.
- ``tla``: target Ethernet LLA.
- Default ``mask`` matches target link-layer address only.

Normally preceded by any of:

- `Item: ICMP6_ND_NS`_
- `Item: ICMP6_ND_OPT`_

Item: ``META``
^^^^^^^^^^^^^^

Matches an application specific 32 bit metadata item.

- Default ``mask`` matches the specified metadata value.

.. _table_rte_flow_item_meta:

.. table:: META

   +----------+----------+---------------------------------------+
   | Field    | Subfield | Value                                 |
   +==========+==========+=======================================+
   | ``spec`` | ``data`` | 32 bit metadata value                 |
   +----------+--------------------------------------------------+
   | ``last`` | ``data`` | upper range value                     |
   +----------+----------+---------------------------------------+
   | ``mask`` | ``data`` | bit-mask applies to "spec" and "last" |
   +----------+----------+---------------------------------------+

Actions
~~~~~~~

Each possible action is represented by a type. Some have associated
configuration structures. Several actions combined in a list can be assigned
to a flow rule and are performed in order.

They fall in three categories:

- Actions that modify the fate of matching traffic, for instance by dropping
  or assigning it a specific destination.

- Actions that modify matching traffic contents or its properties. This
  includes adding/removing encapsulation, encryption, compression and marks.

- Actions related to the flow rule itself, such as updating counters or
  making it non-terminating.

Flow rules being terminating by default, not specifying any action of the
fate kind results in undefined behavior. This applies to both ingress and
egress.

PASSTHRU, when supported, makes a flow rule non-terminating.

Like matching patterns, action lists are terminated by END items.

Example of action that redirects packets to queue index 10:

.. _table_rte_flow_action_example:

.. table:: Queue action

   +-----------+-------+
   | Field     | Value |
   +===========+=======+
   | ``index`` | 10    |
   +-----------+-------+

Actions are performed in list order:

.. _table_rte_flow_count_then_drop:

.. table:: Count then drop

   +-------+--------+
   | Index | Action |
   +=======+========+
   | 0     | COUNT  |
   +-------+--------+
   | 1     | DROP   |
   +-------+--------+
   | 2     | END    |
   +-------+--------+

|

.. _table_rte_flow_mark_count_redirect:

.. table:: Mark, count then redirect

   +-------+--------+------------+-------+
   | Index | Action | Field      | Value |
   +=======+========+============+=======+
   | 0     | MARK   | ``mark``   | 0x2a  |
   +-------+--------+------------+-------+
   | 1     | COUNT  | ``shared`` | 0     |
   |       |        +------------+-------+
   |       |        | ``id``     | 0     |
   +-------+--------+------------+-------+
   | 2     | QUEUE  | ``queue``  | 10    |
   +-------+--------+------------+-------+
   | 3     | END                         |
   +-------+-----------------------------+

|

.. _table_rte_flow_redirect_queue_5:

.. table:: Redirect to queue 5

   +-------+--------+-----------+-------+
   | Index | Action | Field     | Value |
   +=======+========+===========+=======+
   | 0     | DROP                       |
   +-------+--------+-----------+-------+
   | 1     | QUEUE  | ``queue`` | 5     |
   +-------+--------+-----------+-------+
   | 2     | END                        |
   +-------+----------------------------+

In the above example, while DROP and QUEUE must be performed in order, both
have to happen before reaching END. Only QUEUE has a visible effect.

Note that such a list may be thought as ambiguous and rejected on that
basis.

.. _table_rte_flow_redirect_queue_5_3:

.. table:: Redirect to queues 5 and 3

   +-------+--------+-----------+-------+
   | Index | Action | Field     | Value |
   +=======+========+===========+=======+
   | 0     | QUEUE  | ``queue`` | 5     |
   +-------+--------+-----------+-------+
   | 1     | VOID                       |
   +-------+--------+-----------+-------+
   | 2     | QUEUE  | ``queue`` | 3     |
   +-------+--------+-----------+-------+
   | 3     | END                        |
   +-------+----------------------------+

As previously described, all actions must be taken into account. This
effectively duplicates traffic to both queues. The above example also shows
that VOID is ignored.

Action types
~~~~~~~~~~~~

Common action types are described in this section. Like pattern item types,
this list is not exhaustive as new actions will be added in the future.

Action: ``END``
^^^^^^^^^^^^^^^

End marker for action lists. Prevents further processing of actions, thereby
ending the list.

- Its numeric value is 0 for convenience.
- PMD support is mandatory.
- No configurable properties.

.. _table_rte_flow_action_end:

.. table:: END

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``VOID``
^^^^^^^^^^^^^^^^

Used as a placeholder for convenience. It is ignored and simply discarded by
PMDs.

- PMD support is mandatory.
- No configurable properties.

.. _table_rte_flow_action_void:

.. table:: VOID

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``PASSTHRU``
^^^^^^^^^^^^^^^^^^^^

Leaves traffic up for additional processing by subsequent flow rules; makes
a flow rule non-terminating.

- No configurable properties.

.. _table_rte_flow_action_passthru:

.. table:: PASSTHRU

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Example to copy a packet to a queue and continue processing by subsequent
flow rules:

.. _table_rte_flow_action_passthru_example:

.. table:: Copy to queue 8

   +-------+--------+-----------+-------+
   | Index | Action | Field     | Value |
   +=======+========+===========+=======+
   | 0     | PASSTHRU                   |
   +-------+--------+-----------+-------+
   | 1     | QUEUE  | ``queue`` | 8     |
   +-------+--------+-----------+-------+
   | 2     | END                        |
   +-------+----------------------------+

Action: ``JUMP``
^^^^^^^^^^^^^^^^

Redirects packets to a group on the current device.

In a hierarchy of groups, which can be used to represent physical or logical
flow group/tables on the device, this action redirects the matched flow to
the specified group on that device.

If a matched flow is redirected to a table which doesn't contain a matching
rule for that flow then the behavior is undefined and the resulting behavior
is up to the specific device. Best practice when using groups would be define
a default flow rule for each group which a defines the default actions in that
group so a consistent behavior is defined.

Defining an action for matched flow in a group to jump to a group which is
higher in the group hierarchy may not be supported by physical devices,
depending on how groups are mapped to the physical devices. In the
definitions of jump actions, applications should be aware that it may be
possible to define flow rules which trigger an undefined behavior causing
flows to loop between groups.

.. _table_rte_flow_action_jump:

.. table:: JUMP

   +-----------+------------------------------+
   | Field     | Value                        |
   +===========+==============================+
   | ``group`` | Group to redirect packets to |
   +-----------+------------------------------+

Action: ``MARK``
^^^^^^^^^^^^^^^^

Attaches an integer value to packets and sets ``PKT_RX_FDIR`` and
``PKT_RX_FDIR_ID`` mbuf flags.

This value is arbitrary and application-defined. Maximum allowed value
depends on the underlying implementation. It is returned in the
``hash.fdir.hi`` mbuf field.

.. _table_rte_flow_action_mark:

.. table:: MARK

   +--------+--------------------------------------+
   | Field  | Value                                |
   +========+======================================+
   | ``id`` | integer value to return with packets |
   +--------+--------------------------------------+

Action: ``FLAG``
^^^^^^^^^^^^^^^^

Flags packets. Similar to `Action: MARK`_ without a specific value; only
sets the ``PKT_RX_FDIR`` mbuf flag.

- No configurable properties.

.. _table_rte_flow_action_flag:

.. table:: FLAG

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``QUEUE``
^^^^^^^^^^^^^^^^^

Assigns packets to a given queue index.

.. _table_rte_flow_action_queue:

.. table:: QUEUE

   +-----------+--------------------+
   | Field     | Value              |
   +===========+====================+
   | ``index`` | queue index to use |
   +-----------+--------------------+

Action: ``DROP``
^^^^^^^^^^^^^^^^

Drop packets.

- No configurable properties.

.. _table_rte_flow_action_drop:

.. table:: DROP

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``COUNT``
^^^^^^^^^^^^^^^^^

Adds a counter action to a matched flow.

If more than one count action is specified in a single flow rule, then each
action must specify a unique id.

Counters can be retrieved and reset through ``rte_flow_query()``, see
``struct rte_flow_query_count``.

The shared flag indicates whether the counter is unique to the flow rule the
action is specified with, or whether it is a shared counter.

For a count action with the shared flag set, then then a global device
namespace is assumed for the counter id, so that any matched flow rules using
a count action with the same counter id on the same port will contribute to
that counter.

For ports within the same switch domain then the counter id namespace extends
to all ports within that switch domain.

.. _table_rte_flow_action_count:

.. table:: COUNT

   +------------+---------------------+
   | Field      | Value               |
   +============+=====================+
   | ``shared`` | shared counter flag |
   +------------+---------------------+
   | ``id``     | counter id          |
   +------------+---------------------+

Query structure to retrieve and reset flow rule counters:

.. _table_rte_flow_query_count:

.. table:: COUNT query

   +---------------+-----+-----------------------------------+
   | Field         | I/O | Value                             |
   +===============+=====+===================================+
   | ``reset``     | in  | reset counter after query         |
   +---------------+-----+-----------------------------------+
   | ``hits_set``  | out | ``hits`` field is set             |
   +---------------+-----+-----------------------------------+
   | ``bytes_set`` | out | ``bytes`` field is set            |
   +---------------+-----+-----------------------------------+
   | ``hits``      | out | number of hits for this rule      |
   +---------------+-----+-----------------------------------+
   | ``bytes``     | out | number of bytes through this rule |
   +---------------+-----+-----------------------------------+

Action: ``RSS``
^^^^^^^^^^^^^^^

Similar to QUEUE, except RSS is additionally performed on packets to spread
them among several queues according to the provided parameters.

Unlike global RSS settings used by other DPDK APIs, unsetting the ``types``
field does not disable RSS in a flow rule. Doing so instead requests safe
unspecified "best-effort" settings from the underlying PMD, which depending
on the flow rule, may result in anything ranging from empty (single queue)
to all-inclusive RSS.

Note: RSS hash result is stored in the ``hash.rss`` mbuf field which
overlaps ``hash.fdir.lo``. Since `Action: MARK`_ sets the ``hash.fdir.hi``
field only, both can be requested simultaneously.

Also, regarding packet encapsulation ``level``:

- ``0`` requests the default behavior. Depending on the packet type, it can
  mean outermost, innermost, anything in between or even no RSS.

  It basically stands for the innermost encapsulation level RSS can be
  performed on according to PMD and device capabilities.

- ``1`` requests RSS to be performed on the outermost packet encapsulation
  level.

- ``2`` and subsequent values request RSS to be performed on the specified
   inner packet encapsulation level, from outermost to innermost (lower to
   higher values).

Values other than ``0`` are not necessarily supported.

Requesting a specific RSS level on unrecognized traffic results in undefined
behavior. For predictable results, it is recommended to make the flow rule
pattern match packet headers up to the requested encapsulation level so that
only matching traffic goes through.

.. _table_rte_flow_action_rss:

.. table:: RSS

   +---------------+---------------------------------------------+
   | Field         | Value                                       |
   +===============+=============================================+
   | ``func``      | RSS hash function to apply                  |
   +---------------+---------------------------------------------+
   | ``level``     | encapsulation level for ``types``           |
   +---------------+---------------------------------------------+
   | ``types``     | specific RSS hash types (see ``ETH_RSS_*``) |
   +---------------+---------------------------------------------+
   | ``key_len``   | hash key length in bytes                    |
   +---------------+---------------------------------------------+
   | ``queue_num`` | number of entries in ``queue``              |
   +---------------+---------------------------------------------+
   | ``key``       | hash key                                    |
   +---------------+---------------------------------------------+
   | ``queue``     | queue indices to use                        |
   +---------------+---------------------------------------------+

Action: ``PF``
^^^^^^^^^^^^^^

Directs matching traffic to the physical function (PF) of the current
device.

See `Item: PF`_.

- No configurable properties.

.. _table_rte_flow_action_pf:

.. table:: PF

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``VF``
^^^^^^^^^^^^^^

Directs matching traffic to a given virtual function of the current device.

Packets matched by a VF pattern item can be redirected to their original VF
ID instead of the specified one. This parameter may not be available and is
not guaranteed to work properly if the VF part is matched by a prior flow
rule or if packets are not addressed to a VF in the first place.

See `Item: VF`_.

.. _table_rte_flow_action_vf:

.. table:: VF

   +--------------+--------------------------------+
   | Field        | Value                          |
   +==============+================================+
   | ``original`` | use original VF ID if possible |
   +--------------+--------------------------------+
   | ``id``       | VF ID                          |
   +--------------+--------------------------------+

Action: ``PHY_PORT``
^^^^^^^^^^^^^^^^^^^^

Directs matching traffic to a given physical port index of the underlying
device.

See `Item: PHY_PORT`_.

.. _table_rte_flow_action_phy_port:

.. table:: PHY_PORT

   +--------------+-------------------------------------+
   | Field        | Value                               |
   +==============+=====================================+
   | ``original`` | use original port index if possible |
   +--------------+-------------------------------------+
   | ``index``    | physical port index                 |
   +--------------+-------------------------------------+

Action: ``PORT_ID``
^^^^^^^^^^^^^^^^^^^
Directs matching traffic to a given DPDK port ID.

See `Item: PORT_ID`_.

.. _table_rte_flow_action_port_id:

.. table:: PORT_ID

   +--------------+---------------------------------------+
   | Field        | Value                                 |
   +==============+=======================================+
   | ``original`` | use original DPDK port ID if possible |
   +--------------+---------------------------------------+
   | ``id``       | DPDK port ID                          |
   +--------------+---------------------------------------+

Action: ``METER``
^^^^^^^^^^^^^^^^^

Applies a stage of metering and policing.

The metering and policing (MTR) object has to be first created using the
rte_mtr_create() API function. The ID of the MTR object is specified as
action parameter. More than one flow can use the same MTR object through
the meter action. The MTR object can be further updated or queried using
the rte_mtr* API.

.. _table_rte_flow_action_meter:

.. table:: METER

   +--------------+---------------+
   | Field        | Value         |
   +==============+===============+
   | ``mtr_id``   | MTR object ID |
   +--------------+---------------+

Action: ``SECURITY``
^^^^^^^^^^^^^^^^^^^^

Perform the security action on flows matched by the pattern items
according to the configuration of the security session.

This action modifies the payload of matched flows. For INLINE_CRYPTO, the
security protocol headers and IV are fully provided by the application as
specified in the flow pattern. The payload of matching packets is
encrypted on egress, and decrypted and authenticated on ingress.
For INLINE_PROTOCOL, the security protocol is fully offloaded to HW,
providing full encapsulation and decapsulation of packets in security
protocols. The flow pattern specifies both the outer security header fields
and the inner packet fields. The security session specified in the action
must match the pattern parameters.

The security session specified in the action must be created on the same
port as the flow action that is being specified.

The ingress/egress flow attribute should match that specified in the
security session if the security session supports the definition of the
direction.

Multiple flows can be configured to use the same security session.

.. _table_rte_flow_action_security:

.. table:: SECURITY

   +----------------------+--------------------------------------+
   | Field                | Value                                |
   +======================+======================================+
   | ``security_session`` | security session to apply            |
   +----------------------+--------------------------------------+

The following is an example of configuring IPsec inline using the
INLINE_CRYPTO security session:

The encryption algorithm, keys and salt are part of the opaque
``rte_security_session``. The SA is identified according to the IP and ESP
fields in the pattern items.

.. _table_rte_flow_item_esp_inline_example:

.. table:: IPsec inline crypto flow pattern items.

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | Ethernet |
   +-------+----------+
   | 1     | IPv4     |
   +-------+----------+
   | 2     | ESP      |
   +-------+----------+
   | 3     | END      |
   +-------+----------+

.. _table_rte_flow_action_esp_inline_example:

.. table:: IPsec inline flow actions.

   +-------+----------+
   | Index | Action   |
   +=======+==========+
   | 0     | SECURITY |
   +-------+----------+
   | 1     | END      |
   +-------+----------+

Action: ``OF_SET_MPLS_TTL``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_SET_MPLS_TTL`` ("MPLS TTL") as defined by the `OpenFlow
Switch Specification`_.

.. _table_rte_flow_action_of_set_mpls_ttl:

.. table:: OF_SET_MPLS_TTL

   +--------------+----------+
   | Field        | Value    |
   +==============+==========+
   | ``mpls_ttl`` | MPLS TTL |
   +--------------+----------+

Action: ``OF_DEC_MPLS_TTL``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_DEC_MPLS_TTL`` ("decrement MPLS TTL") as defined by the
`OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_dec_mpls_ttl:

.. table:: OF_DEC_MPLS_TTL

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``OF_SET_NW_TTL``
^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_SET_NW_TTL`` ("IP TTL") as defined by the `OpenFlow
Switch Specification`_.

.. _table_rte_flow_action_of_set_nw_ttl:

.. table:: OF_SET_NW_TTL

   +------------+--------+
   | Field      | Value  |
   +============+========+
   | ``nw_ttl`` | IP TTL |
   +------------+--------+

Action: ``OF_DEC_NW_TTL``
^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_DEC_NW_TTL`` ("decrement IP TTL") as defined by the
`OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_dec_nw_ttl:

.. table:: OF_DEC_NW_TTL

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``OF_COPY_TTL_OUT``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_COPY_TTL_OUT`` ("copy TTL "outwards" -- from
next-to-outermost to outermost") as defined by the `OpenFlow Switch
Specification`_.

.. _table_rte_flow_action_of_copy_ttl_out:

.. table:: OF_COPY_TTL_OUT

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``OF_COPY_TTL_IN``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_COPY_TTL_IN`` ("copy TTL "inwards" -- from outermost to
next-to-outermost") as defined by the `OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_copy_ttl_in:

.. table:: OF_COPY_TTL_IN

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``OF_POP_VLAN``
^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_POP_VLAN`` ("pop the outer VLAN tag") as defined
by the `OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_pop_vlan:

.. table:: OF_POP_VLAN

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``OF_PUSH_VLAN``
^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_PUSH_VLAN`` ("push a new VLAN tag") as defined by the
`OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_push_vlan:

.. table:: OF_PUSH_VLAN

   +---------------+-----------+
   | Field         | Value     |
   +===============+===========+
   | ``ethertype`` | EtherType |
   +---------------+-----------+

Action: ``OF_SET_VLAN_VID``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_SET_VLAN_VID`` ("set the 802.1q VLAN id") as defined by
the `OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_set_vlan_vid:

.. table:: OF_SET_VLAN_VID

   +--------------+---------+
   | Field        | Value   |
   +==============+=========+
   | ``vlan_vid`` | VLAN id |
   +--------------+---------+

Action: ``OF_SET_VLAN_PCP``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_SET_LAN_PCP`` ("set the 802.1q priority") as defined by
the `OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_set_vlan_pcp:

.. table:: OF_SET_VLAN_PCP

   +--------------+---------------+
   | Field        | Value         |
   +==============+===============+
   | ``vlan_pcp`` | VLAN priority |
   +--------------+---------------+

Action: ``OF_POP_MPLS``
^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_POP_MPLS`` ("pop the outer MPLS tag") as defined by the
`OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_pop_mpls:

.. table:: OF_POP_MPLS

   +---------------+-----------+
   | Field         | Value     |
   +===============+===========+
   | ``ethertype`` | EtherType |
   +---------------+-----------+

Action: ``OF_PUSH_MPLS``
^^^^^^^^^^^^^^^^^^^^^^^^

Implements ``OFPAT_PUSH_MPLS`` ("push a new MPLS tag") as defined by the
`OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_push_mpls:

.. table:: OF_PUSH_MPLS

   +---------------+-----------+
   | Field         | Value     |
   +===============+===========+
   | ``ethertype`` | EtherType |
   +---------------+-----------+

Action: ``VXLAN_ENCAP``
^^^^^^^^^^^^^^^^^^^^^^^

Performs a VXLAN encapsulation action by encapsulating the matched flow in the
VXLAN tunnel as defined in the``rte_flow_action_vxlan_encap`` flow items
definition.

This action modifies the payload of matched flows. The flow definition specified
in the ``rte_flow_action_tunnel_encap`` action structure must define a valid
VLXAN network overlay which conforms with RFC 7348 (Virtual eXtensible Local
Area Network (VXLAN): A Framework for Overlaying Virtualized Layer 2 Networks
over Layer 3 Networks). The pattern must be terminated with the
RTE_FLOW_ITEM_TYPE_END item type.

.. _table_rte_flow_action_vxlan_encap:

.. table:: VXLAN_ENCAP

   +----------------+-------------------------------------+
   | Field          | Value                               |
   +================+=====================================+
   | ``definition`` | Tunnel end-point overlay definition |
   +----------------+-------------------------------------+

.. _table_rte_flow_action_vxlan_encap_example:

.. table:: IPv4 VxLAN flow pattern example.

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | Ethernet |
   +-------+----------+
   | 1     | IPv4     |
   +-------+----------+
   | 2     | UDP      |
   +-------+----------+
   | 3     | VXLAN    |
   +-------+----------+
   | 4     | END      |
   +-------+----------+

Action: ``VXLAN_DECAP``
^^^^^^^^^^^^^^^^^^^^^^^

Performs a decapsulation action by stripping all headers of the VXLAN tunnel
network overlay from the matched flow.

The flow items pattern defined for the flow rule with which a ``VXLAN_DECAP``
action is specified, must define a valid VXLAN tunnel as per RFC7348. If the
flow pattern does not specify a valid VXLAN tunnel then a
RTE_FLOW_ERROR_TYPE_ACTION error should be returned.

This action modifies the payload of matched flows.

Action: ``NVGRE_ENCAP``
^^^^^^^^^^^^^^^^^^^^^^^

Performs a NVGRE encapsulation action by encapsulating the matched flow in the
NVGRE tunnel as defined in the``rte_flow_action_tunnel_encap`` flow item
definition.

This action modifies the payload of matched flows. The flow definition specified
in the ``rte_flow_action_tunnel_encap`` action structure must defined a valid
NVGRE network overlay which conforms with RFC 7637 (NVGRE: Network
Virtualization Using Generic Routing Encapsulation). The pattern must be
terminated with the RTE_FLOW_ITEM_TYPE_END item type.

.. _table_rte_flow_action_nvgre_encap:

.. table:: NVGRE_ENCAP

   +----------------+-------------------------------------+
   | Field          | Value                               |
   +================+=====================================+
   | ``definition`` | NVGRE end-point overlay definition  |
   +----------------+-------------------------------------+

.. _table_rte_flow_action_nvgre_encap_example:

.. table:: IPv4 NVGRE flow pattern example.

   +-------+----------+
   | Index | Item     |
   +=======+==========+
   | 0     | Ethernet |
   +-------+----------+
   | 1     | IPv4     |
   +-------+----------+
   | 2     | NVGRE    |
   +-------+----------+
   | 3     | END      |
   +-------+----------+

Action: ``NVGRE_DECAP``
^^^^^^^^^^^^^^^^^^^^^^^

Performs a decapsulation action by stripping all headers of the NVGRE tunnel
network overlay from the matched flow.

The flow items pattern defined for the flow rule with which a ``NVGRE_DECAP``
action is specified, must define a valid NVGRE tunnel as per RFC7637. If the
flow pattern does not specify a valid NVGRE tunnel then a
RTE_FLOW_ERROR_TYPE_ACTION error should be returned.

This action modifies the payload of matched flows.

Action: ``RAW_ENCAP``
^^^^^^^^^^^^^^^^^^^^^

Adds outer header whose template is provided in its data buffer,
as defined in the ``rte_flow_action_raw_encap`` definition.

This action modifies the payload of matched flows. The data supplied must
be a valid header, either holding layer 2 data in case of adding layer 2 after
decap layer 3 tunnel (for example MPLSoGRE) or complete tunnel definition
starting from layer 2 and moving to the tunnel item itself. When applied to
the original packet the resulting packet must be a valid packet.

.. _table_rte_flow_action_raw_encap:

.. table:: RAW_ENCAP

   +----------------+----------------------------------------+
   | Field          | Value                                  |
   +================+========================================+
   | ``data``       | Encapsulation data                     |
   +----------------+----------------------------------------+
   | ``preserve``   | Bit-mask of data to preserve on output |
   +----------------+----------------------------------------+
   | ``size``       | Size of data and preserve              |
   +----------------+----------------------------------------+

Action: ``RAW_DECAP``
^^^^^^^^^^^^^^^^^^^^^^^

Remove outer header whose template is provided in its data buffer,
as defined in the ``rte_flow_action_raw_decap``

This action modifies the payload of matched flows. The data supplied must
be a valid header, either holding layer 2 data in case of removing layer 2
before encapsulation of layer 3 tunnel (for example MPLSoGRE) or complete
tunnel definition starting from layer 2 and moving to the tunnel item itself.
When applied to the original packet the resulting packet must be a
valid packet.

.. _table_rte_flow_action_raw_decap:

.. table:: RAW_DECAP

   +----------------+----------------------------------------+
   | Field          | Value                                  |
   +================+========================================+
   | ``data``       | Decapsulation data                     |
   +----------------+----------------------------------------+
   | ``size``       | Size of data                           |
   +----------------+----------------------------------------+

Action: ``SET_IPV4_SRC``
^^^^^^^^^^^^^^^^^^^^^^^^

Set a new IPv4 source address in the outermost IPv4 header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_IPV4 flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_ipv4_src:

.. table:: SET_IPV4_SRC

   +-----------------------------------------+
   | Field         | Value                   |
   +===============+=========================+
   | ``ipv4_addr`` | new IPv4 source address |
   +---------------+-------------------------+

Action: ``SET_IPV4_DST``
^^^^^^^^^^^^^^^^^^^^^^^^

Set a new IPv4 destination address in the outermost IPv4 header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_IPV4 flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_ipv4_dst:

.. table:: SET_IPV4_DST

   +---------------+------------------------------+
   | Field         | Value                        |
   +===============+==============================+
   | ``ipv4_addr`` | new IPv4 destination address |
   +---------------+------------------------------+

Action: ``SET_IPV6_SRC``
^^^^^^^^^^^^^^^^^^^^^^^^

Set a new IPv6 source address in the outermost IPv6 header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_IPV6 flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_ipv6_src:

.. table:: SET_IPV6_SRC

   +---------------+-------------------------+
   | Field         | Value                   |
   +===============+=========================+
   | ``ipv6_addr`` | new IPv6 source address |
   +---------------+-------------------------+

Action: ``SET_IPV6_DST``
^^^^^^^^^^^^^^^^^^^^^^^^

Set a new IPv6 destination address in the outermost IPv6 header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_IPV6 flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_ipv6_dst:

.. table:: SET_IPV6_DST

   +---------------+------------------------------+
   | Field         | Value                        |
   +===============+==============================+
   | ``ipv6_addr`` | new IPv6 destination address |
   +---------------+------------------------------+

Action: ``SET_TP_SRC``
^^^^^^^^^^^^^^^^^^^^^^^^^

Set a new source port number in the outermost TCP/UDP header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_TCP or RTE_FLOW_ITEM_TYPE_UDP
flow pattern item. Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_tp_src:

.. table:: SET_TP_SRC

   +----------+-------------------------+
   | Field    | Value                   |
   +==========+=========================+
   | ``port`` | new TCP/UDP source port |
   +---------------+--------------------+

Action: ``SET_TP_DST``
^^^^^^^^^^^^^^^^^^^^^^^^^

Set a new destination port number in the outermost TCP/UDP header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_TCP or RTE_FLOW_ITEM_TYPE_UDP
flow pattern item. Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_tp_dst:

.. table:: SET_TP_DST

   +----------+------------------------------+
   | Field    | Value                        |
   +==========+==============================+
   | ``port`` | new TCP/UDP destination port |
   +---------------+-------------------------+

Action: ``MAC_SWAP``
^^^^^^^^^^^^^^^^^^^^^^^^^

Swap the source and destination MAC addresses in the outermost Ethernet
header.

It must be used with a valid RTE_FLOW_ITEM_TYPE_ETH flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_mac_swap:

.. table:: MAC_SWAP

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``DEC_TTL``
^^^^^^^^^^^^^^^^^^^

Decrease TTL value.

If there is no valid RTE_FLOW_ITEM_TYPE_IPV4 or RTE_FLOW_ITEM_TYPE_IPV6
in pattern, Some PMDs will reject rule because behavior will be undefined.

.. _table_rte_flow_action_dec_ttl:

.. table:: DEC_TTL

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``SET_TTL``
^^^^^^^^^^^^^^^^^^^

Assigns a new TTL value.

If there is no valid RTE_FLOW_ITEM_TYPE_IPV4 or RTE_FLOW_ITEM_TYPE_IPV6
in pattern, Some PMDs will reject rule because behavior will be undefined.

.. _table_rte_flow_action_set_ttl:

.. table:: SET_TTL

   +---------------+--------------------+
   | Field         | Value              |
   +===============+====================+
   | ``ttl_value`` | new TTL value      |
   +---------------+--------------------+

Action: ``SET_MAC_SRC``
^^^^^^^^^^^^^^^^^^^^^^^

Set source MAC address.

It must be used with a valid RTE_FLOW_ITEM_TYPE_ETH flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_mac_src:

.. table:: SET_MAC_SRC

   +--------------+---------------+
   | Field        | Value         |
   +==============+===============+
   | ``mac_addr`` | MAC address   |
   +--------------+---------------+

Action: ``SET_MAC_DST``
^^^^^^^^^^^^^^^^^^^^^^^

Set destination MAC address.

It must be used with a valid RTE_FLOW_ITEM_TYPE_ETH flow pattern item.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_mac_dst:

.. table:: SET_MAC_DST

   +--------------+---------------+
   | Field        | Value         |
   +==============+===============+
   | ``mac_addr`` | MAC address   |
   +--------------+---------------+

Negative types
~~~~~~~~~~~~~~

All specified pattern items (``enum rte_flow_item_type``) and actions
(``enum rte_flow_action_type``) use positive identifiers.

The negative space is reserved for dynamic types generated by PMDs during
run-time. PMDs may encounter them as a result but must not accept negative
identifiers they are not aware of.

A method to generate them remains to be defined.

Planned types
~~~~~~~~~~~~~

Pattern item types will be added as new protocols are implemented.

Variable headers support through dedicated pattern items, for example in
order to match specific IPv4 options and IPv6 extension headers would be
stacked after IPv4/IPv6 items.

Other action types are planned but are not defined yet. These include the
ability to alter packet data in several ways, such as performing
encapsulation/decapsulation of tunnel headers.

Rules management
----------------

A rather simple API with few functions is provided to fully manage flow
rules.

Each created flow rule is associated with an opaque, PMD-specific handle
pointer. The application is responsible for keeping it until the rule is
destroyed.

Flows rules are represented by ``struct rte_flow`` objects.

Validation
~~~~~~~~~~

Given that expressing a definite set of device capabilities is not
practical, a dedicated function is provided to check if a flow rule is
supported and can be created.

.. code-block:: c

   int
   rte_flow_validate(uint16_t port_id,
                     const struct rte_flow_attr *attr,
                     const struct rte_flow_item pattern[],
                     const struct rte_flow_action actions[],
                     struct rte_flow_error *error);

The flow rule is validated for correctness and whether it could be accepted
by the device given sufficient resources. The rule is checked against the
current device mode and queue configuration. The flow rule may also
optionally be validated against existing flow rules and device resources.
This function has no effect on the target device.

The returned value is guaranteed to remain valid only as long as no
successful calls to ``rte_flow_create()`` or ``rte_flow_destroy()`` are made
in the meantime and no device parameter affecting flow rules in any way are
modified, due to possible collisions or resource limitations (although in
such cases ``EINVAL`` should not be returned).

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``attr``: flow rule attributes.
- ``pattern``: pattern specification (list terminated by the END pattern
  item).
- ``actions``: associated actions (list terminated by the END action).
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

- 0 if flow rule is valid and can be created. A negative errno value
  otherwise (``rte_errno`` is also set), the following errors are defined.
- ``-ENOSYS``: underlying device does not support this functionality.
- ``-EINVAL``: unknown or invalid rule specification.
- ``-ENOTSUP``: valid but unsupported rule specification (e.g. partial
  bit-masks are unsupported).
- ``EEXIST``: collision with an existing rule. Only returned if device
  supports flow rule collision checking and there was a flow rule
  collision. Not receiving this return code is no guarantee that creating
  the rule will not fail due to a collision.
- ``ENOMEM``: not enough memory to execute the function, or if the device
  supports resource validation, resource limitation on the device.
- ``-EBUSY``: action cannot be performed due to busy device resources, may
  succeed if the affected queues or even the entire port are in a stopped
  state (see ``rte_eth_dev_rx_queue_stop()`` and ``rte_eth_dev_stop()``).

Creation
~~~~~~~~

Creating a flow rule is similar to validating one, except the rule is
actually created and a handle returned.

.. code-block:: c

   struct rte_flow *
   rte_flow_create(uint16_t port_id,
                   const struct rte_flow_attr *attr,
                   const struct rte_flow_item pattern[],
                   const struct rte_flow_action *actions[],
                   struct rte_flow_error *error);

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``attr``: flow rule attributes.
- ``pattern``: pattern specification (list terminated by the END pattern
  item).
- ``actions``: associated actions (list terminated by the END action).
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

A valid handle in case of success, NULL otherwise and ``rte_errno`` is set
to the positive version of one of the error codes defined for
``rte_flow_validate()``.

Destruction
~~~~~~~~~~~

Flow rules destruction is not automatic, and a queue or a port should not be
released if any are still attached to them. Applications must take care of
performing this step before releasing resources.

.. code-block:: c

   int
   rte_flow_destroy(uint16_t port_id,
                    struct rte_flow *flow,
                    struct rte_flow_error *error);


Failure to destroy a flow rule handle may occur when other flow rules depend
on it, and destroying it would result in an inconsistent state.

This function is only guaranteed to succeed if handles are destroyed in
reverse order of their creation.

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``flow``: flow rule handle to destroy.
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

- 0 on success, a negative errno value otherwise and ``rte_errno`` is set.

Flush
~~~~~

Convenience function to destroy all flow rule handles associated with a
port. They are released as with successive calls to ``rte_flow_destroy()``.

.. code-block:: c

   int
   rte_flow_flush(uint16_t port_id,
                  struct rte_flow_error *error);

In the unlikely event of failure, handles are still considered destroyed and
no longer valid but the port must be assumed to be in an inconsistent state.

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

- 0 on success, a negative errno value otherwise and ``rte_errno`` is set.

Query
~~~~~

Query an existing flow rule.

This function allows retrieving flow-specific data such as counters. Data
is gathered by special actions which must be present in the flow rule
definition.

.. code-block:: c

   int
   rte_flow_query(uint16_t port_id,
                  struct rte_flow *flow,
                  const struct rte_flow_action *action,
                  void *data,
                  struct rte_flow_error *error);

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``flow``: flow rule handle to query.
- ``action``: action to query, this must match prototype from flow rule.
- ``data``: pointer to storage for the associated query data type.
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

- 0 on success, a negative errno value otherwise and ``rte_errno`` is set.

.. _flow_isolated_mode:

Flow isolated mode
------------------

The general expectation for ingress traffic is that flow rules process it
first; the remaining unmatched or pass-through traffic usually ends up in a
queue (with or without RSS, locally or in some sub-device instance)
depending on the global configuration settings of a port.

While fine from a compatibility standpoint, this approach makes drivers more
complex as they have to check for possible side effects outside of this API
when creating or destroying flow rules. It results in a more limited set of
available rule types due to the way device resources are assigned (e.g. no
support for the RSS action even on capable hardware).

Given that nonspecific traffic can be handled by flow rules as well,
isolated mode is a means for applications to tell a driver that ingress on
the underlying port must be injected from the defined flow rules only; that
no default traffic is expected outside those rules.

This has the following benefits:

- Applications get finer-grained control over the kind of traffic they want
  to receive (no traffic by default).

- More importantly they control at what point nonspecific traffic is handled
  relative to other flow rules, by adjusting priority levels.

- Drivers can assign more hardware resources to flow rules and expand the
  set of supported rule types.

Because toggling isolated mode may cause profound changes to the ingress
processing path of a driver, it may not be possible to leave it once
entered. Likewise, existing flow rules or global configuration settings may
prevent a driver from entering isolated mode.

Applications relying on this mode are therefore encouraged to toggle it as
soon as possible after device initialization, ideally before the first call
to ``rte_eth_dev_configure()`` to avoid possible failures due to conflicting
settings.

Once effective, the following functionality has no effect on the underlying
port and may return errors such as ``ENOTSUP`` ("not supported"):

- Toggling promiscuous mode.
- Toggling allmulticast mode.
- Configuring MAC addresses.
- Configuring multicast addresses.
- Configuring VLAN filters.
- Configuring Rx filters through the legacy API (e.g. FDIR).
- Configuring global RSS settings.

.. code-block:: c

   int
   rte_flow_isolate(uint16_t port_id, int set, struct rte_flow_error *error);

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``set``: nonzero to enter isolated mode, attempt to leave it otherwise.
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

- 0 on success, a negative errno value otherwise and ``rte_errno`` is set.

Verbose error reporting
-----------------------

The defined *errno* values may not be accurate enough for users or
application developers who want to investigate issues related to flow rules
management. A dedicated error object is defined for this purpose:

.. code-block:: c

   enum rte_flow_error_type {
       RTE_FLOW_ERROR_TYPE_NONE, /**< No error. */
       RTE_FLOW_ERROR_TYPE_UNSPECIFIED, /**< Cause unspecified. */
       RTE_FLOW_ERROR_TYPE_HANDLE, /**< Flow rule (handle). */
       RTE_FLOW_ERROR_TYPE_ATTR_GROUP, /**< Group field. */
       RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, /**< Priority field. */
       RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, /**< Ingress field. */
       RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, /**< Egress field. */
       RTE_FLOW_ERROR_TYPE_ATTR, /**< Attributes structure. */
       RTE_FLOW_ERROR_TYPE_ITEM_NUM, /**< Pattern length. */
       RTE_FLOW_ERROR_TYPE_ITEM, /**< Specific pattern item. */
       RTE_FLOW_ERROR_TYPE_ACTION_NUM, /**< Number of actions. */
       RTE_FLOW_ERROR_TYPE_ACTION, /**< Specific action. */
   };

   struct rte_flow_error {
       enum rte_flow_error_type type; /**< Cause field and error types. */
       const void *cause; /**< Object responsible for the error. */
       const char *message; /**< Human-readable error message. */
   };

Error type ``RTE_FLOW_ERROR_TYPE_NONE`` stands for no error, in which case
remaining fields can be ignored. Other error types describe the type of the
object pointed by ``cause``.

If non-NULL, ``cause`` points to the object responsible for the error. For a
flow rule, this may be a pattern item or an individual action.

If non-NULL, ``message`` provides a human-readable error message.

This object is normally allocated by applications and set by PMDs in case of
error, the message points to a constant string which does not need to be
freed by the application, however its pointer can be considered valid only
as long as its associated DPDK port remains configured. Closing the
underlying device or unloading the PMD invalidates it.

Helpers
-------

Error initializer
~~~~~~~~~~~~~~~~~

.. code-block:: c

   static inline int
   rte_flow_error_set(struct rte_flow_error *error,
                      int code,
                      enum rte_flow_error_type type,
                      const void *cause,
                      const char *message);

This function initializes ``error`` (if non-NULL) with the provided
parameters and sets ``rte_errno`` to ``code``. A negative error ``code`` is
then returned.

Object conversion
~~~~~~~~~~~~~~~~~

.. code-block:: c

   int
   rte_flow_conv(enum rte_flow_conv_op op,
                 void *dst,
                 size_t size,
                 const void *src,
                 struct rte_flow_error *error);

Convert ``src`` to ``dst`` according to operation ``op``. Possible
operations include:

- Attributes, pattern item or action duplication.
- Duplication of an entire pattern or list of actions.
- Duplication of a complete flow rule description.
- Pattern item or action name retrieval.

Caveats
-------

- DPDK does not keep track of flow rules definitions or flow rule objects
  automatically. Applications may keep track of the former and must keep
  track of the latter. PMDs may also do it for internal needs, however this
  must not be relied on by applications.

- Flow rules are not maintained between successive port initializations. An
  application exiting without releasing them and restarting must re-create
  them from scratch.

- API operations are synchronous and blocking (``EAGAIN`` cannot be
  returned).

- There is no provision for re-entrancy/multi-thread safety, although nothing
  should prevent different devices from being configured at the same
  time. PMDs may protect their control path functions accordingly.

- Stopping the data path (TX/RX) should not be necessary when managing flow
  rules. If this cannot be achieved naturally or with workarounds (such as
  temporarily replacing the burst function pointers), an appropriate error
  code must be returned (``EBUSY``).

- PMDs, not applications, are responsible for maintaining flow rules
  configuration when stopping and restarting a port or performing other
  actions which may affect them. They can only be destroyed explicitly by
  applications.

For devices exposing multiple ports sharing global settings affected by flow
rules:

- All ports under DPDK control must behave consistently, PMDs are
  responsible for making sure that existing flow rules on a port are not
  affected by other ports.

- Ports not under DPDK control (unaffected or handled by other applications)
  are user's responsibility. They may affect existing flow rules and cause
  undefined behavior. PMDs aware of this may prevent flow rules creation
  altogether in such cases.

PMD interface
-------------

The PMD interface is defined in ``rte_flow_driver.h``. It is not subject to
API/ABI versioning constraints as it is not exposed to applications and may
evolve independently.

It is currently implemented on top of the legacy filtering framework through
filter type *RTE_ETH_FILTER_GENERIC* that accepts the single operation
*RTE_ETH_FILTER_GET* to return PMD-specific *rte_flow* callbacks wrapped
inside ``struct rte_flow_ops``.

This overhead is temporarily necessary in order to keep compatibility with
the legacy filtering framework, which should eventually disappear.

- PMD callbacks implement exactly the interface described in `Rules
  management`_, except for the port ID argument which has already been
  converted to a pointer to the underlying ``struct rte_eth_dev``.

- Public API functions do not process flow rules definitions at all before
  calling PMD functions (no basic error checking, no validation
  whatsoever). They only make sure these callbacks are non-NULL or return
  the ``ENOSYS`` (function not supported) error.

This interface additionally defines the following helper function:

- ``rte_flow_ops_get()``: get generic flow operations structure from a
  port.

More will be added over time.

Device compatibility
--------------------

No known implementation supports all the described features.

Unsupported features or combinations are not expected to be fully emulated
in software by PMDs for performance reasons. Partially supported features
may be completed in software as long as hardware performs most of the work
(such as queue redirection and packet recognition).

However PMDs are expected to do their best to satisfy application requests
by working around hardware limitations as long as doing so does not affect
the behavior of existing flow rules.

The following sections provide a few examples of such cases and describe how
PMDs should handle them, they are based on limitations built into the
previous APIs.

Global bit-masks
~~~~~~~~~~~~~~~~

Each flow rule comes with its own, per-layer bit-masks, while hardware may
support only a single, device-wide bit-mask for a given layer type, so that
two IPv4 rules cannot use different bit-masks.

The expected behavior in this case is that PMDs automatically configure
global bit-masks according to the needs of the first flow rule created.

Subsequent rules are allowed only if their bit-masks match those, the
``EEXIST`` error code should be returned otherwise.

Unsupported layer types
~~~~~~~~~~~~~~~~~~~~~~~

Many protocols can be simulated by crafting patterns with the `Item: RAW`_
type.

PMDs can rely on this capability to simulate support for protocols with
headers not directly recognized by hardware.

``ANY`` pattern item
~~~~~~~~~~~~~~~~~~~~

This pattern item stands for anything, which can be difficult to translate
to something hardware would understand, particularly if followed by more
specific types.

Consider the following pattern:

.. _table_rte_flow_unsupported_any:

.. table:: Pattern with ANY as L3

   +-------+-----------------------+
   | Index | Item                  |
   +=======+=======================+
   | 0     | ETHER                 |
   +-------+-----+---------+-------+
   | 1     | ANY | ``num`` | ``1`` |
   +-------+-----+---------+-------+
   | 2     | TCP                   |
   +-------+-----------------------+
   | 3     | END                   |
   +-------+-----------------------+

Knowing that TCP does not make sense with something other than IPv4 and IPv6
as L3, such a pattern may be translated to two flow rules instead:

.. _table_rte_flow_unsupported_any_ipv4:

.. table:: ANY replaced with IPV4

   +-------+--------------------+
   | Index | Item               |
   +=======+====================+
   | 0     | ETHER              |
   +-------+--------------------+
   | 1     | IPV4 (zeroed mask) |
   +-------+--------------------+
   | 2     | TCP                |
   +-------+--------------------+
   | 3     | END                |
   +-------+--------------------+

|

.. _table_rte_flow_unsupported_any_ipv6:

.. table:: ANY replaced with IPV6

   +-------+--------------------+
   | Index | Item               |
   +=======+====================+
   | 0     | ETHER              |
   +-------+--------------------+
   | 1     | IPV6 (zeroed mask) |
   +-------+--------------------+
   | 2     | TCP                |
   +-------+--------------------+
   | 3     | END                |
   +-------+--------------------+

Note that as soon as a ANY rule covers several layers, this approach may
yield a large number of hidden flow rules. It is thus suggested to only
support the most common scenarios (anything as L2 and/or L3).

Unsupported actions
~~~~~~~~~~~~~~~~~~~

- When combined with `Action: QUEUE`_, packet counting (`Action: COUNT`_)
  and tagging (`Action: MARK`_ or `Action: FLAG`_) may be implemented in
  software as long as the target queue is used by a single rule.

- When a single target queue is provided, `Action: RSS`_ can also be
  implemented through `Action: QUEUE`_.

Flow rules priority
~~~~~~~~~~~~~~~~~~~

While it would naturally make sense, flow rules cannot be assumed to be
processed by hardware in the same order as their creation for several
reasons:

- They may be managed internally as a tree or a hash table instead of a
  list.
- Removing a flow rule before adding another one can either put the new rule
  at the end of the list or reuse a freed entry.
- Duplication may occur when packets are matched by several rules.

For overlapping rules (particularly in order to use `Action: PASSTHRU`_)
predictable behavior is only guaranteed by using different priority levels.

Priority levels are not necessarily implemented in hardware, or may be
severely limited (e.g. a single priority bit).

For these reasons, priority levels may be implemented purely in software by
PMDs.

- For devices expecting flow rules to be added in the correct order, PMDs
  may destroy and re-create existing rules after adding a new one with
  a higher priority.

- A configurable number of dummy or empty rules can be created at
  initialization time to save high priority slots for later.

- In order to save priority levels, PMDs may evaluate whether rules are
  likely to collide and adjust their priority accordingly.

Future evolutions
-----------------

- A device profile selection function which could be used to force a
  permanent profile instead of relying on its automatic configuration based
  on existing flow rules.

- A method to optimize *rte_flow* rules with specific pattern items and
  action types generated on the fly by PMDs. DPDK should assign negative
  numbers to these in order to not collide with the existing types. See
  `Negative types`_.

- Adding specific egress pattern items and actions as described in
  `Attribute: Traffic direction`_.

- Optional software fallback when PMDs are unable to handle requested flow
  rules so applications do not have to implement their own.

.. _OpenFlow Switch Specification: https://www.opennetworking.org/software-defined-standards/specifications/
