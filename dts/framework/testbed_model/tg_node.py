# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Traffic generator node.

A traffic generator (TG) generates traffic that's sent towards the SUT node.
A TG node is where the TG runs.
"""

from scapy.packet import Packet  # type: ignore[import]

from framework.config import TGNodeConfiguration

from .node import Node
from .port import Port
from .traffic_generator import CapturingTrafficGenerator, create_traffic_generator


class TGNode(Node):
    """The traffic generator node.

    The TG node extends :class:`Node` with TG specific features:

        * Traffic generator initialization,
        * The sending of traffic and receiving packets,
        * The sending of traffic without receiving packets.

    Not all traffic generators are capable of capturing traffic, which is why there
    must be a way to send traffic without that.

    Attributes:
        traffic_generator: The traffic generator running on the node.
    """

    traffic_generator: CapturingTrafficGenerator

    def __init__(self, node_config: TGNodeConfiguration):
        """Extend the constructor with TG node specifics.

        Initialize the traffic generator on the TG node.

        Args:
            node_config: The TG node's test run configuration.
        """
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
        """Send `packet`, return received traffic.

        Send `packet` on `send_port` and then return all traffic captured
        on `receive_port` for the given duration. Also record the captured traffic
        in a pcap file.

        Args:
            packet: The packet to send.
            send_port: The egress port on the TG node.
            receive_port: The ingress port in the TG node.
            duration: Capture traffic for this amount of time after sending `packet`.

        Returns:
             A list of received packets. May be empty if no packets are captured.
        """
        return self.traffic_generator.send_packet_and_capture(
            packet, send_port, receive_port, duration
        )

    def close(self) -> None:
        """Free all resources used by the node.

        This extends the superclass method with TG cleanup.
        """
        self.traffic_generator.close()
        super(TGNode, self).close()
