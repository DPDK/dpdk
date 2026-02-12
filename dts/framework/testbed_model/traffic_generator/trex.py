# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""Implementation for TRex performance traffic generator."""

import ast
import time
from dataclasses import dataclass, field
from enum import auto
from typing import ClassVar

from scapy.packet import Packet

from framework.config.node import OS, NodeConfiguration
from framework.config.test_run import TrexTrafficGeneratorConfig
from framework.parser import TextParser
from framework.remote_session.blocking_app import BlockingApp
from framework.remote_session.python_shell import PythonShell
from framework.testbed_model.node import Node, create_session
from framework.testbed_model.os_session import OSSession
from framework.testbed_model.topology import Topology
from framework.testbed_model.traffic_generator.performance_traffic_generator import (
    PerformanceTrafficGenerator,
    PerformanceTrafficStats,
)
from framework.utils import StrEnum


@dataclass(slots=True)
class TrexPerformanceTrafficStats(PerformanceTrafficStats, TextParser):
    """Data structure to store performance statistics for a given test run.

    This class overrides the initialization of :class:`PerformanceTrafficStats`
    in order to set the attribute values using the TRex stats output.

    Attributes:
        tx_pps: Recorded tx packets per second.
        tx_bps: Recorded tx bytes per second.
        rx_pps: Recorded rx packets per second.
        rx_bps: Recorded rx bytes per second.
        frame_size: The total length of the frame.
    """

    tx_pps: int = field(metadata=TextParser.find_int(r"total.*'tx_pps': (\d+)"))
    tx_bps: int = field(metadata=TextParser.find_int(r"total.*'tx_bps': (\d+)"))
    rx_pps: int = field(metadata=TextParser.find_int(r"total.*'rx_pps': (\d+)"))
    rx_bps: int = field(metadata=TextParser.find_int(r"total.*'rx_bps': (\d+)"))


class TrexStatelessTXModes(StrEnum):
    """Flags indicating TRex instance's current transmission mode."""

    #: Transmit continuously
    STLTXCont = auto()
    #: Transmit in a single burst
    STLTXSingleBurst = auto()
    #: Transmit in multiple bursts
    STLTXMultiBurst = auto()


