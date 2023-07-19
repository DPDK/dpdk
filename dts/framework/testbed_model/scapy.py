# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Scapy traffic generator.

Traffic generator used for functional testing, implemented using the Scapy library.
The traffic generator uses an XML-RPC server to run Scapy on the remote TG node.

The XML-RPC server runs in an interactive remote SSH session running Python console,
where we start the server. The communication with the server is facilitated with
a local server proxy.
"""

from scapy.packet import Packet  # type: ignore[import]

from framework.config import OS, ScapyTrafficGeneratorConfig
from framework.logger import getLogger

from .capturing_traffic_generator import (
    CapturingTrafficGenerator,
    _get_default_capture_name,
)
from .hw.port import Port
from .tg_node import TGNode


class ScapyTrafficGenerator(CapturingTrafficGenerator):
    """Provides access to scapy functions via an RPC interface.

    The traffic generator first starts an XML-RPC on the remote TG node.
    Then it populates the server with functions which use the Scapy library
    to send/receive traffic.

    Any packets sent to the remote server are first converted to bytes.
    They are received as xmlrpc.client.Binary objects on the server side.
    When the server sends the packets back, they are also received as
    xmlrpc.client.Binary object on the client side, are converted back to Scapy
    packets and only then returned from the methods.

    Arguments:
        tg_node: The node where the traffic generator resides.
        config: The user configuration of the traffic generator.
    """

    _config: ScapyTrafficGeneratorConfig
    _tg_node: TGNode

    def __init__(self, tg_node: TGNode, config: ScapyTrafficGeneratorConfig):
        self._config = config
        self._tg_node = tg_node
        self._logger = getLogger(
            f"{self._tg_node.name} {self._config.traffic_generator_type}"
        )

        assert (
            self._tg_node.config.os == OS.linux
        ), "Linux is the only supported OS for scapy traffic generation"

    def _send_packets(self, packets: list[Packet], port: Port) -> None:
        raise NotImplementedError()

    def _send_packets_and_capture(
        self,
        packets: list[Packet],
        send_port: Port,
        receive_port: Port,
        duration: float,
        capture_name: str = _get_default_capture_name(),
    ) -> list[Packet]:
        raise NotImplementedError()

    def close(self):
        pass
