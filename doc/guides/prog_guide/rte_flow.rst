..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 6WIND S.A.
    Copyright 2016 Mellanox Technologies, Ltd

Generic flow API (rte_flow)
===========================

Overview
--------

This API provides a generic means to configure hardware to match specific
traffic, alter its fate and query related counters according to any
number of user-defined rules.

It is named *rte_flow* after the prefix used for all its symbols, and is
defined in ``rte_flow.h``.

- Matching can be performed on packet data (protocol headers, payload) and
  properties (e.g. associated physical port, virtual device function ID).

- Possible operations include dropping traffic, diverting it to specific
  queues, to virtual/physical device functions or ports, performing tunnel
  offloads, adding marks and so on.

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
group using the JUMP action to define the next group to redirect to. Only flow
rules defined in the default group 0 are guaranteed to be matched against. This
makes group 0 the origin of any group hierarchy defined by an application.

Support for multiple actions per rule may be implemented internally on top
of non-default hardware priorities. As a result, both features may not be
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

.. warning::

   The following description of rule persistence is an experimental behavior
   that may change without a prior notice.

When the device is stopped, its rules do not process the traffic.
In particular, transfer rules created using some device
stop affecting the traffic even if they refer to different ports.

If ``RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP`` is not advertised,
rules cannot be created until the device is started for the first time
and cannot be kept when the device is stopped.
However, PMD also does not flush them automatically on stop,
so the application must call ``rte_flow_flush()`` or ``rte_flow_destroy()``
before stopping the device to ensure no rules remain.

If ``RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP`` is advertised, this means
the PMD can keep at least some rules across the device stop and start.
However, ``rte_eth_dev_configure()`` may fail if any rules remain,
so the application must flush them before attempting a reconfiguration.
Keeping may be unsupported for some types of rule items and actions,
as well as depending on the value of flow attributes transfer bit.
A combination of a single an item or action type
and a value of the transfer bit is called a rule feature.
For example: a COUNT action with the transfer bit set.
To test if rules with a particular feature are kept, the application must try
to create a valid rule using this feature when the device is not started
(either before the first start or after a stop).
If it fails with an error of type ``RTE_FLOW_ERROR_TYPE_STATE``,
all rules using this feature must be flushed by the application
before stopping the device.
If it succeeds, such rules will be kept when the device is stopped,
provided they do not use other features that are not supported.
Rules that are created when the device is stopped, including the rules
created for the test, will be kept after the device is started.

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
Group 0 is the default group and is the only group that
flows are guaranteed to be matched against.
All subsequent groups can only be reached by using a JUMP action
from a matched flow rule.

Although optional, applications are encouraged to group similar rules as
much as possible to fully take advantage of hardware capabilities
(e.g. optimized matching) and work around limitations (e.g. a single pattern
type possibly allowed in a given group), while being aware that the groups'
hierarchies must be programmed explicitly.

Note that support for more than a single group is not guaranteed.

Attribute: Priority
^^^^^^^^^^^^^^^^^^^

A priority level can be assigned to a flow rule, lower values
denote higher priority, with 0 as the maximum.

Priority levels are arbitrary and up to the application, they do
not need to be contiguous nor start from 0, however the maximum number
varies between devices and may be affected by existing flow rules.

A flow which matches multiple rules in the same group will always be matched by
the rule with the highest priority in that group.

If a packet is matched by several rules of a given group for a given
priority level, the outcome is undefined. It can take any path, may be
duplicated or even cause unrecoverable errors.

Note that support for more than a single priority level is not guaranteed.

Attribute: Traffic direction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Unless `Attribute: Transfer`_ is specified, flow rule patterns apply
to inbound and / or outbound traffic. With this respect, ``ingress``
and ``egress`` respectively stand for **inbound** and **outbound**
based on the standpoint of the application creating a flow rule.

Several pattern items and actions are valid and can be used in both
directions. At least one direction must be specified.

Specifying both directions at once for a given rule is not recommended but
may be valid in a few cases.

Attribute: Transfer
^^^^^^^^^^^^^^^^^^^

Instead of simply matching the properties of traffic as it would appear on a
given DPDK port ID, enabling this attribute transfers a flow rule to the
lowest possible level of any device endpoints found in the pattern.

When supported, this effectively enables an application to reroute traffic
not necessarily intended for it (e.g. coming from or addressed to different
physical ports, VFs or applications) at the device level.

In "transfer" flows, the use of `Attribute: Traffic direction`_ in not allowed.
One may use `Item: PORT_REPRESENTOR`_ and `Item: REPRESENTED_PORT`_ instead.

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

   +----------+----------+-----------------------+
   | Field    | Subfield | Value                 |
   +==========+==========+=======================+
   | ``spec`` | ``src``  | ``00:00:01:02:03:04`` |
   |          +----------+-----------------------+
   |          | ``dst``  | ``00:00:2a:66:00:01`` |
   |          +----------+-----------------------+
   |          | ``type`` | ``0x22aa``            |
   +----------+----------+-----------------------+
   | ``last`` | unspecified                      |
   +----------+----------+-----------------------+
   | ``mask`` | ``src``  | ``00:00:ff:ff:ff:00`` |
   |          +----------+-----------------------+
   |          | ``dst``  | ``00:00:00:00:00:ff`` |
   |          +----------+-----------------------+
   |          | ``type`` | ``0x0000``            |
   +----------+----------+-----------------------+

Non-masked bits stand for any value (shown as ``?`` below), Ethernet headers
with the following properties are thus matched:

- ``src``: ``??:??:01:02:03:??``
- ``dst``: ``??:??:??:??:??:01``
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

Item: ``PORT_ID``
^^^^^^^^^^^^^^^^^

This item is deprecated. Consider:
 - `Item: PORT_REPRESENTOR`_
 - `Item: REPRESENTED_PORT`_

Matches traffic originating from (ingress) or going to (egress) a given DPDK
port ID.

Normally only supported if the port ID in question is known by the
underlying PMD and related to the device the flow rule is created against.

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

Item: ``TAG``
^^^^^^^^^^^^^

Matches tag item set by other flows. Multiple tags are supported by specifying
``index``.

- Default ``mask`` matches the specified tag value and index.

.. _table_rte_flow_item_tag:

.. table:: TAG

   +----------+----------+----------------------------------------+
   | Field    | Subfield  | Value                                 |
   +==========+===========+=======================================+
   | ``spec`` | ``data``  | 32 bit flow tag value                 |
   |          +-----------+---------------------------------------+
   |          | ``index`` | index of flow tag                     |
   +----------+-----------+---------------------------------------+
   | ``last`` | ``data``  | upper range value                     |
   |          +-----------+---------------------------------------+
   |          | ``index`` | field is ignored                      |
   +----------+-----------+---------------------------------------+
   | ``mask`` | ``data``  | bit-mask applies to "spec" and "last" |
   |          +-----------+---------------------------------------+
   |          | ``index`` | field is ignored                      |
   +----------+-----------+---------------------------------------+

Item: ``META``
^^^^^^^^^^^^^^^^^

Matches 32 bit metadata item set.

On egress, metadata can be set either by mbuf metadata field with
RTE_MBUF_DYNFLAG_TX_METADATA flag or ``SET_META`` action. On ingress, ``SET_META``
action sets metadata for a packet and the metadata will be reported via
``metadata`` dynamic field of ``rte_mbuf`` with RTE_MBUF_DYNFLAG_RX_METADATA flag.

- Default ``mask`` matches the specified Rx metadata value.

.. _table_rte_flow_item_meta:

.. table:: META

   +----------+----------+---------------------------------------+
   | Field    | Subfield | Value                                 |
   +==========+==========+=======================================+
   | ``spec`` | ``data`` | 32 bit metadata value                 |
   +----------+----------+---------------------------------------+
   | ``last`` | ``data`` | upper range value                     |
   +----------+----------+---------------------------------------+
   | ``mask`` | ``data`` | bit-mask applies to "spec" and "last" |
   +----------+----------+---------------------------------------+

Data matching item types
~~~~~~~~~~~~~~~~~~~~~~~~

Most of these are basically protocol header definitions with associated
bit-masks. They must be specified (stacked) from lowest to highest protocol
layer to form a matching pattern.

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
If the ``type`` field contains a TPID value, then only tagged packets with the
specified TPID will match the pattern.
The field ``has_vlan`` can be used to match any type of tagged packets,
instead of using the ``type`` field.
If the ``type`` and ``has_vlan`` fields are not specified, then both tagged
and untagged packets will match the pattern.

- ``hdr``:  header definition (``rte_ether.h``).
- ``has_vlan``: packet header contains at least one VLAN.
- Default ``mask`` matches destination and source addresses only.

Item: ``VLAN``
^^^^^^^^^^^^^^

Matches an 802.1Q/ad VLAN tag.

The corresponding standard outer EtherType (TPID) values are
``RTE_ETHER_TYPE_VLAN`` or ``RTE_ETHER_TYPE_QINQ``. It can be overridden by the
preceding pattern item.
If a ``VLAN`` item is present in the pattern, then only tagged packets will
match the pattern.
The field ``has_more_vlan`` can be used to match any type of tagged packets,
instead of using the ``inner_type field``.
If the ``inner_type`` and ``has_more_vlan`` fields are not specified,
then any tagged packets will match the pattern.

- ``hdr``:  header definition (``rte_ether.h``).
- ``has_more_vlan``: packet header contains at least one more VLAN, after this VLAN.
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

Dedicated flags indicate if header contains specific extension headers.
To match on packets containing a specific extension header, an application
should match on the dedicated flag set to 1.
To match on packets not containing a specific extension header, an application
should match on the dedicated flag clear to 0.
In case application doesn't care about the existence of a specific extension
header, it should not specify the dedicated flag for matching.