class TrexTrafficGenerator(PerformanceTrafficGenerator):
    """TRex traffic generator.

    This implementation leverages the stateless API library provided in the TRex installation.

    Attributes:
        stl_client_name: The name of the stateless client used in the stateless API.
        packet_stream_name: The name of the stateless packet stream used in the stateless API.
    """

    _os_session: OSSession

    _tg_config: TrexTrafficGeneratorConfig
    _node_config: NodeConfiguration

    _shell: PythonShell
    _python_indentation: ClassVar[str] = " " * 4

    stl_client_name: ClassVar[str] = "client"
    packet_stream_name: ClassVar[str] = "stream"

    _streaming_mode: TrexStatelessTXModes = TrexStatelessTXModes.STLTXCont

    _tg_cores: int = 10

    _trex_app: BlockingApp

    def __init__(self, tg_node: Node, config: TrexTrafficGeneratorConfig) -> None:
        """Initialize the TRex server.

        Initializes needed OS sessions for the creation of the TRex server process.

        Args:
            tg_node: TG node the TRex instance is operating on.
            config: Traffic generator config provided for TRex instance.
        """
        assert (
            tg_node.config.os == OS.linux
        ), "Linux is the only supported OS for trex traffic generation"

        super().__init__(tg_node=tg_node, config=config)
        self._tg_node_config = tg_node.config
        self._tg_config = config

        self._os_session = create_session(self._tg_node.config, "TRex", self._logger)

    def setup(self, topology: Topology):
        """Initialize and start a TRex server process."""
        super().setup(topology)

        self._shell = PythonShell(self._tg_node, "TRex-client", privileged=True)

        # Start TRex server process.
        trex_app_path = f"cd {self._tg_config.remote_path} && ./t-rex-64"
        self._trex_app = BlockingApp(
            node=self._tg_node,
            path=trex_app_path,
            name="trex-tg",
            privileged=True,
            app_params=f"--cfg {self._tg_config.config} -c {self._tg_cores} -i",
            add_to_shell_pool=False,
        )
        self._trex_app.wait_until_ready("-Per port stats table")

        self._shell.start_application()
        self._shell.send_command("import os")
        self._shell.send_command(
            f"os.chdir('{self._tg_config.remote_path}/automation/trex_control_plane/interactive')"
        )

        # Import stateless API components.
        imports = [
            "import trex",
            "import trex.stl",
            "import trex.stl.trex_stl_client",
            "import trex.stl.trex_stl_streams",
            "import trex.stl.trex_stl_packet_builder_scapy",
            "from scapy.layers.l2 import Ether",
            "from scapy.layers.inet import IP",
            "from scapy.packet import Raw",
        ]
        self._shell.send_command("\n".join(imports))

        stateless_client = [
            f"{self.stl_client_name} = trex.stl.trex_stl_client.STLClient(",
            f"username='{self._tg_node_config.user}',",
            "server='127.0.0.1',",
            ")",
        ]

        self._shell.send_command(f"\n{self._python_indentation}".join(stateless_client))
        self._shell.send_command(f"{self.stl_client_name}.connect()")

    def calculate_traffic_and_stats(
        self,
        packet: Packet,
        duration: float,
        send_mpps: int | None = None,
    ) -> PerformanceTrafficStats:
        """Send packet traffic and acquire associated statistics.

        Overrides
        :meth:`~.traffic_generator.PerformanceTrafficGenerator.calculate_traffic_and_stats`.
        """
        trex_stats_output = ast.literal_eval(self._generate_traffic(packet, duration, send_mpps))
        stats = TrexPerformanceTrafficStats.parse(str(trex_stats_output))
        stats.frame_size = len(packet)
        return stats

    def _generate_traffic(
        self, packet: Packet, duration: float, send_mpps: int | None = None
    ) -> str:
        """Generate traffic using provided packet.

        Uses the provided packet to generate traffic for the provided duration.

        Args:
            packet: The packet being used for the performance test.
            duration: The duration of the test being performed.
            send_mpps: MPPS send rate.

        Returns:
            A string output of statistics provided by the traffic generator.
        """
        self._create_packet_stream(packet)
        self._setup_trex_client()

        stats = self._send_traffic_and_get_stats(duration, send_mpps)

        return stats

    def _setup_trex_client(self) -> None:
        """Create trex client and connect to the server process."""
        # Prepare TRex client for next performance test.
        procedure = [
            f"{self.stl_client_name}.connect()",
            f"{self.stl_client_name}.reset(ports = [0, 1])",
            f"{self.stl_client_name}.clear_stats()",
            f"{self.stl_client_name}.add_streams({self.packet_stream_name}, ports=[0, 1])",
        ]

        for command in procedure:
            self._shell.send_command(command)

    def _create_packet_stream(self, packet: Packet) -> None:
        """Create TRex packet stream with the given packet.

        Args:
            packet: The packet being used for the performance test.
        """
        # Create the tx packet on the TG shell
        self._shell.send_command(f"packet={packet.command()}")

        packet_stream = [
            f"{self.packet_stream_name} = trex.stl.trex_stl_streams.STLStream(",
            f"name='Test_{len(packet)}_bytes',",
            "packet=trex.stl.trex_stl_packet_builder_scapy.STLPktBuilder(pkt=packet),",
            f"mode=trex.stl.trex_stl_streams.{self._streaming_mode}(percentage=100),",
            ")",
        ]
        self._shell.send_command("\n".join(packet_stream))

    def _send_traffic_and_get_stats(self, duration: float, send_mpps: float | None = None) -> str:
        """Send traffic and get TG Rx stats.

        Sends traffic from the TRex client's ports for the given duration.
        When the traffic sending duration has passed, collect the aggregate
        statistics and return TRex's global stats as a string.

        Args:
            duration: The traffic generation duration.
            send_mpps: The millions of packets per second for TRex to send from each port.
        """
        if send_mpps:
            self._shell.send_command(f"""{self.stl_client_name}.start(ports=[0, 1],
                mult = '{send_mpps}mpps',
                duration = {duration})""")
        else:
            self._shell.send_command(f"""{self.stl_client_name}.start(ports=[0, 1],
                mult = '100%',
                duration = {duration})""")

        time.sleep(duration)

        stats = self._shell.send_command(
            f"{self.stl_client_name}.get_stats(ports=[0, 1])", skip_first_line=True
        )

        self._shell.send_command(f"{self.stl_client_name}.stop(ports=[0, 1])")

        return stats

    def close(self) -> None:
        """Overrides :meth:`.traffic_generator.TrafficGenerator.close`.

        Stops the traffic generator and sniffer shells.
        """
        self._trex_app.close()
        self._shell.close()
