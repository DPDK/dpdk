# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Traffic generator node.

This is the node where the traffic generator resides.
The distinction between a node and a traffic generator is as follows:
A node is a host that DTS connects to. It could be a baremetal server,
a VM or a container.
A traffic generator is software running on the node.
A traffic generator node is a node running a traffic generator.
A node can be a traffic generator node as well as system under test node.
"""

from scapy.packet import Packet  # type: ignore[import]

from framework.config import (
    ScapyTrafficGeneratorConfig,
    TGNodeConfiguration,
    TrafficGeneratorType,
)
from framework.exception import ConfigurationError

from .capturing_traffic_generator import CapturingTrafficGenerator
from .hw.port import Port
from .node import Node


class TGNode(Node):
    """Manage connections to a node with a traffic generator.

    Apart from basic node management capabilities, the Traffic Generator node has
    specialized methods for handling the traffic generator running on it.

    Arguments:
        node_config: The user configuration of the traffic generator node.

    Attributes:
        traffic_generator: The traffic generator running on the node.
    """

    traffic_generator: CapturingTrafficGenerator

    def __init__(self, node_config: TGNodeConfiguration):
        super(TGNode, self).__init__(node_config)
        self.traffic_generator = create_traffic_generator(self, node_config.traffic_generator)
        self._logger.info(f"Created node: {self.name}")

    def send_packet_and_capture(
        self,
        packet: Packet,
        send_port: Port,
        receive_port: Port,
        duration: float = 1,
    ) -> list[Packet]:
        """Send a packet, return received traffic.

        Send a packet on the send_port and then return all traffic captured
        on the receive_port for the given duration. Also record the captured traffic
        in a pcap file.

        Args:
            packet: The packet to send.
            send_port: The egress port on the TG node.
            receive_port: The ingress port in the TG node.
            duration: Capture traffic for this amount of time after sending the packet.

        Returns:
             A list of received packets. May be empty if no packets are captured.
        """
        return self.traffic_generator.send_packet_and_capture(
            packet, send_port, receive_port, duration
        )

    def close(self) -> None:
        """Free all resources used by the node"""
        self.traffic_generator.close()
        super(TGNode, self).close()


def create_traffic_generator(
    tg_node: TGNode, traffic_generator_config: ScapyTrafficGeneratorConfig
) -> CapturingTrafficGenerator:
    """A factory function for creating traffic generator object from user config."""

    from .scapy import ScapyTrafficGenerator

    match traffic_generator_config.traffic_generator_type:
        case TrafficGeneratorType.SCAPY:
            return ScapyTrafficGenerator(tg_node, traffic_generator_config)
        case _:
            raise ConfigurationError(
                f"Unknown traffic generator: {traffic_generator_config.traffic_generator_type}"
            )