- ``hdr``: IPv6 header definition (``rte_ip.h``).
- ``has_hop_ext``: header contains Hop-by-Hop Options extension header.
- ``has_route_ext``: header contains Routing extension header.
- ``has_frag_ext``: header contains Fragment extension header.
- ``has_auth_ext``: header contains Authentication extension header.
- ``has_esp_ext``: header contains Encapsulation Security Payload extension header.
- ``has_dest_ext``: header contains Destination Options extension header.
- ``has_mobil_ext``: header contains Mobility extension header.
- ``has_hip_ext``: header contains Host Identity Protocol extension header.
- ``has_shim6_ext``: header contains Shim6 Protocol extension header.
- Default ``mask`` matches ``hdr`` source and destination addresses only.

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

- ``hdr``:  header definition (``rte_vxlan.h``).
- Default ``mask`` matches VNI only.

Item: ``E_TAG``
^^^^^^^^^^^^^^^

Matches an IEEE 802.1BR E-Tag header.

The corresponding standard outer EtherType (TPID) value is
``RTE_ETHER_TYPE_ETAG``. It can be overridden by the preceding pattern item.

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

Item: ``GRE_KEY``
^^^^^^^^^^^^^^^^^
This action is deprecated. Consider `Item: GRE_OPTION`.

Matches a GRE key field.
This should be preceded by item ``GRE``.

- Value to be matched is a big-endian 32 bit integer.
- When this item present it implicitly match K bit in default mask as "1"

Item: ``GRE_OPTION``
^^^^^^^^^^^^^^^^^^^^

Matches a GRE optional fields (checksum/key/sequence).
This should be preceded by item ``GRE``.

- ``checksum``: checksum.
- ``key``: key.
- ``sequence``: sequence.
- The items in GRE_OPTION do not change bit flags(c_bit/k_bit/s_bit) in GRE
  item. The bit flags need be set with GRE item by application. When the items
  present, the corresponding bits in GRE spec and mask should be set "1" by
  application, it means to match specified value of the fields. When the items
  no present, but the corresponding bits in GRE spec and mask is "1", it means
  to match any value of the fields.

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

- ``hdr``:  header definition (``rte_gtp.h``).
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

- ``hdr``:  header definition (``rte_vxlan.h``).
- Default ``mask`` matches VNI only.

Item: ``ARP_ETH_IPV4``
^^^^^^^^^^^^^^^^^^^^^^

Matches an ARP header for Ethernet/IPv4.

- ``hdr``:  header definition (``rte_arp.h``).
- Default ``mask`` matches SHA, SPA, THA and TPA.

Item: ``IPV6_EXT``
^^^^^^^^^^^^^^^^^^

Matches the presence of any IPv6 extension header.

- ``next_hdr``: next header.
- Default ``mask`` matches ``next_hdr``.

Normally preceded by any of:

- `Item: IPV6`_
- `Item: IPV6_EXT`_

Item: ``IPV6_FRAG_EXT``
^^^^^^^^^^^^^^^^^^^^^^^

Matches the presence of IPv6 fragment extension header.

- ``hdr``: IPv6 fragment extension header definition (``rte_ip.h``).

Normally preceded by any of:

- `Item: IPV6`_
- `Item: IPV6_EXT`_

Item: ``IPV6_ROUTING_EXT``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches IPv6 routing extension header.

- ``next_hdr``: Next layer header type.
- ``type``: IPv6 routing extension header type.
- ``segments_left``: How many IPv6 destination addresses carries on.

Item: ``ICMP6``
^^^^^^^^^^^^^^^

Matches any ICMPv6 header.

- ``type``: ICMPv6 type.
- ``code``: ICMPv6 code.
- ``checksum``: ICMPv6 checksum.
- Default ``mask`` matches ``type`` and ``code``.

Item: ``ICMP6_ECHO_REQUEST``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches an ICMPv6 echo request.

- ``hdr``: ICMP6 echo header definition (``rte_icmp.h``).

Item: ``ICMP6_ECHO_REPLY``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches an ICMPv6 echo reply.

- ``hdr``: ICMP6 echo header definition (``rte_icmp.h``).

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

Item: ``GTP_PSC``
^^^^^^^^^^^^^^^^^

Matches a GTP PDU extension header with type 0x85.

- ``hdr``:  header definition (``rte_gtp.h``).
- Default ``mask`` matches QFI only.

Item: ``PPPOES``, ``PPPOED``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches a PPPoE header.

- ``version_type``: version (4b), type (4b).
- ``code``: message type.
- ``session_id``: session identifier.
- ``length``: payload length.

Item: ``PPPOE_PROTO_ID``
^^^^^^^^^^^^^^^^^^^^^^^^

Matches a PPPoE session protocol identifier.

- ``proto_id``: PPP protocol identifier.
- Default ``mask`` matches proto_id only.

Item: ``NSH``
^^^^^^^^^^^^^

Matches a network service header (RFC 8300).

- ``version``: normally 0x0 (2 bits).
- ``oam_pkt``: indicate oam packet (1 bit).
- ``reserved``: reserved bit (1 bit).
- ``ttl``: maximum SFF hopes (6 bits).
- ``length``: total length in 4 bytes words (6 bits).
- ``reserved1``: reserved1 bits (4 bits).
- ``mdtype``: indicates format of NSH header (4 bits).
- ``next_proto``: indicates protocol type of encap data (8 bits).
- ``spi``: service path identifier (3 bytes).
- ``sindex``: service index (1 byte).
- Default ``mask`` matches mdtype, next_proto, spi, sindex.


Item: ``IGMP``
^^^^^^^^^^^^^^

Matches a Internet Group Management Protocol (RFC 2236).

- ``type``: IGMP message type (Query/Report).
- ``max_resp_time``: max time allowed before sending report.
- ``checksum``: checksum, 1s complement of whole IGMP message.
- ``group_addr``: group address, for Query value will be 0.
- Default ``mask`` matches group_addr.


Item: ``AH``
^^^^^^^^^^^^

Matches a IP Authentication Header (RFC 4302).

- ``next_hdr``: next payload after AH.
- ``payload_len``: total length of AH in 4B words.
- ``reserved``: reserved bits.
- ``spi``: security parameters index.
- ``seq_num``: counter value increased by 1 on each packet sent.
- Default ``mask`` matches spi.

Item: ``HIGIG2``
^^^^^^^^^^^^^^^^^

Matches a HIGIG2 header field. It is layer 2.5 protocol and used in
Broadcom switches.

- Default ``mask`` matches classification and vlan.

Item: ``L2TPV3OIP``
^^^^^^^^^^^^^^^^^^^

Matches a L2TPv3 over IP header.

- ``session_id``: L2TPv3 over IP session identifier.
- Default ``mask`` matches session_id only.

Item: ``PFCP``
^^^^^^^^^^^^^^

Matches a PFCP Header.

- ``s_field``: S field.
- ``msg_type``: message type.
- ``msg_len``: message length.
- ``seid``: session endpoint identifier.
- Default ``mask`` matches s_field and seid.

Item: ``ECPRI``
^^^^^^^^^^^^^^^

Matches a eCPRI header.

- ``hdr``: eCPRI header definition (``rte_ecpri.h``).
- Default ``mask`` matches nothing, for all eCPRI messages.

Item: ``PACKET_INTEGRITY_CHECKS``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches packet integrity.
For some devices application needs to enable integration checks in HW
before using this item.

- ``level``: the encapsulation level that should be checked:
   - ``level == 0`` means the default PMD mode (can be inner most / outermost).
   - ``level == 1`` means outermost header.
   - ``level > 1``  means inner header. See also RSS level.
- ``packet_ok``: All HW packet integrity checks have passed based on the
  topmost network layer. For example, for ICMP packet the topmost network
  layer is L3 and for TCP or UDP packet the topmost network layer is L4.
- ``l2_ok``: all layer 2 HW integrity checks passed.
- ``l3_ok``: all layer 3 HW integrity checks passed.
- ``l4_ok``: all layer 4 HW integrity checks passed.
- ``l2_crc_ok``: layer 2 CRC check passed.
- ``ipv4_csum_ok``: IPv4 checksum check passed.
- ``l4_csum_ok``: layer 4 checksum check passed.
- ``l3_len_ok``: the layer 3 length is smaller than the frame length.

Item: ``CONNTRACK``
^^^^^^^^^^^^^^^^^^^

Matches a conntrack state after conntrack action.

- ``flags``: conntrack packet state flags.
- Default ``mask`` matches all state bits.

Item: ``PORT_REPRESENTOR``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches traffic entering the embedded switch from the given ethdev.

Term **ethdev** and the concept of **port representor** are synonymous.
The **represented port** is an *entity* plugged to the embedded switch
at the opposite end of the "wire" leading to the ethdev.

::

    .--------------------.
    |  PORT_REPRESENTOR  |  Ethdev (Application Port Referred to by its ID)
    '--------------------'
              ||
              \/
      .----------------.
      |  Logical Port  |
      '----------------'
              ||
              ||
              ||
              \/
         .----------.
         |  Switch  |
         '----------'
              :
               :
              :
               :
      .----------------.
      |  Logical Port  |
      '----------------'
              :
               :
    .--------------------.
    |  REPRESENTED_PORT  |  Net / Guest / Another Ethdev (Same Application)
    '--------------------'


- Incompatible with `Attribute: Traffic direction`_.
- Requires `Attribute: Transfer`_.

.. _table_rte_flow_item_ethdev:

