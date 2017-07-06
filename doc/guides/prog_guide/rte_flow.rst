..  BSD LICENSE
    Copyright 2016 6WIND S.A.
    Copyright 2016 Mellanox.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of 6WIND S.A. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

.. _Generic_flow_API:

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

Several methods to migrate existing applications are described in `API
migration`_.

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
group they belong to. All flow rules in a given group are thus processed
either before or after another group.

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

Flow rules can be grouped by assigning them a common group number. Lower
values have higher priority. Group 0 has the highest priority.

Although optional, applications are encouraged to group similar rules as
much as possible to fully take advantage of hardware capabilities
(e.g. optimized matching) and work around limitations (e.g. a single pattern
type possibly allowed in a given group).

Note that support for more than a single group is not guaranteed.

Attribute: Priority
^^^^^^^^^^^^^^^^^^^

A priority level can be assigned to a flow rule. Like groups, lower values
denote higher priority, with 0 as the maximum.

A rule with priority 0 in group 8 is always matched after a rule with
priority 8 in group 0.

Group and priority levels are arbitrary and up to the application, they do
not need to be contiguous nor start from 0, however the maximum number
varies between devices and may be affected by existing flow rules.

If a packet is matched by several rules of a given group for a given
priority level, the outcome is undefined. It can take any path, may be
duplicated or even cause unrecoverable errors.

Note that support for more than a single priority level is not guaranteed.

Attribute: Traffic direction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Flow rules can apply to inbound and/or outbound traffic (ingress/egress).

Several pattern items and actions are valid and can be used in both
directions. At least one direction must be specified.

Specifying both directions at once for a given rule is not recommended but
may be valid in a few cases (e.g. shared counters).

Pattern item
~~~~~~~~~~~~

Pattern items fall in two categories:

- Matching protocol headers and packet data (ANY, RAW, ETH, VLAN, IPV4,
  IPV6, ICMP, UDP, TCP, SCTP, VXLAN, MPLS, GRE and so on), usually
  associated with a specification structure.

- Matching meta-data or affecting pattern processing (END, VOID, INVERT, PF,
  VF, PORT and so on), often without a specification structure.

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

Matches packets addressed to the physical function of the device.

If the underlying device function differs from the one that would normally
receive the matched traffic, specifying this item prevents it from reaching
that device unless the flow rule contains a `Action: PF`_. Packets are not
duplicated between device instances by default.

- Likely to return an error or never match any traffic if applied to a VF
  device.
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

Matches packets addressed to a virtual function ID of the device.

If the underlying device function differs from the one that would normally
receive the matched traffic, specifying this item prevents it from reaching
that device unless the flow rule contains a `Action: VF`_. Packets are not
duplicated between device instances by default.

- Likely to return an error or never match any traffic if this causes a VF
  device to match traffic addressed to a different VF.
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

Item: ``PORT``
^^^^^^^^^^^^^^

Matches packets coming from the specified physical port of the underlying
device.

The first PORT item overrides the physical port normally associated with the
specified DPDK input port (port_id). This item can be provided several times
to match additional physical ports.

Note that physical ports are not necessarily tied to DPDK input ports
(port_id) when those are not under DPDK control. Possible values are
specific to each device, they are not necessarily indexed from zero and may
not be contiguous.

As a device property, the list of allowed values as well as the value
associated with a port_id should be retrieved by other means.

- Default ``mask`` matches any port index.

.. _table_rte_flow_item_port:

.. table:: PORT

   +----------+-----------+--------------------------------+
   | Field    | Subfield  | Value                          |
   +==========+===========+================================+
   | ``spec`` | ``index`` | physical port index            |
   +----------+-----------+--------------------------------+
   | ``last`` | ``index`` | upper range value              |
   +----------+-----------+--------------------------------+
   | ``mask`` | ``index`` | zeroed to match any port index |
   +----------+-----------+--------------------------------+

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

- ``dst``: destination MAC.
- ``src``: source MAC.
- ``type``: EtherType.
- Default ``mask`` matches destination and source addresses only.

Item: ``VLAN``
^^^^^^^^^^^^^^

Matches an 802.1Q/ad VLAN tag.

- ``tpid``: tag protocol identifier.
- ``tci``: tag control information.
- Default ``mask`` matches TCI only.

Item: ``IPV4``
^^^^^^^^^^^^^^

Matches an IPv4 header.

Note: IPv4 options are handled by dedicated pattern items.

- ``hdr``: IPv4 header definition (``rte_ip.h``).
- Default ``mask`` matches source and destination addresses only.

Item: ``IPV6``
^^^^^^^^^^^^^^

