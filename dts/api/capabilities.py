# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2024 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""Testbed capabilities.

This module provides a protocol that defines the common attributes of test cases and suites
and support for test environment capabilities.

Many test cases are testing features not available on all hardware.
On the other hand, some test cases or suites may not need the most complex topology available.

The module allows developers to mark test cases or suites to require certain hardware capabilities
or a particular topology.

There are differences between hardware and topology capabilities:

    * Hardware capabilities are assumed to not be required when not specified.
    * However, some topology is always available, so each test case or suite is assigned
      a default topology if no topology is specified in the decorator.

Examples:
    .. code:: python

        from framework.test_suite import TestSuite, func_test
        from framework.testbed_model.capability import LinkTopology, requires_link_topology
        # The whole test suite (each test case within) doesn't require any links.
        @requires_link_topology(LinkTopology.NO_LINK)
        @func_test
        class TestHelloWorld(TestSuite):
            def hello_world_single_core(self):
            ...

    .. code:: python

        from framework.test_suite import TestSuite, func_test
        from framework.testbed_model.capability import NicCapability, requires_nic_capability
        class TestPmdBufferScatter(TestSuite):
            # only the test case requires the SCATTERED_RX_ENABLED capability
            # other test cases may not require it
            @requires_nic_capability(NicCapability.SCATTERED_RX_ENABLED)
            @func_test
            def test_scatter_mbuf_2048(self):
"""

from enum import IntEnum, auto
from typing import TYPE_CHECKING, Callable

if TYPE_CHECKING:
    from framework.test_suite import TestProtocol


class LinkTopology(IntEnum):
    """Supported topology types."""

    #: A topology with no Traffic Generator.
    NO_LINK = 0
    #: A topology with one physical link between the SUT node and the TG node.
    ONE_LINK = auto()
    #: A topology with two physical links between the Sut node and the TG node.
    TWO_LINKS = auto()

    @classmethod
    def default(cls) -> "LinkTopology":
        """The default topology required by test cases if not specified otherwise."""
        return cls.TWO_LINKS


class NicCapability(IntEnum):
    """DPDK NIC capabilities.

    The capabilities are used to mark test cases or suites that require a specific
    DPDK NIC capability to run. The capabilities are used by the test framework to
    determine whether a test case or suite can be run on the current testbed.
    """

    #: Scattered packets Rx enabled.
    SCATTERED_RX_ENABLED = 0
    #: Device supports VLAN stripping.
    RX_OFFLOAD_VLAN_STRIP = auto()
    #: Device supports L3 checksum offload.
    RX_OFFLOAD_IPV4_CKSUM = auto()
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_UDP_CKSUM = auto()
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_TCP_CKSUM = auto()
    #: Device supports Large Receive Offload.
    RX_OFFLOAD_TCP_LRO = auto()
    #: Device supports QinQ (queue in queue) offload.
    RX_OFFLOAD_QINQ_STRIP = auto()
    #: Device supports inner packet L3 checksum.
    RX_OFFLOAD_OUTER_IPV4_CKSUM = auto()
    #: Device supports MACsec.
    RX_OFFLOAD_MACSEC_STRIP = auto()
    #: Device supports filtering of a VLAN Tag identifier.
    RX_OFFLOAD_VLAN_FILTER = auto()
    #: Device supports VLAN offload.
    RX_OFFLOAD_VLAN_EXTEND = auto()
    #: Device supports receiving segmented mbufs.
    RX_OFFLOAD_SCATTER = auto()
    #: Device supports Timestamp.
    RX_OFFLOAD_TIMESTAMP = auto()
    #: Device supports crypto processing while packet is received in NIC.
    RX_OFFLOAD_SECURITY = auto()
    #: Device supports CRC stripping.
    RX_OFFLOAD_KEEP_CRC = auto()
    #: Device supports L4 checksum offload.
    RX_OFFLOAD_SCTP_CKSUM = auto()
    #: Device supports inner packet L4 checksum.
    RX_OFFLOAD_OUTER_UDP_CKSUM = auto()
    #: Device supports RSS hashing.
    RX_OFFLOAD_RSS_HASH = auto()
    #: Device supports scatter Rx packets to segmented mbufs.
    RX_OFFLOAD_BUFFER_SPLIT = auto()
    #: Device supports all checksum capabilities.
    RX_OFFLOAD_CHECKSUM = auto()
    #: Device supports all VLAN capabilities.
    RX_OFFLOAD_VLAN = auto()
    #: Device supports Rx queue setup after device started.
    RUNTIME_RX_QUEUE_SETUP = auto()
    #: Device supports Tx queue setup after device started.
    RUNTIME_TX_QUEUE_SETUP = auto()
    #: Device supports shared Rx queue among ports within Rx domain and switch domain.
    RXQ_SHARE = auto()
    #: Device supports keeping flow rules across restart.
    FLOW_RULE_KEEP = auto()
    #: Device supports keeping shared flow objects across restart.
    FLOW_SHARED_OBJECT_KEEP = auto()
    #: Device supports multicast address filtering.
    MCAST_FILTERING = auto()
    #: Device supports flow ctrl.
    FLOW_CTRL = auto()
    #: Device is running on a physical function.
    PHYSICAL_FUNCTION = auto()


def requires_link_topology(
    link_topology: LinkTopology,
) -> Callable[[type["TestProtocol"]], type["TestProtocol"]]:
    """Decorator to set required topology type for a test case or test suite.

    Args:
        link_topology: The topology type the test suite or case requires.

    Returns:
        The decorated test case or test suite.
    """
    from framework.testbed_model.capability import TopologyCapability

    def add_required_topology(
        test_case_or_suite: type["TestProtocol"],
    ) -> type["TestProtocol"]:
        topology_capability = TopologyCapability.get_unique(link_topology)
        topology_capability.set_required(test_case_or_suite)
        return test_case_or_suite

    return add_required_topology


def requires_nic_capability(
    nic_capability: NicCapability,
) -> Callable[[type["TestProtocol"]], type["TestProtocol"]]:
    """Decorator to add a single required NIC capability to a test case or test suite.

    Args:
        nic_capability: The NIC capability that is required by the test case or test suite.

    Returns:
        The decorated test case or test suite.
    """
    from framework.testbed_model.capability import DecoratedNicCapability

    def add_required_capability(
        test_case_or_suite: type["TestProtocol"],
    ) -> type["TestProtocol"]:
        decorated = DecoratedNicCapability.get_unique(nic_capability)
        decorated.add_to_required(test_case_or_suite)
        return test_case_or_suite

    return add_required_capability