.. table:: ``struct rte_flow_item_ethdev``

   +----------+-------------+---------------------------+
   | Field    | Subfield    | Value                     |
   +==========+=============+===========================+
   | ``spec`` | ``port_id`` | ethdev port ID            |
   +----------+-------------+---------------------------+
   | ``last`` | ``port_id`` | upper range value         |
   +----------+-------------+---------------------------+
   | ``mask`` | ``port_id`` | zeroed for wildcard match |
   +----------+-------------+---------------------------+

- Default ``mask`` provides exact match behaviour.

See also `Action: PORT_REPRESENTOR`_.

Item: ``REPRESENTED_PORT``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Matches traffic entering the embedded switch from
the entity represented by the given ethdev.

Term **ethdev** and the concept of **port representor** are synonymous.
The **represented port** is an *entity* plugged to the embedded switch
at the opposite end of the "wire" leading to the ethdev.

::

    .--------------------.
    |  PORT_REPRESENTOR  |  Ethdev (Application Port Referred to by its ID)
    '--------------------'
              :
               :
      .----------------.
      |  Logical Port  |
      '----------------'
              :
               :
              :
               :
         .----------.
         |  Switch  |
         '----------'
              /\
              ||
              ||
              ||
      .----------------.
      |  Logical Port  |
      '----------------'
              /\
              ||
    .--------------------.
    |  REPRESENTED_PORT  |  Net / Guest / Another Ethdev (Same Application)
    '--------------------'


- Incompatible with `Attribute: Traffic direction`_.
- Requires `Attribute: Transfer`_.

This item is meant to use the same structure as `Item: PORT_REPRESENTOR`_.

See also `Action: REPRESENTED_PORT`_.

Item: ``TX_QUEUE``
^^^^^^^^^^^^^^^^^^

Matches on the Tx queue of sent packet.

- ``tx_queue``: Tx queue.

Item: ``AGGR_AFFINITY``
^^^^^^^^^^^^^^^^^^^^^^^

Matches on the aggregated port of the received packet.
In case of multiple aggregated ports, the affinity numbering starts from 1.

- ``affinity``: Aggregated affinity.

Item: ``FLEX``
^^^^^^^^^^^^^^

Matches with the custom network protocol header that was created
using rte_flow_flex_item_create() API. The application describes
the desired header structure, defines the header fields attributes
and header relations with preceding and following protocols and
configures the ethernet devices accordingly via
rte_flow_flex_item_create() routine.

- ``handle``: the flex item handle returned by the PMD on successful
  rte_flow_flex_item_create() call, mask for this field is ignored.
- ``length``: match pattern length in bytes. If the length does not cover
  all fields defined in item configuration, the pattern spec and mask are
  considered by the driver as padded with trailing zeroes till the full
  configured item pattern length.
- ``pattern``: pattern to match. The pattern is concatenation of bit fields
  configured at item creation. At configuration the fields are presented
  by sample_data array. The order of the bitfields is defined by the order
  of sample_data elements. The width of each bitfield is defined by the width
  specified in the corresponding sample_data element as well. If pattern
  length is smaller than configured fields overall length it is considered
  as padded with trailing zeroes up to full configured length, both for
  value and mask.

Item: ``L2TPV2``
^^^^^^^^^^^^^^^^

Matches a L2TPv2 header.

- ``hdr``:  header definition (``rte_l2tpv2.h``).
- Default ``mask`` matches flags_version only.

Item: ``PPP``
^^^^^^^^^^^^^

Matches a PPP header.

- ``addr``: PPP address.
- ``ctrl``: PPP control.
- ``proto_id``: PPP protocol identifier.
- Default ``mask`` matches addr, ctrl, proto_id.

Item: ``METER_COLOR``
^^^^^^^^^^^^^^^^^^^^^

Matches Color Marker set by a Meter.

- ``color``: Metering color marker.

Item: ``QUOTA``
^^^^^^^^^^^^^^^

Matches flow quota state set by quota action.

- ``state``: Flow quota state

Item: ``IB_BTH``
^^^^^^^^^^^^^^^^

Matches an InfiniBand base transport header in RoCE packet.

- ``hdr``: InfiniBand base transport header definition (``rte_ib.h``).

Item: ``PTYPE``
^^^^^^^^^^^^^^^

Matches the packet type as defined in rte_mbuf_ptype.

- ``packet_type``: L2/L3/L4 and tunnel information.

Actions
~~~~~~~

Each possible action is represented by a type.
An action can have an associated configuration object.
Several actions combined in a list can be assigned
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
   | 1     | COUNT  | ``id``     | 0     |
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

Common action types are described in this section.

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
rule for that flow, then the behavior is undefined and the resulting behavior
is up to the specific device. Best practice when using groups would be to define
a default flow rule for each group which a defines the default actions in that
group so a consistent behavior is defined.

Defining an action for a matched flow in a group to jump to a group which is
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

Attaches an integer value to packets and sets ``RTE_MBUF_F_RX_FDIR`` and
``RTE_MBUF_F_RX_FDIR_ID`` mbuf flags.

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
sets the ``RTE_MBUF_F_RX_FDIR`` mbuf flag.

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


Action: ``SKIP_CMAN``
^^^^^^^^^^^^^^^^^^^^^

Skip congestion management on received packets.

- Using ``rte_eth_cman_config_set()``,
  an application can configure ethdev Rx queue's congestion mechanism.
  Once applied, packets congestion configuration is bypassed
  on that particular ethdev Rx queue for all packets directed to that queue.

.. _table_rte_flow_action_skip_cman:

.. table:: SKIP_CMAN

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

For ports within the same switch domain then the counter id namespace extends
to all ports within that switch domain.

.. _table_rte_flow_action_count:

.. table:: COUNT

   +------------+---------------------------------+
   | Field      | Value                           |
   +============+=================================+
   | ``id``     | counter id                      |
   +------------+---------------------------------+

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

If non-applicable for matching packets RSS types are requested,
these RSS types are simply ignored. For example, it happens if:

- Hashing of both TCP and UDP ports is requested
  (only one can be present in a packet).

- Requested RSS types contradict to flow rule pattern
  (e.g. pattern has UDP item, but RSS types contain TCP).

If requested RSS hash types are not supported by the Ethernet device at all
(not reported in ``dev_info.flow_type_rss_offloads``),
the flow creation will fail.

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

   +---------------+-------------------------------------------------+
   | Field         | Value                                           |
   +===============+=================================================+
   | ``func``      | RSS hash function to apply                      |
   +---------------+-------------------------------------------------+
   | ``level``     | encapsulation level for ``types``               |
   +---------------+-------------------------------------------------+
   | ``types``     | specific RSS hash types (see ``RTE_ETH_RSS_*``) |
   +---------------+-------------------------------------------------+
   | ``key_len``   | hash key length in bytes                        |
   +---------------+-------------------------------------------------+
   | ``queue_num`` | number of entries in ``queue``                  |
   +---------------+-------------------------------------------------+
   | ``key``       | hash key                                        |
   +---------------+-------------------------------------------------+
   | ``queue``     | queue indices to use                            |
   +---------------+-------------------------------------------------+

Action: ``PF``
^^^^^^^^^^^^^^

This action is deprecated. Consider:
 - `Action: PORT_REPRESENTOR`_
 - `Action: REPRESENTED_PORT`_

Directs matching traffic to the physical function (PF) of the current
device.

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

This action is deprecated. Consider:
 - `Action: PORT_REPRESENTOR`_
 - `Action: REPRESENTED_PORT`_

Directs matching traffic to a given virtual function of the current device.

Packets can be redirected to the VF they originate from,
instead of the specified one. This parameter may not be available and is
not guaranteed to work properly if the VF part is matched by a prior flow
rule or if packets are not addressed to a VF in the first place.

.. _table_rte_flow_action_vf:

.. table:: VF

   +--------------+--------------------------------+
   | Field        | Value                          |
   +==============+================================+
   | ``original`` | use original VF ID if possible |
   +--------------+--------------------------------+
   | ``id``       | VF ID                          |
   +--------------+--------------------------------+

Action: ``PORT_ID``
^^^^^^^^^^^^^^^^^^^
This action is deprecated. Consider:
 - `Action: PORT_REPRESENTOR`_
 - `Action: REPRESENTED_PORT`_

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

Action: ``OF_DEC_NW_TTL``
^^^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Implements ``OFPAT_DEC_NW_TTL`` ("decrement IP TTL") as defined by the
`OpenFlow Switch Specification`_.

.. _table_rte_flow_action_of_dec_nw_ttl:

.. table:: OF_DEC_NW_TTL

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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

Action: ``IPV6_EXT_PUSH``
^^^^^^^^^^^^^^^^^^^^^^^^^

Add an IPv6 extension into IPv6 header.
Its template is provided in its data buffer
with the specific type as defined in ``rte_flow_action_ipv6_ext_push``.

This action modifies the payload of matched flows.
The data supplied must be a valid extension in the specified type,
it should be added the last one if preceding extension existed.
When applied to the original packet,
the resulting packet must be a valid packet.

Action: ``IPV6_EXT_REMOVE``
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Remove an IPv6 extension whose type is provided in
``rte_flow_action_ipv6_ext_remove``.

This action modifies the payload of matched flow
and the packet should be valid after removing.

Action: ``SET_TP_SRC``
^^^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

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

Action: ``INC_TCP_SEQ``
^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Increase sequence number in the outermost TCP header.
Value to increase TCP sequence number by is a big-endian 32 bit integer.

Using this action on non-matching traffic will result in undefined behavior.

Action: ``DEC_TCP_SEQ``
^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Decrease sequence number in the outermost TCP header.
Value to decrease TCP sequence number by is a big-endian 32 bit integer.

Using this action on non-matching traffic will result in undefined behavior.