Matches an IPv6 header.

Note: IPv6 options are handled by dedicated pattern items.

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

- ``tpid``: tag protocol identifier (0x893F)
- ``epcp_edei_in_ecid_b``: E-Tag control information (E-TCI), E-PCP (3b),
  E-DEI (1b), ingress E-CID base (12b).
- ``rsvd_grp_ecid_b``: reserved (2b), GRP (2b), E-CID base (12b).
- ``in_ecid_e``: ingress E-CID ext.
- ``ecid_e``: E-CID ext.
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

Actions
~~~~~~~

Each possible action is represented by a type. Some have associated
configuration structures. Several actions combined in a list can be affected
to a flow rule. That list is not ordered.

They fall in three categories:

- Terminating actions (such as QUEUE, DROP, RSS, PF, VF) that prevent
  processing matched packets by subsequent flow rules, unless overridden
  with PASSTHRU.

- Non-terminating actions (PASSTHRU, DUP) that leave matched packets up for
  additional processing by subsequent flow rules.

- Other non-terminating meta actions that do not affect the fate of packets
  (END, VOID, MARK, FLAG, COUNT).

When several actions are combined in a flow rule, they should all have
different types (e.g. dropping a packet twice is not possible).

Only the last action of a given type is taken into account. PMDs still
perform error checking on the entire list.

Like matching patterns, action lists are terminated by END items.

*Note that PASSTHRU is the only action able to override a terminating rule.*

Example of action that redirects packets to queue index 10:

.. _table_rte_flow_action_example:

.. table:: Queue action

   +-----------+-------+
   | Field     | Value |
   +===========+=======+
   | ``index`` | 10    |
   +-----------+-------+

Action lists examples, their order is not significant, applications must
consider all actions to be performed simultaneously:

.. _table_rte_flow_count_and_drop:

.. table:: Count and drop

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

.. table:: Mark, count and redirect

   +-------+--------+-----------+-------+
   | Index | Action | Field     | Value |
   +=======+========+===========+=======+
   | 0     | MARK   | ``mark``  | 0x2a  |
   +-------+--------+-----------+-------+
   | 1     | COUNT                      |
   +-------+--------+-----------+-------+
   | 2     | QUEUE  | ``queue`` | 10    |
   +-------+--------+-----------+-------+
   | 3     | END                        |
   +-------+----------------------------+

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

In the above example, considering both actions are performed simultaneously,
the end result is that only QUEUE has any effect.

.. _table_rte_flow_redirect_queue_3:

.. table:: Redirect to queue 3

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

As previously described, only the last action of a given type found in the
list is taken into account. The above example also shows that VOID is
ignored.

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

Leaves packets up for additional processing by subsequent flow rules. This
is the default when a rule does not contain a terminating action, but can be
specified to force a rule to become non-terminating.

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

- Terminating by default.

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
- Terminating by default.
- PASSTHRU overrides this action if both are specified.

.. _table_rte_flow_action_drop:

.. table:: DROP

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``COUNT``
^^^^^^^^^^^^^^^^^

Enables counters for this rule.

These counters can be retrieved and reset through ``rte_flow_query()``, see
``struct rte_flow_query_count``.

- Counters can be retrieved with ``rte_flow_query()``.
- No configurable properties.

.. _table_rte_flow_action_count:

.. table:: COUNT

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

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

Action: ``DUP``
^^^^^^^^^^^^^^^

Duplicates packets to a given queue index.

This is normally combined with QUEUE, however when used alone, it is
actually similar to QUEUE + PASSTHRU.

- Non-terminating by default.

.. _table_rte_flow_action_dup:

.. table:: DUP

   +-----------+------------------------------------+
   | Field     | Value                              |
   +===========+====================================+
   | ``index`` | queue index to duplicate packet to |
   +-----------+------------------------------------+

Action: ``RSS``
^^^^^^^^^^^^^^^

Similar to QUEUE, except RSS is additionally performed on packets to spread
them among several queues according to the provided parameters.

Note: RSS hash result is stored in the ``hash.rss`` mbuf field which
overlaps ``hash.fdir.lo``. Since `Action: MARK`_ sets the ``hash.fdir.hi``
field only, both can be requested simultaneously.

- Terminating by default.

.. _table_rte_flow_action_rss:

.. table:: RSS

   +--------------+------------------------------+
   | Field        | Value                        |
   +==============+==============================+
   | ``rss_conf`` | RSS parameters               |
   +--------------+------------------------------+
   | ``num``      | number of entries in queue[] |
   +--------------+------------------------------+
   | ``queue[]``  | queue indices to use         |
   +--------------+------------------------------+

