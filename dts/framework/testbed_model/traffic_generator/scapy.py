# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""The Scapy traffic generator.

A traffic generator used for functional testing, implemented with
`the Scapy library <https://scapy.readthedocs.io/en/latest/>`_.
The traffic generator uses an interactive shell to run Scapy on the remote TG node.

The traffic generator extends :class:`framework.remote_session.python_shell.PythonShell` to
implement the methods for handling packets by sending commands into the interactive shell.
"""

import re
import time
from typing import ClassVar

from scapy.compat import base64_bytes
from scapy.layers.l2 import Ether
from scapy.packet import Packet

from framework.config.node import OS, ScapyTrafficGeneratorConfig
from framework.remote_session.python_shell import PythonShell
from framework.testbed_model.node import Node
from framework.testbed_model.port import Port
from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
    PacketFilteringConfig,
)
from framework.utils import REGEX_FOR_BASE64_ENCODING

from .capturing_traffic_generator import CapturingTrafficGenerator


class ScapyTrafficGenerator(PythonShell, CapturingTrafficGenerator):
    """Provides access to scapy functions on a traffic generator node.

    This class extends the base with remote execution of scapy functions. All methods for
    processing packets are implemented using an underlying
    :class:`framework.remote_session.python_shell.PythonShell` which imports the Scapy library. This
    class also extends :class:`.capturing_traffic_generator.CapturingTrafficGenerator` to expose
    methods that utilize said packet processing functionality to test suites.

    Because of the double inheritance, this class has both methods that wrap scapy commands
    sent into the shell (running on the TG node) and methods that run locally to fulfill
    traffic generation needs.
    To help make a clear distinction between the two, the names of the methods
    that wrap the logic of the underlying shell should be prepended with "shell".

    Note that the order of inheritance is important for this class. In order to instantiate this
    class, the abstract methods of :class:`~.capturing_traffic_generator.CapturingTrafficGenerator`
    must be implemented. Since some of these methods are implemented in the underlying interactive
    shell, according to Python's Method Resolution Order (MRO), the interactive shell must come
    first.
    """

    _config: ScapyTrafficGeneratorConfig

    #: Name of sniffer to ensure the same is used in all places
    _sniffer_name: ClassVar[str] = "sniffer"
    #: Name of variable that points to the list of packets inside the scapy shell.
    _send_packet_list_name: ClassVar[str] = "packets"
    #: Padding to add to the start of a line for python syntax compliance.
    _python_indentation: ClassVar[str] = " " * 4

    def __init__(self, tg_node: Node, config: ScapyTrafficGeneratorConfig, **kwargs):
        """Extend the constructor with Scapy TG specifics.

        Initializes both the traffic generator and the interactive shell used to handle Scapy
        functions. The interactive shell will be started on `tg_node`. The additional keyword
        arguments in `kwargs` are used to pass into the constructor for the interactive shell.

        Args:
            tg_node: The node where the traffic generator resides.
            config: The traffic generator's test run configuration.
            kwargs: Additional keyword arguments. Supported arguments correspond to the parameters
                of :meth:`PythonShell.__init__` in this case.
        """
        assert (
            tg_node.config.os == OS.linux
        ), "Linux is the only supported OS for scapy traffic generation"

        super().__init__(node=tg_node, config=config, tg_node=tg_node, **kwargs)
        self.start_application()

    def start_application(self) -> None:
        """Extends :meth:`framework.remote_session.interactive_shell.start_application`.

        Adds a command that imports everything from the scapy library immediately after starting
        the shell for usage in later calls to the methods of this class.
        """
        super().start_application()
        self.send_command("from scapy.all import *")

    def _send_packets(self, packets: list[Packet], port: Port) -> None:
        """Implementation for sending packets without capturing any received traffic.

        Provides a "fire and forget" method of sending packets.
        """
        self._shell_set_packet_list(packets)
        send_command = [
            "sendp(",
            f"{self._send_packet_list_name},",
            f"iface='{port.logical_name}',",
            "realtime=True,",
            "verbose=True",
            ")",
        ]
        self.send_command(f"\n{self._python_indentation}".join(send_command))

    def _send_packets_and_capture(
        self,
        packets: list[Packet],
        send_port: Port,
        recv_port: Port,
        filter_config: PacketFilteringConfig,
        duration: float,
    ) -> list[Packet]:
        """Implementation for sending packets and capturing any received traffic.

        This method first creates an asynchronous sniffer that holds the packets to send, then
        starts and stops said sniffer, collecting any packets that it had received while it was
        running.

        Returns:
            A list of packets received after sending `packets`.
        """
        self._shell_create_sniffer(
            packets, send_port, recv_port, self._create_packet_filter(filter_config)
        )
        return self._shell_start_and_stop_sniffing(duration)

    def _shell_set_packet_list(self, packets: list[Packet]) -> None:
        """Build a list of packets to send later.

        Sends the string that represents the Python command that was used to create each packet in
        `packets` into the underlying Python session. The purpose behind doing this is to create a
        list that is identical to `packets` inside the shell. This method should only be called by
        methods for sending packets immediately prior to sending. The list of packets will continue
        to exist in the scope of the shell until subsequent calls to this method, so failure to
        rebuild the list prior to sending packets could lead to undesired "stale" packets to be
        sent.

        Args:
            packets: The list of packets to recreate in the shell.
        """
        self._logger.info("Building a list of packets to send.")
        self.send_command(
            f"{self._send_packet_list_name} = [{', '.join(map(Packet.command, packets))}]"
        )

    def _create_packet_filter(self, filter_config: PacketFilteringConfig) -> str:
        """Combine filter settings from `filter_config` into a BPF that scapy can use.

        Scapy allows for the use of Berkeley Packet Filters (BPFs) to filter what packets are
        collected based on various attributes of the packet.

        Args:
            filter_config: Config class that specifies which filters should be applied.

        Returns:
            A string representing the combination of BPF filters to be passed to scapy. For
            example:

            "ether[12:2] != 0x88cc && ether[12:2] != 0x0806"
        """
        bpf_filter = []
        if filter_config.no_arp:
            bpf_filter.append("ether[12:2] != 0x0806")
        if filter_config.no_lldp:
            bpf_filter.append("ether[12:2] != 0x88cc")
        return " && ".join(bpf_filter)

    def _shell_create_sniffer(
        self,
        packets_to_send: list[Packet],
        send_port: Port,
        recv_port: Port,
        filter_config: str,
    ) -> None:
        """Create an asynchronous sniffer in the shell.

        A list of packets is passed to the sniffer's callback function so that they are immediately
        sent at the time sniffing is started.

        Args:
            packets_to_send: A list of packets to send when sniffing is started.
            send_port: The port to send the packets on when sniffing is started.
            recv_port: The port to collect the traffic from.
            filter_config: An optional BPF format filter to use when sniffing for packets. Omitted
                when set to an empty string.
        """
        self._shell_set_packet_list(packets_to_send)

        self.send_command("import time")
        sniffer_commands = [
            f"{self._sniffer_name} = AsyncSniffer(",
            f"iface='{recv_port.logical_name}',",
            "store=True,",
            # *args is used in the arguments of the lambda since Scapy sends parameters to the
            # callback function which we do not need for our purposes.
            "started_callback=lambda *args: (time.sleep(1), sendp(",
            (
                # Additional indentation is added to this line only for readability of the logs.
                f"{self._python_indentation}{self._send_packet_list_name},"
                f" iface='{send_port.logical_name}')),"
            ),
            ")",
        ]
        if filter_config:
            sniffer_commands.insert(-1, f"filter='{filter_config}'")

        self.send_command(f"\n{self._python_indentation}".join(sniffer_commands))

    def _shell_start_and_stop_sniffing(self, duration: float) -> list[Packet]:
        """Start asynchronous sniffer, run for a set `duration`, then collect received packets.

        This method expects that you have first created an asynchronous sniffer inside the shell
        and will fail if you haven't. Received packets are collected by printing the base64
        encoding of each packet in the shell and then harvesting these encodings using regex to
        convert back into packet objects.

        Args:
            duration: The amount of time in seconds to sniff for received packets.

        Returns:
            A list of all packets that were received while the sniffer was running.
        """
        sniffed_packets_name = "gathered_packets"
        self.send_command(f"{self._sniffer_name}.start()")
        # Insert a one second delay to prevent timeout errors from occurring
        time.sleep(duration + 1)
        self.send_command(f"{sniffed_packets_name} = {self._sniffer_name}.stop(join=True)")
        # An extra newline is required here due to the nature of interactive Python shells
        packet_strs = self.send_command(
            f"for pakt in {sniffed_packets_name}: print(bytes_base64(pakt.build()))\n"
        )
        # In the string of bytes "b'XXXX'", we only want the contents ("XXXX")
        list_of_packets_base64 = re.findall(
            rf"^b'({REGEX_FOR_BASE64_ENCODING})'", packet_strs, re.MULTILINE
        )
        return [Ether(base64_bytes(pakt)) for pakt in list_of_packets_base64]