Action: ``INC_TCP_ACK``
^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Increase acknowledgment number in the outermost TCP header.
Value to increase TCP acknowledgment number by is a big-endian 32 bit integer.

Using this action on non-matching traffic will result in undefined behavior.

Action: ``DEC_TCP_ACK``
^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Decrease acknowledgment number in the outermost TCP header.
Value to decrease TCP acknowledgment number by is a big-endian 32 bit integer.

Using this action on non-matching traffic will result in undefined behavior.

Action: ``SET_TAG``
^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Set Tag.

Tag is a transient data used during flow matching. This is not delivered to
application. Multiple tags are supported by specifying index.

.. _table_rte_flow_action_set_tag:

.. table:: SET_TAG

   +-----------+----------------------------+
   | Field     | Value                      |
   +===========+============================+
   | ``data``  | 32 bit tag value           |
   +-----------+----------------------------+
   | ``mask``  | bit-mask applies to "data" |
   +-----------+----------------------------+
   | ``index`` | index of tag to set        |
   +-----------+----------------------------+

Action: ``SET_META``
^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Set metadata. Item ``META`` matches metadata.

Metadata set by mbuf metadata field with RTE_MBUF_DYNFLAG_TX_METADATA flag on egress
will be overridden by this action. On ingress, the metadata will be carried by
``metadata`` dynamic field of ``rte_mbuf`` which can be accessed by
``RTE_FLOW_DYNF_METADATA()``. RTE_MBUF_DYNFLAG_RX_METADATA flag will be set along
with the data.

The mbuf dynamic field must be registered by calling
``rte_flow_dynf_metadata_register()`` prior to use ``SET_META`` action.

Altering partial bits is supported with ``mask``. For bits which have never been
set, unpredictable value will be seen depending on driver implementation. For
loopback/hairpin packet, metadata set on Rx/Tx may or may not be propagated to
the other path depending on HW capability.

In hairpin case with Tx explicit flow mode, metadata could (not mandatory) be
used to connect the Rx and Tx flows if it can be propagated from Rx to Tx path.

.. _table_rte_flow_action_set_meta:

.. table:: SET_META

   +----------+----------------------------+
   | Field    | Value                      |
   +==========+============================+
   | ``data`` | 32 bit metadata value      |
   +----------+----------------------------+
   | ``mask`` | bit-mask applies to "data" |
   +----------+----------------------------+

Action: ``SET_IPV4_DSCP``
^^^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Set IPv4 DSCP.

Modify DSCP in IPv4 header.

It must be used with RTE_FLOW_ITEM_TYPE_IPV4 in pattern.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_ipv4_dscp:

.. table:: SET_IPV4_DSCP

   +-----------+---------------------------------+
   | Field     | Value                           |
   +===========+=================================+
   | ``dscp``  | DSCP in low 6 bits, rest ignore |
   +-----------+---------------------------------+

Action: ``SET_IPV6_DSCP``
^^^^^^^^^^^^^^^^^^^^^^^^^
This is a legacy action. Consider `Action: MODIFY_FIELD`_ as alternative.

Set IPv6 DSCP.

Modify DSCP in IPv6 header.

It must be used with RTE_FLOW_ITEM_TYPE_IPV6 in pattern.
Otherwise, RTE_FLOW_ERROR_TYPE_ACTION error will be returned.

.. _table_rte_flow_action_set_ipv6_dscp:

.. table:: SET_IPV6_DSCP

   +-----------+---------------------------------+
   | Field     | Value                           |
   +===========+=================================+
   | ``dscp``  | DSCP in low 6 bits, rest ignore |
   +-----------+---------------------------------+

Action: ``AGE``
^^^^^^^^^^^^^^^

Set ageing timeout configuration to a flow.

Event RTE_ETH_EVENT_FLOW_AGED will be reported if
timeout passed without any matching on the flow.

.. _table_rte_flow_action_age:

.. table:: AGE

   +--------------+---------------------------------+
   | Field        | Value                           |
   +==============+=================================+
   | ``timeout``  | 24 bits timeout value           |
   +--------------+---------------------------------+
   | ``reserved`` | 8 bits reserved, must be zero   |
   +--------------+---------------------------------+
   | ``context``  | user input flow context         |
   +--------------+---------------------------------+

Query structure to retrieve ageing status information of a
shared AGE action, or a flow rule using the AGE action:

.. _table_rte_flow_query_age:

.. table:: AGE query

   +------------------------------+-----+----------------------------------------+
   | Field                        | I/O | Value                                  |
   +==============================+=====+========================================+
   | ``aged``                     | out | Aging timeout expired                  |
   +------------------------------+-----+----------------------------------------+
   | ``sec_since_last_hit_valid`` | out | ``sec_since_last_hit`` value is valid  |
   +------------------------------+-----+----------------------------------------+
   | ``sec_since_last_hit``       | out | Seconds since last traffic hit         |
   +------------------------------+-----+----------------------------------------+

Update structure to modify the parameters of an indirect AGE action.
The update structure is used by ``rte_flow_action_handle_update()`` function.

.. _table_rte_flow_update_age:

.. table:: AGE update

   +-------------------+--------------------------------------------------------------+
   | Field             | Value                                                        |
   +===================+==============================================================+
   | ``reserved``      | 6 bits reserved, must be zero                                |
   +-------------------+--------------------------------------------------------------+
   | ``timeout_valid`` | 1 bit, timeout value is valid                                |
   +-------------------+--------------------------------------------------------------+
   | ``timeout``       | 24 bits timeout value                                        |
   +-------------------+--------------------------------------------------------------+
   | ``touch``         | 1 bit, touch the AGE action to set ``sec_since_last_hit`` 0  |
   +-------------------+--------------------------------------------------------------+

Action: ``SAMPLE``
^^^^^^^^^^^^^^^^^^

Adds a sample action to a matched flow.

The matching packets will be duplicated with the specified ``ratio`` and
applied with own set of actions with a fate action, the packets sampled
equals is '1/ratio'. All the packets continue to the target destination.

When the ``ratio`` is set to 1 then the packets will be 100% mirrored.
``actions`` represent the different set of actions for the sampled or mirrored
packets, and must have a fate action.

.. _table_rte_flow_action_sample:

.. table:: SAMPLE

   +--------------+---------------------------------+
   | Field        | Value                           |
   +==============+=================================+
   | ``ratio``    | 32 bits sample ratio value      |
   +--------------+---------------------------------+
   | ``actions``  | sub-action list for sampling    |
   +--------------+---------------------------------+

Action: ``INDIRECT``
^^^^^^^^^^^^^^^^^^^^

Flow utilize indirect action by handle as returned from
``rte_flow_action_handle_create()``.

The behaviour of the indirect action defined by ``action`` argument of type
``struct rte_flow_action`` passed to ``rte_flow_action_handle_create()``.

The indirect action can be used by a single flow or shared among multiple flows.
The indirect action can be in-place updated by ``rte_flow_action_handle_update()``
without destroying flow and creating flow again. The fields that could be
updated depend on the type of the ``action`` and different for every type.

The indirect action specified data (e.g. counter) can be queried by
``rte_flow_action_handle_query()``.

.. warning::

   The following description of indirect action persistence
   is an experimental behavior that may change without a prior notice.

If ``RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP`` is not advertised,
indirect actions cannot be created until the device is started for the first time
and cannot be kept when the device is stopped.
However, PMD also does not flush them automatically on stop,
so the application must call ``rte_flow_action_handle_destroy()``
before stopping the device to ensure no indirect actions remain.

If ``RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP`` is advertised,
this means that the PMD can keep at least some indirect actions
across device stop and start.
However, ``rte_eth_dev_configure()`` may fail if any indirect actions remain,
so the application must destroy them before attempting a reconfiguration.
Keeping may be only supported for certain kinds of indirect actions.
A kind is a combination of an action type and a value of its transfer bit.
For example: an indirect counter with the transfer bit reset.
To test if a particular kind of indirect actions is kept,
the application must try to create a valid indirect action of that kind
when the device is not started (either before the first start of after a stop).
If it fails with an error of type ``RTE_FLOW_ERROR_TYPE_STATE``,
application must destroy all indirect actions of this kind
before stopping the device.
If it succeeds, all indirect actions of the same kind are kept
when the device is stopped.
Indirect actions of a kept kind that are created when the device is stopped,
including the ones created for the test, will be kept after the device start.

.. _table_rte_flow_action_handle:

.. table:: INDIRECT

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``INDIRECT_LIST``
^^^^^^^^^^^^^^^^^^^^^^^^^

Indirect API creates a shared flow action with unique action handle.
Flow rules can access the shared flow action and resources related to
that action through the indirect action handle.
In addition, the API allows to update existing shared flow action configuration.
After the update completes, new action configuration
is available to all flows that reference that shared action.

Indirect actions list expands the indirect action API:

- Indirect action list creates a handle for one or several
  flow actions, while legacy indirect action handle references
  single action only.
  Input flow actions arranged in END terminated list.

- Flow rule can provide rule specific configuration parameters to
  existing shared handle.
  Updates of flow rule specific configuration will not change the base
  action configuration.
  Base action configuration was set during the action creation.

Indirect action list handle defines 2 types of resources:

- Mutable handle resource can be changed during handle lifespan.

- Immutable handle resource value is set during handle creation
  and cannot be changed.

There are 2 types of mutable indirect handle contexts:

- Action mutable context is always shared between all flows
  that referenced indirect actions list handle.
  Action mutable context can be changed by explicit invocation
  of indirect handle update function.

- Flow mutable context is private to a flow.
  Flow mutable context can be updated by indirect list handle
  flow rule configuration.