Action: ``PF``
^^^^^^^^^^^^^^

Redirects packets to the physical function (PF) of the current device.

- No configurable properties.
- Terminating by default.

.. _table_rte_flow_action_pf:

.. table:: PF

   +---------------+
   | Field         |
   +===============+
   | no properties |
   +---------------+

Action: ``VF``
^^^^^^^^^^^^^^

Redirects packets to a virtual function (VF) of the current device.

Packets matched by a VF pattern item can be redirected to their original VF
ID instead of the specified one. This parameter may not be available and is
not guaranteed to work properly if the VF part is matched by a prior flow
rule or if packets are not addressed to a VF in the first place.

- Terminating by default.

.. _table_rte_flow_action_vf:

.. table:: VF

   +--------------+--------------------------------+
   | Field        | Value                          |
   +==============+================================+
   | ``original`` | use original VF ID if possible |
   +--------------+--------------------------------+
   | ``vf``       | VF ID to redirect packets to   |
   +--------------+--------------------------------+

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
   rte_flow_validate(uint8_t port_id,
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
   rte_flow_create(uint8_t port_id,
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
   rte_flow_destroy(uint8_t port_id,
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
   rte_flow_flush(uint8_t port_id,
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
   rte_flow_query(uint8_t port_id,
                  struct rte_flow *flow,
                  enum rte_flow_action_type action,
                  void *data,
                  struct rte_flow_error *error);

Arguments:

- ``port_id``: port identifier of Ethernet device.
- ``flow``: flow rule handle to query.
- ``action``: action type to query.
- ``data``: pointer to storage for the associated query data type.
- ``error``: perform verbose error reporting if not NULL. PMDs initialize
  this structure in case of error only.

Return values:

- 0 on success, a negative errno value otherwise and ``rte_errno`` is set.

Isolated mode
-------------

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
   rte_flow_isolate(uint8_t port_id, int set, struct rte_flow_error *error);

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

- There is no provision for reentrancy/multi-thread safety, although nothing
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

This interface additionally defines the following helper functions:

- ``rte_flow_ops_get()``: get generic flow operations structure from a
  port.

- ``rte_flow_error_set()``: initialize generic flow error structure.

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

- A rule specifying both `Action: DUP`_ + `Action: QUEUE`_ may be translated
  to two hidden rules combining `Action: QUEUE`_ and `Action: PASSTHRU`_.

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

API migration
-------------

Exhaustive list of deprecated filter types (normally prefixed with
*RTE_ETH_FILTER_*) found in ``rte_eth_ctrl.h`` and methods to convert them
to *rte_flow* rules.

``MACVLAN`` to ``ETH`` → ``VF``, ``PF``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*MACVLAN* can be translated to a basic `Item: ETH`_ flow rule with a
terminating `Action: VF`_ or `Action: PF`_.

.. _table_rte_flow_migration_macvlan:

.. table:: MACVLAN conversion

   +--------------------------+---------+
   | Pattern                  | Actions |
   +===+=====+==========+=====+=========+
   | 0 | ETH | ``spec`` | any | VF,     |
   |   |     +----------+-----+ PF      |
   |   |     | ``last`` | N/A |         |
   |   |     +----------+-----+         |
   |   |     | ``mask`` | any |         |
   +---+-----+----------+-----+---------+
   | 1 | END                  | END     |
   +---+----------------------+---------+

``ETHERTYPE`` to ``ETH`` → ``QUEUE``, ``DROP``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*ETHERTYPE* is basically an `Item: ETH`_ flow rule with a terminating
`Action: QUEUE`_ or `Action: DROP`_.

.. _table_rte_flow_migration_ethertype:

.. table:: ETHERTYPE conversion

   +--------------------------+---------+
   | Pattern                  | Actions |
   +===+=====+==========+=====+=========+
   | 0 | ETH | ``spec`` | any | QUEUE,  |
   |   |     +----------+-----+ DROP    |
   |   |     | ``last`` | N/A |         |
   |   |     +----------+-----+         |
   |   |     | ``mask`` | any |         |
   +---+-----+----------+-----+---------+
   | 1 | END                  | END     |
   +---+----------------------+---------+

``FLEXIBLE`` to ``RAW`` → ``QUEUE``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*FLEXIBLE* can be translated to one `Item: RAW`_ pattern with a terminating
`Action: QUEUE`_ and a defined priority level.

.. _table_rte_flow_migration_flexible:

.. table:: FLEXIBLE conversion

   +--------------------------+---------+
   | Pattern                  | Actions |
   +===+=====+==========+=====+=========+
   | 0 | RAW | ``spec`` | any | QUEUE   |
   |   |     +----------+-----+         |
   |   |     | ``last`` | N/A |         |
   |   |     +----------+-----+         |
   |   |     | ``mask`` | any |         |
   +---+-----+----------+-----+---------+
   | 1 | END                  | END     |
   +---+----------------------+---------+

``SYN`` to ``TCP`` → ``QUEUE``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*SYN* is a `Item: TCP`_ rule with only the ``syn`` bit enabled and masked,
and a terminating `Action: QUEUE`_.

Priority level can be set to simulate the high priority bit.

.. _table_rte_flow_migration_syn:

.. table:: SYN conversion

   +-----------------------------------+---------+
   | Pattern                           | Actions |
   +===+======+==========+=============+=========+
   | 0 | ETH  | ``spec`` | unset       | QUEUE   |
   |   |      +----------+-------------+         |
   |   |      | ``last`` | unset       |         |
   |   |      +----------+-------------+         |
   |   |      | ``mask`` | unset       |         |
   +---+------+----------+-------------+---------+
   | 1 | IPV4 | ``spec`` | unset       | END     |
   |   |      +----------+-------------+         |
   |   |      | ``mask`` | unset       |         |
   |   |      +----------+-------------+         |
   |   |      | ``mask`` | unset       |         |
   +---+------+----------+---------+---+         |
   | 2 | TCP  | ``spec`` | ``syn`` | 1 |         |
   |   |      +----------+---------+---+         |
   |   |      | ``mask`` | ``syn`` | 1 |         |
   +---+------+----------+---------+---+         |
   | 3 | END                           |         |
   +---+-------------------------------+---------+

``NTUPLE`` to ``IPV4``, ``TCP``, ``UDP`` → ``QUEUE``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*NTUPLE* is similar to specifying an empty L2, `Item: IPV4`_ as L3 with
`Item: TCP`_ or `Item: UDP`_ as L4 and a terminating `Action: QUEUE`_.

A priority level can be specified as well.

.. _table_rte_flow_migration_ntuple:

.. table:: NTUPLE conversion

   +-----------------------------+---------+
   | Pattern                     | Actions |
   +===+======+==========+=======+=========+
   | 0 | ETH  | ``spec`` | unset | QUEUE   |
   |   |      +----------+-------+         |
   |   |      | ``last`` | unset |         |
   |   |      +----------+-------+         |
   |   |      | ``mask`` | unset |         |
   +---+------+----------+-------+---------+
   | 1 | IPV4 | ``spec`` | any   | END     |
   |   |      +----------+-------+         |
   |   |      | ``last`` | unset |         |
   |   |      +----------+-------+         |
   |   |      | ``mask`` | any   |         |
   +---+------+----------+-------+         |
   | 2 | TCP, | ``spec`` | any   |         |
   |   | UDP  +----------+-------+         |
   |   |      | ``last`` | unset |         |
   |   |      +----------+-------+         |
   |   |      | ``mask`` | any   |         |
   +---+------+----------+-------+         |
   | 3 | END                     |         |
   +---+-------------------------+---------+

``TUNNEL`` to ``ETH``, ``IPV4``, ``IPV6``, ``VXLAN`` (or other) → ``QUEUE``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*TUNNEL* matches common IPv4 and IPv6 L3/L4-based tunnel types.

In the following table, `Item: ANY`_ is used to cover the optional L4.

.. _table_rte_flow_migration_tunnel:

.. table:: TUNNEL conversion

   +-------------------------------------------------------+---------+
   | Pattern                                               | Actions |
   +===+==========================+==========+=============+=========+
   | 0 | ETH                      | ``spec`` | any         | QUEUE   |
   |   |                          +----------+-------------+         |
   |   |                          | ``last`` | unset       |         |
   |   |                          +----------+-------------+         |
   |   |                          | ``mask`` | any         |         |
   +---+--------------------------+----------+-------------+---------+
   | 1 | IPV4, IPV6               | ``spec`` | any         | END     |
   |   |                          +----------+-------------+         |
   |   |                          | ``last`` | unset       |         |
   |   |                          +----------+-------------+         |
   |   |                          | ``mask`` | any         |         |
   +---+--------------------------+----------+-------------+         |
   | 2 | ANY                      | ``spec`` | any         |         |
   |   |                          +----------+-------------+         |
   |   |                          | ``last`` | unset       |         |
   |   |                          +----------+---------+---+         |
   |   |                          | ``mask`` | ``num`` | 0 |         |
   +---+--------------------------+----------+---------+---+         |
   | 3 | VXLAN, GENEVE, TEREDO,   | ``spec`` | any         |         |
   |   | NVGRE, GRE, ...          +----------+-------------+         |
   |   |                          | ``last`` | unset       |         |
   |   |                          +----------+-------------+         |
   |   |                          | ``mask`` | any         |         |
   +---+--------------------------+----------+-------------+         |
   | 4 | END                                               |         |
   +---+---------------------------------------------------+---------+

``FDIR`` to most item types → ``QUEUE``, ``DROP``, ``PASSTHRU``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*FDIR* is more complex than any other type, there are several methods to
emulate its functionality. It is summarized for the most part in the table
below.

A few features are intentionally not supported:

- The ability to configure the matching input set and masks for the entire
  device, PMDs should take care of it automatically according to the
  requested flow rules.

  For example if a device supports only one bit-mask per protocol type,
  source/address IPv4 bit-masks can be made immutable by the first created
  rule. Subsequent IPv4 or TCPv4 rules can only be created if they are
  compatible.

  Note that only protocol bit-masks affected by existing flow rules are
  immutable, others can be changed later. They become mutable again after
  the related flow rules are destroyed.

- Returning four or eight bytes of matched data when using flex bytes
  filtering. Although a specific action could implement it, it conflicts
  with the much more useful 32 bits tagging on devices that support it.

- Side effects on RSS processing of the entire device. Flow rules that
  conflict with the current device configuration should not be
  allowed. Similarly, device configuration should not be allowed when it
  affects existing flow rules.

- Device modes of operation. "none" is unsupported since filtering cannot be
  disabled as long as a flow rule is present.

- "MAC VLAN" or "tunnel" perfect matching modes should be automatically set
  according to the created flow rules.

- Signature mode of operation is not defined but could be handled through
  "FUZZY" item.

.. _table_rte_flow_migration_fdir:

.. table:: FDIR conversion

   +----------------------------------------+-----------------------+
   | Pattern                                | Actions               |
   +===+===================+==========+=====+=======================+
   | 0 | ETH, RAW          | ``spec`` | any | QUEUE, DROP, PASSTHRU |
   |   |                   +----------+-----+                       |
   |   |                   | ``last`` | N/A |                       |
   |   |                   +----------+-----+                       |
   |   |                   | ``mask`` | any |                       |
   +---+-------------------+----------+-----+-----------------------+
   | 1 | IPV4, IPv6        | ``spec`` | any | MARK                  |
   |   |                   +----------+-----+                       |
   |   |                   | ``last`` | N/A |                       |
   |   |                   +----------+-----+                       |
   |   |                   | ``mask`` | any |                       |
   +---+-------------------+----------+-----+-----------------------+
   | 2 | TCP, UDP, SCTP    | ``spec`` | any | END                   |
   |   |                   +----------+-----+                       |
   |   |                   | ``last`` | N/A |                       |
   |   |                   +----------+-----+                       |
   |   |                   | ``mask`` | any |                       |
   +---+-------------------+----------+-----+                       |
   | 3 | VF, PF, FUZZY     | ``spec`` | any |                       |
   |   | (optional)        +----------+-----+                       |
   |   |                   | ``last`` | N/A |                       |
   |   |                   +----------+-----+                       |
   |   |                   | ``mask`` | any |                       |
   +---+-------------------+----------+-----+                       |
   | 4 | END                                |                       |
   +---+------------------------------------+-----------------------+

``HASH``
~~~~~~~~

There is no counterpart to this filter type because it translates to a
global device setting instead of a pattern item. Device settings are
automatically set according to the created flow rules.

``L2_TUNNEL`` to ``VOID`` → ``VXLAN`` (or others)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All packets are matched. This type alters incoming packets to encapsulate
them in a chosen tunnel type, optionally redirect them to a VF as well.

The destination pool for tag based forwarding can be emulated with other
flow rules using `Action: DUP`_.

.. _table_rte_flow_migration_l2tunnel:

.. table:: L2_TUNNEL conversion

   +---------------------------+--------------------+
   | Pattern                   | Actions            |
   +===+======+==========+=====+====================+
   | 0 | VOID | ``spec`` | N/A | VXLAN, GENEVE, ... |
   |   |      |          |     |                    |
   |   |      |          |     |                    |
   |   |      +----------+-----+                    |
   |   |      | ``last`` | N/A |                    |
   |   |      +----------+-----+                    |
   |   |      | ``mask`` | N/A |                    |
   |   |      |          |     |                    |
   +---+------+----------+-----+--------------------+
   | 1 | END                   | VF (optional)      |
   +---+                       +--------------------+
   | 2 |                       | END                |
   +---+-----------------------+--------------------+