Indirect action types - immutable, action / flow mutable, are mutually
exclusive and depend on the action definition.

If indirect list handle was created from a list of actions A1 / A2 ... An / END
indirect list flow action can update Ai flow mutable context in the
action configuration parameter.
Indirect list action configuration is and array [C1, C2,  .., Cn]
where Ci corresponds to Ai in the action handle source.
Ci configuration element points Ai flow mutable update, or it's NULL
if Ai has no flow mutable update.
Indirect list action configuration is NULL if the action has no flow mutable updates.
Otherwise it points to an array of n flow mutable configuration pointers.

**Template API:**

*Action template format:*

``template .. indirect_list handle Htmpl conf Ctmpl ..``

``mask     .. indirect_list handle Hmask conf Cmask ..``

- If Htmpl was masked (Hmask != 0), it will be fixed in that template.
  Otherwise, indirect action value is set in a flow rule.

- If Htmpl and Ctmpl[i] were masked (Hmask !=0 and Cmask[i] != 0),
  Htmpl's Ai action flow mutable context fill be updated to
  Ctmpl[i] values and will be fixed in that template.

*Flow rule format:*

``actions .. indirect_list handle Hflow conf Cflow ..``

- If Htmpl was not masked in actions template, Hflow references an
  action of the same type as Htmpl.

- Cflow[i] updates handle's Ai flow mutable configuration if
  the Ci was not masked in action template.

.. _table_rte_flow_action_indirect_list:

.. table:: INDIRECT_LIST

   +------------------+----------------------------------+
   | Field            | Value                            |
   +==================+==================================+
   | ``handle``       | Indirect action list handle      |
   +------------------+----------------------------------+
   | ``conf``         | Flow mutable configuration array |
   +------------------+----------------------------------+

.. code-block:: text

   flow 1:
    / indirect handle H conf C1 /
                      |       |
                      |       |
                      |       |         flow 2:
                      |       |         / indirect handle H conf C2 /
                      |       |                           |      |
                      |       |                           |      |
                      |       |                           |      |
              =========================================================
              ^       |       |                           |      |
              |       |       V                           |      V
              |    ~~~~~~~~~~~~~~                      ~~~~~~~~~~~~~~~
              |     flow mutable                        flow mutable
              |     context 1                           context 2
              |    ~~~~~~~~~~~~~~                      ~~~~~~~~~~~~~~~
    indirect  |       |                                   |
    action    |       |                                   |
    context   |       V                                   V
              |   -----------------------------------------------------
              |                 action mutable context
              |   -----------------------------------------------------
              v                action immutable context
              =========================================================

Action: ``MODIFY_FIELD``
^^^^^^^^^^^^^^^^^^^^^^^^

Modify ``dst`` field according to ``op`` selected (set, addition,
subtraction) with ``width`` bits of data from ``src`` field.

Any arbitrary header field (as well as mark, metadata or tag values)
can be used as both source and destination fields as set by ``field``.
The immediate value ``RTE_FLOW_FIELD_VALUE`` (or a pointer to it
``RTE_FLOW_FIELD_POINTER``) is allowed as a source only.
``RTE_FLOW_FIELD_START`` is used to point to the beginning of a packet.
See ``enum rte_flow_field_id`` for the list of supported fields.

``op`` selects the operation to perform on a destination field:

- ``set`` copies the data from ``src`` field to ``dst`` field.
- ``add`` adds together ``dst`` and ``src`` and stores the result into ``dst``.
- ``sub`` subtracts ``src`` from ``dst`` and stores the result into ``dst``.

``width`` defines a number of bits to use from ``src`` field.

``level`` is used to access any packet field on any encapsulation level:

- ``0`` means the default behaviour. Depending on the packet type,
  it can mean outermost, innermost or anything in between.
- ``1`` requests access to the outermost packet encapsulation level.
- ``2`` and subsequent values requests access to the specified packet
  encapsulation level, from outermost to innermost (lower to higher values).

``tag_index`` is the index of the header inside encapsulation level.
It is used to modify either ``VLAN`` or ``MPLS`` or ``TAG`` headers
which multiple of them might be supported in the same encapsulation level.

.. note::

   For ``RTE_FLOW_FIELD_TAG`` type, the tag array was provided in ``level``
   field and it is still supported for backwards compatibility.
   When ``tag_index`` is zero, the tag array is taken from ``level`` field.

``type`` is used to specify (along with ``class_id``) the Geneve option
which is being modified.
This field is relevant only for ``RTE_FLOW_FIELD_GENEVE_OPT_XXXX`` type.

``class_id`` is used to specify (along with ``type``) the Geneve option
which is being modified.
This field is relevant only for ``RTE_FLOW_FIELD_GENEVE_OPT_XXXX`` type.

``flex_handle`` is used to specify the flex item pointer which is being
modified. ``flex_handle`` and ``level`` are mutually exclusive.

``offset`` specifies the number of bits to skip from a field's start.
That allows performing a partial copy of the needed part or to divide a big
packet field into multiple smaller fields. Alternatively, ``offset`` allows
going past the specified packet field boundary to copy a field to an
arbitrary place in a packet, essentially providing a way to copy any part of
a packet to any other part of it.

``value`` sets an immediate value to be used as a source or points to a
location of the value in memory. It is used instead of ``level`` and ``offset``
for ``RTE_FLOW_FIELD_VALUE`` and ``RTE_FLOW_FIELD_POINTER`` respectively.
The data in memory should be presented exactly in the same byte order and
length as in the relevant flow item, i.e. data for field with type
``RTE_FLOW_FIELD_MAC_DST`` should follow the conventions of ``dst`` field
in ``rte_flow_item_eth`` structure, with type ``RTE_FLOW_FIELD_IPV6_SRC`` -
``rte_flow_item_ipv6`` conventions, and so on. If the field size is larger than
16 bytes the pattern can be provided as pointer only.

The bitfield extracted from the memory being applied as second operation
parameter is defined by action width and by the destination field offset.
Application should provide the data in immediate value memory (either as
buffer or by pointer) exactly as item field without any applied explicit offset,
and destination packet field (with specified width and bit offset) will be
replaced by immediate source bits from the same bit offset. For example,
to replace the third byte of MAC address with value 0x85, application should
specify destination width as 8, destination offset as 16, and provide immediate
value as sequence of bytes {xxx, xxx, 0x85, xxx, xxx, xxx}.

The ``RTE_FLOW_FIELD_GENEVE_OPT_DATA`` type supports modifying only one DW in
single action and align to 32 bits.
For example, for modifying 16 bits start from offset 24,
2 different actions should be prepared.
The first one includes ``offset=24`` and ``width=8``,
and the second one includes ``offset=32`` and ``width=8``.
Application should provide the data in immediate value memory only
for the single DW even though the offset is related to start of first DW.
For example, to replace the third byte of second DW in Geneve option data
with value ``0x85``, the application should specify destination width as ``8``,
destination offset as ``48``, and provide immediate value ``0xXXXX85XX``.

.. _table_rte_flow_action_modify_field:

.. table:: MODIFY_FIELD

   +---------------+-------------------------+
   | Field         | Value                   |
   +===============+=========================+
   | ``op``        | operation to perform    |
   +---------------+-------------------------+
   | ``dst``       | destination field       |
   +---------------+-------------------------+
   | ``src``       | source field            |
   +---------------+-------------------------+
   | ``width``     | number of bits to use   |
   +---------------+-------------------------+

.. _table_rte_flow_action_modify_data:

.. table:: destination/source field definition

   +-----------------+----------------------------------------------------------+
   | Field           | Value                                                    |
   +=================+==========================================================+
   | ``field``       | ID: packet field, mark, meta, tag, immediate, pointer    |
   +-----------------+----------------------------------------------------------+
   | ``level``       | encapsulation level of a packet field                    |
   +-----------------+----------------------------------------------------------+
   | ``tag_index``   | tag index inside encapsulation level                     |
   +-----------------+----------------------------------------------------------+
   | ``type``        | Geneve option type                                       |
   +-----------------+----------------------------------------------------------+
   | ``class_id``    | Geneve option class ID                                   |
   +-----------------+----------------------------------------------------------+
   | ``flex_handle`` | flex item handle of a packet field                       |
   +-----------------+----------------------------------------------------------+
   | ``offset``      | number of bits to skip at the beginning                  |
   +-----------------+----------------------------------------------------------+
   | ``value``       | immediate value buffer (source field only, not           |
   |                 | applicable to destination) for RTE_FLOW_FIELD_VALUE      |
   |                 | field type                                               |
   |                 | This field is only 16 bytes, maybe not big enough for    |
   |                 | all NICs' flex item                                      |
   +-----------------+----------------------------------------------------------+
   | ``pvalue``      | pointer to immediate value data (source field only, not  |
   |                 | applicable to destination) for RTE_FLOW_FIELD_POINTER    |
   |                 | field type                                               |
   +-----------------+----------------------------------------------------------+

Action: ``CONNTRACK``
^^^^^^^^^^^^^^^^^^^^^

Create a conntrack (connection tracking) context with the provided information.

In stateful session like TCP, the conntrack action provides the ability to
examine every packet of this connection and associate the state to every
packet. It will help to realize the stateful offload of connections with little
software participation. For example, the packets with invalid state may be
handled by the software. The control packets could be handled in the hardware.
The software just need to query the state of a connection when needed, and then
decide how to handle the flow rules and conntrack context.

A conntrack context should be created via ``rte_flow_action_handle_create()``
before using. Then the handle with ``INDIRECT`` type is used for a flow rule
creation. If a flow rule with an opposite direction needs to be created, the
``rte_flow_action_handle_update()`` should be used to modify the direction.

Not all the fields of the ``struct rte_flow_action_conntrack`` will be used
for a conntrack context creating, depending on the HW, and they should be
in host byte order. PMD should convert them into network byte order when
needed by the HW.

The ``struct rte_flow_modify_conntrack`` should be used for an updating.

The current conntrack context information could be queried via the
``rte_flow_action_handle_query()`` interface.

.. _table_rte_flow_action_conntrack:

.. table:: CONNTRACK

   +--------------------------+-------------------------------------------------------------+
   | Field                    | Value                                                       |
   +==========================+=============================================================+
   | ``peer_port``            | peer port number                                            |
   +--------------------------+-------------------------------------------------------------+
   | ``is_original_dir``      | direction of this connection for creating flow rule         |
   +--------------------------+-------------------------------------------------------------+
   | ``enable``               | enable the conntrack context                                |
   +--------------------------+-------------------------------------------------------------+
   | ``live_connection``      | one ack was seen for this connection                        |
   +--------------------------+-------------------------------------------------------------+
   | ``selective_ack``        | SACK enabled                                                |
   +--------------------------+-------------------------------------------------------------+
   | ``challenge_ack_passed`` | a challenge ack has passed                                  |
   +--------------------------+-------------------------------------------------------------+
   | ``last_direction``       | direction of the last passed packet                         |
   +--------------------------+-------------------------------------------------------------+
   | ``liberal_mode``         | only report state change                                    |
   +--------------------------+-------------------------------------------------------------+
   | ``state``                | current state                                               |
   +--------------------------+-------------------------------------------------------------+
   | ``max_ack_window``       | maximal window scaling factor                               |
   +--------------------------+-------------------------------------------------------------+
   | ``retransmission_limit`` | maximal retransmission times                                |
   +--------------------------+-------------------------------------------------------------+
   | ``original_dir``         | TCP parameters of the original direction                    |
   +--------------------------+-------------------------------------------------------------+
   | ``reply_dir``            | TCP parameters of the reply direction                       |
   +--------------------------+-------------------------------------------------------------+
   | ``last_window``          | window size of the last passed packet                       |
   +--------------------------+-------------------------------------------------------------+
   | ``last_seq``             | sequence number of the last passed packet                   |
   +--------------------------+-------------------------------------------------------------+
   | ``last_ack``             | acknowledgment number the last passed packet                |
   +--------------------------+-------------------------------------------------------------+
   | ``last_end``             | sum of ack number and length of the last passed packet      |
   +--------------------------+-------------------------------------------------------------+

.. _table_rte_flow_tcp_dir_param:

.. table:: configuration parameters for each direction

   +---------------------+---------------------------------------------------------+
   | Field               | Value                                                   |
   +=====================+=========================================================+
   | ``scale``           | TCP window scaling factor                               |
   +---------------------+---------------------------------------------------------+
   | ``close_initiated`` | FIN sent from this direction                            |
   +---------------------+---------------------------------------------------------+
   | ``last_ack_seen``   | an ACK packet received                                  |
   +---------------------+---------------------------------------------------------+
   | ``data_unacked``    | unacknowledged data for packets from this direction     |
   +---------------------+---------------------------------------------------------+
   | ``sent_end``        | max{seq + len} seen in sent packets                     |
   +---------------------+---------------------------------------------------------+
   | ``reply_end``       | max{sack + max{win, 1}} seen in reply packets           |
   +---------------------+---------------------------------------------------------+
   | ``max_win``         | max{max{win, 1}} + {sack - ack} seen in sent packets    |
   +---------------------+---------------------------------------------------------+
   | ``max_ack``         | max{ack} + seen in sent packets                         |
   +---------------------+---------------------------------------------------------+

.. _table_rte_flow_modify_conntrack:

.. table:: update a conntrack context

   +----------------+-------------------------------------------------+
   | Field          | Value                                           |
   +================+=================================================+
   | ``new_ct``     | new conntrack information                       |
   +----------------+-------------------------------------------------+
   | ``direction``  | direction will be updated                       |
   +----------------+-------------------------------------------------+
   | ``state``      | other fields except direction will be updated   |
   +----------------+-------------------------------------------------+
   | ``reserved``   | reserved bits                                   |
   +----------------+-------------------------------------------------+

Action: ``METER_COLOR``
^^^^^^^^^^^^^^^^^^^^^^^

Color the packet to reflect the meter color result.

The meter action must be configured before meter color action.
Meter color action is set to a color to reflect the meter color result.
Set the meter color in the mbuf to the selected color.
The meter color action output color is the output color of the packet,
which is set in the packet meta-data (i.e. struct ``rte_mbuf::sched::color``)

.. _table_rte_flow_action_meter_color:

.. table:: METER_COLOR

   +-----------------+--------------+
   | Field           | Value        |
   +=================+==============+
   | ``meter_color`` | Packet color |
   +-----------------+--------------+

Action: ``PORT_REPRESENTOR``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

At embedded switch level, send matching traffic to the given ethdev.

Term **ethdev** and the concept of **port representor** are synonymous.
The **represented port** is an *entity* plugged to the embedded switch
at the opposite end of the "wire" leading to the ethdev.

::

    .--------------------.
    |  PORT_REPRESENTOR  |  Ethdev (Application Port Referred to by its ID)
    '--------------------'
              /\
              ||
      .----------------.
      |  Logical Port  |
      '----------------'
              /\
              ||
              ||
              ||
         .----------.       .--------------------.
         |  Switch  |  <==  |  Matching Traffic  |
         '----------'       '--------------------'
              :
               :
              :
               :
      .----------------.
      |  Logical Port  |
      '----------------'
              :
               :
    .--------------------.
    |  REPRESENTED_PORT  |  Net / Guest / Another Ethdev (Same Application)
    '--------------------'


- Requires `Attribute: Transfer`_.

.. _table_rte_flow_action_ethdev:

.. table:: ``struct rte_flow_action_ethdev``

   +-------------+----------------+
   | Field       | Value          |
   +=============+================+
   | ``port_id`` | ethdev port ID |
   +-------------+----------------+

See also `Item: PORT_REPRESENTOR`_.

Action: ``REPRESENTED_PORT``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

At embedded switch level, send matching traffic to
the entity represented by the given ethdev.

Term **ethdev** and the concept of **port representor** are synonymous.
The **represented port** is an *entity* plugged to the embedded switch
at the opposite end of the "wire" leading to the ethdev.

::

    .--------------------.
    |  PORT_REPRESENTOR  |  Ethdev (Application Port Referred to by its ID)
    '--------------------'
              :
               :
      .----------------.
      |  Logical Port  |
      '----------------'
              :
               :
              :
               :
         .----------.       .--------------------.
         |  Switch  |  <==  |  Matching Traffic  |
         '----------'       '--------------------'
              ||
              ||
              ||
              \/
      .----------------.
      |  Logical Port  |
      '----------------'
              ||
              \/
    .--------------------.
    |  REPRESENTED_PORT  |  Net / Guest / Another Ethdev (Same Application)
    '--------------------'


- Requires `Attribute: Transfer`_.

This action is meant to use the same structure as `Action: PORT_REPRESENTOR`_.

See also `Item: REPRESENTED_PORT`_.

Action: ``METER_MARK``
^^^^^^^^^^^^^^^^^^^^^^

Meters a packet stream and marks its packets with colors.

Unlike the ``METER`` action, policing is optional and may be
performed later with the help of the ``METER_COLOR`` item.
The profile and/or policy objects have to be created
using the rte_mtr_profile_add()/rte_mtr_policy_add() API.
Pointers to these objects are used as action parameters
and need to be retrieved using the rte_mtr_profile_get() API
and rte_mtr_policy_get() API respectively.

.. _table_rte_flow_action_meter_mark:

.. table:: METER_MARK

   +------------------+----------------------+
   | Field            | Value                |
   +==================+======================+
   | ``profile``      | Meter profile object |
   +------------------+----------------------+
   | ``policy``       | Meter policy object  |
   +------------------+----------------------+

Action: ``QUOTA``
^^^^^^^^^^^^^^^^^

Update ``quota`` value and set packet quota state.

If the ``quota`` value after update is non-negative,
the packet quota state is set to ``RTE_FLOW_QUOTA_STATE_PASS``.
Otherwise, the packet quota state is set to ``RTE_FLOW_QUOTA_STATE_BLOCK``.

The ``quota`` value is reduced according to ``mode`` setting.

.. _table_rte_flow_action_quota:

.. table:: QUOTA

   +------------------+------------------------+
   | Field            | Value                  |
   +==================+========================+
   | ``mode``         | Quota operational mode |
   +------------------+------------------------+
   | ``quota``        | Quota value            |
   +------------------+------------------------+

.. _rte_flow_quota_mode:

.. table:: Quota update modes

   +---------------------------------+-------------------------------------+
   | Value                           | Description                         |
   +=================================+=====================================+
   | ``RTE_FLOW_QUOTA_MODE_PACKET``  | Count packets                       |
   +---------------------------------+-------------------------------------+
   | ``RTE_FLOW_QUOTA_MODE_L2``      | Count packet bytes starting from L2 |
   +------------------+----------------------------------------------------+
   | ``RTE_FLOW_QUOTA_MODE_L3``      | Count packet bytes starting from L3 |
   +------------------+----------------------------------------------------+

Action: ``SEND_TO_KERNEL``
^^^^^^^^^^^^^^^^^^^^^^^^^^

Send packets to the kernel, without going to userspace at all.

The packets will be received by the kernel driver sharing the same device
as the DPDK port on which this action is configured.


Negative types
~~~~~~~~~~~~~~

All specified pattern items (``enum rte_flow_item_type``) and actions
(``enum rte_flow_action_type``) use positive identifiers.

The negative space is reserved for dynamic types generated by PMDs during
run-time. PMDs may encounter them as a result but must not accept negative
identifiers they are not aware of.

A method to generate them remains to be defined.

Application may use PMD dynamic items or actions in flow rules. In that case
size of configuration object in dynamic element must be a pointer size.

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

Update
~~~~~~

Update an existing flow rule with a new set of actions.

.. code-block:: c

   struct rte_flow *
   rte_flow_actions_update(uint16_t port_id,
                           struct rte_flow *flow,
                           const struct rte_flow_action *actions[],
                           struct rte_flow_error *error);

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``flow``: flow rule handle to update.
- ``actions``: associated actions (list terminated by the END action).
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

Flow engine configuration
-------------------------

Configure flow API management.

An application may provide some parameters at the initialization phase about
rules engine configuration and/or expected flow rules characteristics.
These parameters may be used by PMD to preallocate resources and configure NIC.

Configuration
~~~~~~~~~~~~~

This function performs the flow API engine configuration and allocates
requested resources beforehand to avoid costly allocations later.
Expected number of resources in an application allows PMD to prepare
and optimize NIC hardware configuration and memory layout in advance.
``rte_flow_configure()`` must be called before any flow rule is created,
but after an Ethernet device is configured.
It also creates flow queues for asynchronous flow rules operations via
queue-based API, see `Asynchronous operations`_ section.

.. code-block:: c

   int
   rte_flow_configure(uint16_t port_id,
                      const struct rte_flow_port_attr *port_attr,
                      uint16_t nb_queue,
                      const struct rte_flow_queue_attr *queue_attr[],
                      struct rte_flow_error *error);

Information about the number of available resources can be retrieved via
``rte_flow_info_get()`` API.

.. code-block:: c

   int
   rte_flow_info_get(uint16_t port_id,
                     struct rte_flow_port_info *port_info,
                     struct rte_flow_queue_info *queue_info,
                     struct rte_flow_error *error);

Group Miss Actions
~~~~~~~~~~~~~~~~~~

In an application, many flow rules share common group attributes, meaning they can be grouped and
classified together. A user can explicitly specify a set of actions performed on a packet when it
did not match any flows rules in a group using the following API:

.. code-block:: c

      int
      rte_flow_group_set_miss_actions(uint16_t port_id,
                                      uint32_t group_id,
                                      const struct rte_flow_group_attr *attr,
                                      const struct rte_flow_action actions[],
                                      struct rte_flow_error *error);

For example, to configure a RTE_FLOW_TYPE_JUMP action as a miss action for ingress group 1:

.. code-block:: c

      struct rte_flow_group_attr attr = {.ingress = 1};
      struct rte_flow_action act[] = {
      /* Setting miss actions to jump to group 3 */
          [0] = {.type = RTE_FLOW_ACTION_TYPE_JUMP,
                 .conf = &(struct rte_flow_action_jump){.group = 3}},
          [1] = {.type = RTE_FLOW_ACTION_TYPE_END},
      };
      struct rte_flow_error err;
      rte_flow_group_set_miss_actions(port, 1, &attr, act, &err);

Flow templates
~~~~~~~~~~~~~~

Oftentimes in an application, many flow rules share a common structure
(the same pattern and/or action list) so they can be grouped and classified
together. This knowledge may be used as a source of optimization by a PMD/HW.
The flow rule creation is done by selecting a table, a pattern template
and an actions template (which are bound to the table), and setting unique
values for the items and actions. This API is not thread-safe.

Pattern templates
^^^^^^^^^^^^^^^^^

The pattern template defines a common pattern (the item mask) without values.
The mask value is used to select a field to match on, spec/last are ignored.
The pattern template may be used by multiple tables and must not be destroyed
until all these tables are destroyed first.

.. code-block:: c

   struct rte_flow_pattern_template *
   rte_flow_pattern_template_create(uint16_t port_id,
       const struct rte_flow_pattern_template_attr *template_attr,
       const struct rte_flow_item pattern[],
       struct rte_flow_error *error);

For example, to create a pattern template to match on the destination MAC:

.. code-block:: c

   const struct rte_flow_pattern_template_attr attr = {.ingress = 1};
   struct rte_flow_item_eth eth_m = {
       .dst.addr_bytes = "\xff\xff\xff\xff\xff\xff";
   };
   struct rte_flow_item pattern[] = {
       [0] = {.type = RTE_FLOW_ITEM_TYPE_ETH,
              .mask = &eth_m},
       [1] = {.type = RTE_FLOW_ITEM_TYPE_END,},
   };
   struct rte_flow_error err;

   struct rte_flow_pattern_template *pattern_template =
           rte_flow_pattern_template_create(port, &attr, &pattern, &err);

The concrete value to match on will be provided at the rule creation.

Actions templates
^^^^^^^^^^^^^^^^^

The actions template holds a list of action types to be used in flow rules.
The mask parameter allows specifying a shared constant value for every rule.
The actions template may be used by multiple tables and must not be destroyed
until all these tables are destroyed first.

.. code-block:: c

   struct rte_flow_actions_template *
   rte_flow_actions_template_create(uint16_t port_id,
       const struct rte_flow_actions_template_attr *template_attr,
       const struct rte_flow_action actions[],
       const struct rte_flow_action masks[],
       struct rte_flow_error *error);

For example, to create an actions template with the same Mark ID
but different Queue Index for every rule:

.. code-block:: c

   rte_flow_actions_template_attr attr = {.ingress = 1};
   struct rte_flow_action act[] = {
   /* Mark ID is 4 for every rule, Queue Index is unique */
       [0] = {.type = RTE_FLOW_ACTION_TYPE_MARK,
              .conf = &(struct rte_flow_action_mark){.id = 4}},
       [1] = {.type = RTE_FLOW_ACTION_TYPE_QUEUE},
       [2] = {.type = RTE_FLOW_ACTION_TYPE_END,},
   };
   struct rte_flow_action msk[] = {
   /* Assign to MARK mask any non-zero value to make it constant */
       [0] = {.type = RTE_FLOW_ACTION_TYPE_MARK,
              .conf = &(struct rte_flow_action_mark){.id = 1}},
       [1] = {.type = RTE_FLOW_ACTION_TYPE_QUEUE},
       [2] = {.type = RTE_FLOW_ACTION_TYPE_END,},
   };
   struct rte_flow_error err;

   struct rte_flow_actions_template *actions_template =
           rte_flow_actions_template_create(port, &attr, &act, &msk, &err);

The concrete value for Queue Index will be provided at the rule creation.

Template table
^^^^^^^^^^^^^^

A template table combines a number of pattern and actions templates along with
shared flow rule attributes (group ID, priority and traffic direction).
This way a PMD/HW can prepare all the resources needed for efficient flow rules
creation in the datapath. To avoid any hiccups due to memory reallocation,
the maximum number of flow rules is defined at table creation time.
Any flow rule creation beyond the maximum table size is rejected.
Application may create another table to accommodate more rules in this case.

.. code-block:: c

   struct rte_flow_template_table *
   rte_flow_template_table_create(uint16_t port_id,
       const struct rte_flow_template_table_attr *table_attr,
       struct rte_flow_pattern_template *pattern_templates[],
       uint8_t nb_pattern_templates,
       struct rte_flow_actions_template *actions_templates[],
       uint8_t nb_actions_templates,
       struct rte_flow_error *error);

A table can be created only after the Flow Rules management is configured
and pattern and actions templates are created.

.. code-block:: c

   rte_flow_template_table_attr table_attr = {
       .flow_attr.ingress = 1,
       .nb_flows = 10000;
   };
   uint8_t nb_pattern_templ = 1;
   struct rte_flow_pattern_template *pattern_templates[nb_pattern_templ];
   pattern_templates[0] = pattern_template;
   uint8_t nb_actions_templ = 1;
   struct rte_flow_actions_template *actions_templates[nb_actions_templ];
   actions_templates[0] = actions_template;
   struct rte_flow_error error;

   struct rte_flow_template_table *table =
           rte_flow_template_table_create(port, &table_attr,
                   &pattern_templates, nb_pattern_templ,
                   &actions_templates, nb_actions_templ,
                   &error);

Table Attribute: Specialize
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Application can help optimizing underlayer resources and insertion rate
by specializing template table.
Specialization is done by providing hints
in the template table attribute ``specialize``.

This attribute is not mandatory for driver to implement.
If a hint is not supported, it will be silently ignored,
and no special optimization is done.

If a table is specialized, the application should make sure the rules
comply with the table attribute.
The application functionality must not rely on the hints,
they are not replacing the matching criteria of flow rules.

Asynchronous operations
-----------------------

Flow rules management can be done via special lockless flow management queues.

- Queue operations are asynchronous and not thread-safe.

- Operations can thus be invoked by the app's datapath,
  packet processing can continue while queue operations are processed by NIC.

- Number of flow queues is configured at initialization stage.

- Available operation types: rule creation, rule destruction,
  indirect rule creation, indirect rule destruction, indirect rule update.

- Operations may be reordered within a queue.

- Operations can be postponed and pushed to NIC in batches.

- Results pulling must be done on time to avoid queue overflows.

- User data is returned as part of the result to identify an operation.

- Flow handle is valid once the creation operation is enqueued and must be
  destroyed even if the operation is not successful and the rule is not inserted.

- Application must wait for the creation operation result before enqueueing
  the deletion operation to make sure the creation is processed by NIC.

The asynchronous flow rule insertion logic can be broken into two phases.

#. Initialization stage as shown here:

   .. _figure_rte_flow_async_init:

   .. figure:: img/rte_flow_async_init.*

#. Main loop as presented on a datapath application example:

   .. _figure_rte_flow_async_usage:

   .. figure:: img/rte_flow_async_usage.*

Enqueue creation operation
~~~~~~~~~~~~~~~~~~~~~~~~~~

Enqueueing a flow rule creation operation is similar to simple creation.

.. code-block:: c

   struct rte_flow *
   rte_flow_async_create(uint16_t port_id,
                         uint32_t queue_id,
                         const struct rte_flow_op_attr *op_attr,
                         struct rte_flow_template_table *template_table,
                         const struct rte_flow_item pattern[],
                         uint8_t pattern_template_index,
                         const struct rte_flow_action actions[],
                         uint8_t actions_template_index,
                         void *user_data,
                         struct rte_flow_error *error);

A valid handle in case of success is returned. It must be destroyed later
by calling ``rte_flow_async_destroy()`` even if the rule is rejected by HW.

Enqueue creation by index operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enqueueing a flow rule creation operation to insert a rule at a table index.

.. code-block:: c

   struct rte_flow *
   rte_flow_async_create_by_index(uint16_t port_id,
                                  uint32_t queue_id,
                                  const struct rte_flow_op_attr *op_attr,
                                  struct rte_flow_template_table *template_table,
                                  uint32_t rule_index,
                                  const struct rte_flow_action actions[],
                                  uint8_t actions_template_index,
                                  void *user_data,
                                  struct rte_flow_error *error);

A valid handle in case of success is returned. It must be destroyed later
by calling ``rte_flow_async_destroy()`` even if the rule is rejected by HW.

Enqueue destruction operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Enqueueing a flow rule destruction operation is similar to simple destruction.

.. code-block:: c

   int
   rte_flow_async_destroy(uint16_t port_id,
                          uint32_t queue_id,
                          const struct rte_flow_op_attr *op_attr,
                          struct rte_flow *flow,
                          void *user_data,
                          struct rte_flow_error *error);

Enqueue update operation
~~~~~~~~~~~~~~~~~~~~~~~~

Enqueueing a flow rule update operation to replace actions in the existing rule.

.. code-block:: c

   int
   rte_flow_async_actions_update(uint16_t port_id,
                                 uint32_t queue_id,
                                 const struct rte_flow_op_attr *op_attr,
                                 struct rte_flow *flow,
                                 const struct rte_flow_action actions[],
                                 uint8_t actions_template_index,
                                 void *user_data,
                                 struct rte_flow_error *error);

Enqueue indirect action creation operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asynchronous version of indirect action creation API.

.. code-block:: c

   struct rte_flow_action_handle *
   rte_flow_async_action_handle_create(uint16_t port_id,
           uint32_t queue_id,
           const struct rte_flow_op_attr *q_ops_attr,
           const struct rte_flow_indir_action_conf *indir_action_conf,
           const struct rte_flow_action *action,
           void *user_data,
           struct rte_flow_error *error);

A valid handle in case of success is returned. It must be destroyed later by
``rte_flow_async_action_handle_destroy()`` even if the rule was rejected.

Enqueue indirect action destruction operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asynchronous version of indirect action destruction API.

.. code-block:: c

   int
   rte_flow_async_action_handle_destroy(uint16_t port_id,
           uint32_t queue_id,
           const struct rte_flow_op_attr *q_ops_attr,
           struct rte_flow_action_handle *action_handle,
           void *user_data,
           struct rte_flow_error *error);

Enqueue indirect action update operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asynchronous version of indirect action update API.

.. code-block:: c

   int
   rte_flow_async_action_handle_update(uint16_t port_id,
           uint32_t queue_id,
           const struct rte_flow_op_attr *q_ops_attr,
           struct rte_flow_action_handle *action_handle,
           const void *update,
           void *user_data,
           struct rte_flow_error *error);

Enqueue indirect action query operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Asynchronous version of indirect action query API.

.. code-block:: c

   int
   rte_flow_async_action_handle_query(uint16_t port_id,
           uint32_t queue_id,
           const struct rte_flow_op_attr *q_ops_attr,
           struct rte_flow_action_handle *action_handle,
           void *data,
           void *user_data,
           struct rte_flow_error *error);

Push enqueued operations
~~~~~~~~~~~~~~~~~~~~~~~~

Pushing all internally stored rules from a queue to the NIC.

.. code-block:: c

   int
   rte_flow_push(uint16_t port_id,
                 uint32_t queue_id,
                 struct rte_flow_error *error);

There is the postpone attribute in the queue operation attributes.
When it is set, multiple operations can be bulked together and not sent to HW
right away to save SW/HW interactions and prioritize throughput over latency.
The application must invoke this function to actually push all outstanding
operations to HW in this case.

Pull enqueued operations
~~~~~~~~~~~~~~~~~~~~~~~~

Pulling asynchronous operations results.

The application must invoke this function in order to complete asynchronous
flow rule operations and to receive flow rule operations statuses.

.. code-block:: c

   int
   rte_flow_pull(uint16_t port_id,
                 uint32_t queue_id,
                 struct rte_flow_op_result res[],
                 uint16_t n_res,
                 struct rte_flow_error *error);

Multiple outstanding operation results can be pulled simultaneously.
User data may be provided during a flow creation/destruction in order
to distinguish between multiple operations. User data is returned as part
of the result to provide a method to detect which operation is completed.

Calculate hash
~~~~~~~~~~~~~~

Calculating hash of a packet in SW as it would be calculated in HW.

The application can use this function to calculate the hash of a given packet
as it would be calculated in the HW.

.. code-block:: c

   int
   rte_flow_calc_table_hash(uint16_t port_id,
                            const struct rte_flow_template_table *table,
			                   const struct rte_flow_item pattern[],
                            uint8_t pattern_template_index,
			                   uint32_t *hash, struct rte_flow_error *error);

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

Tunneled traffic offload
~~~~~~~~~~~~~~~~~~~~~~~~

rte_flow API provides the building blocks for vendor-agnostic flow
classification offloads. The rte_flow "patterns" and "actions"
primitives are fine-grained, thus enabling DPDK applications the
flexibility to offload network stacks and complex pipelines.
Applications wishing to offload tunneled traffic are required to use
the rte_flow primitives, such as group, meta, mark, tag, and others to
model their high-level objects.  The hardware model design for
high-level software objects is not trivial.  Furthermore, an optimal
design is often vendor-specific.

When hardware offloads tunneled traffic in multi-group logic,
partially offloaded packets may arrive to the application after they
were modified in hardware. In this case, the application may need to
restore the original packet headers. Consider the following sequence:
The application decaps a packet in one group and jumps to a second
group where it tries to match on a 5-tuple, that will miss and send
the packet to the application. In this case, the application does not
receive the original packet but a modified one. Also, in this case,
the application cannot match on the outer header fields, such as VXLAN
vni and 5-tuple.

There are several possible ways to use rte_flow "patterns" and
"actions" to resolve the issues above. For example:

1 Mapping headers to a hardware registers using the
rte_flow_action_mark/rte_flow_action_tag/rte_flow_set_meta objects.

2 Apply the decap only at the last offload stage after all the
"patterns" were matched and the packet will be fully offloaded.

Every approach has its pros and cons and is highly dependent on the
hardware vendor.  For example, some hardware may have a limited number
of registers while other hardware could not support inner actions and
must decap before accessing inner headers.

The tunnel offload model resolves these issues. The model goals are:

1 Provide a unified application API to offload tunneled traffic that
is capable to match on outer headers after decap.

2 Allow the application to restore the outer header of partially
offloaded packets.

The tunnel offload model does not introduce new elements to the
existing RTE flow model and is implemented as a set of helper
functions.

For the application to work with the tunnel offload API it
has to adjust flow rules in multi-table tunnel offload in the
following way:

1 Remove explicit call to decap action and replace it with PMD actions
obtained from rte_flow_tunnel_decap_and_set() helper.

2 Add PMD items obtained from rte_flow_tunnel_match() helper to all
other rules in the tunnel offload sequence.

The model requirements:

Software application must initialize
rte_tunnel object with tunnel parameters before calling
rte_flow_tunnel_decap_set() & rte_flow_tunnel_match().

PMD actions array obtained in rte_flow_tunnel_decap_set() must be
released by application with rte_flow_action_release() call.

PMD items array obtained with rte_flow_tunnel_match() must be released
by application with rte_flow_item_release() call.  Application can
release PMD items and actions after rule was created. However, if the
application needs to create additional rule for the same tunnel it
will need to obtain PMD items again.

Application cannot destroy rte_tunnel object before it releases all
PMD actions & PMD items referencing that tunnel.

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

- Stopping the data path (TX/RX) should not be necessary when managing flow
  rules. If this cannot be achieved naturally or with workarounds (such as
  temporarily replacing the burst function pointers), an appropriate error
  code must be returned (``EBUSY``).

- Applications, not PMDs, are responsible for maintaining flow rules
  configuration when closing, stopping or restarting a port or performing other
  actions which may affect them.
  Applications must assume that after port close, stop or restart all flows
  related to that port are not valid, hardware rules are destroyed and relevant
  PMD resources are released.

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

The PMD interface is based on callbacks pointed by the ``struct rte_flow_ops``.

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

If PMD interfaces don't support re-entrancy/multi-thread safety,
the rte_flow API functions will protect threads by mutex per port.
The application can check whether ``RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE``
is set in ``dev_flags``, meaning the PMD is thread-safe regarding rte_flow,
so the API level protection is disabled.
Please note that this API-level mutex protects only rte_flow functions,
other control path functions are not in scope.

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


.. _OpenFlow Switch Specification: https://www.opennetworking.org/software-defined-standards/specifications/
