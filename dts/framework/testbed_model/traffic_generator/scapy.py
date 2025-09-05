# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2025 Arm Limited

"""The Scapy traffic generator.

A traffic generator used for functional testing, implemented with
`the Scapy library <https://scapy.readthedocs.io/en/latest/>`_.
The traffic generator uses an interactive shell to run Scapy on the remote TG node.

The traffic generator extends :class:`framework.remote_session.python_shell.PythonShell` to
implement the methods for handling packets by sending commands into the interactive shell.
"""

from collections.abc import Callable
from queue import Empty, SimpleQueue
from threading import Event, Thread
from typing import Any, ClassVar

from scapy.compat import base64_bytes
from scapy.data import ETHER_TYPES, IP_PROTOS
from scapy.layers.inet import IP
from scapy.layers.inet6 import IPv6
from scapy.layers.l2 import Ether
from scapy.packet import Packet

from framework.config.node import OS
from framework.config.test_run import ScapyTrafficGeneratorConfig
from framework.exception import InteractiveSSHSessionDeadError, InternalError
from framework.remote_session.python_shell import PythonShell
from framework.testbed_model.node import Node
from framework.testbed_model.port import Port
from framework.testbed_model.topology import Topology
from framework.testbed_model.traffic_generator.capturing_traffic_generator import (
    PacketFilteringConfig,
)

from .capturing_traffic_generator import CapturingTrafficGenerator


class ScapyAsyncSniffer(PythonShell):
    """Asynchronous Scapy sniffer class.

    Starts its own dedicated :class:`PythonShell` to constantly sniff packets asynchronously to
    minimize delays between runs. This is achieved using the synchronous `sniff` Scapy function,
    which prints one packet per line in Base 64 notation. This class spawns a thread to constantly
    read the stdout for packets. Packets are only parsed and captured, i.e. placed on a thread-safe
    queue, when the `_is_capturing` event is set.
    """

    _sniffer: Thread
    _is_sniffing: Event
    _is_capturing: Event
    _packets: SimpleQueue[Packet]
    _packet_filter: Callable[[Packet], bool] | None

    def __init__(
        self, node: Node, recv_port: Port, name: str | None = None, privileged: bool = True
    ) -> None:
        """Sniffer constructor.

        Args:
            node: Node to start the sniffer on.
            recv_port: Port to sniff packets from.
            name: Name identifying the sniffer.
            privileged: Enables the shell to run as superuser.
        """
        super().__init__(node, name, privileged)
        self._sniffer = Thread(target=self._sniff, args=(recv_port,))
        self._is_sniffing = Event()
        self._is_capturing = Event()
        self._packets = SimpleQueue()
        self._packet_filter = None

    def start_capturing(self, filter_config: PacketFilteringConfig) -> None:
        """Start packet capturing.

        Args:
            filter_config: The packet filtering configuration.

        Raises:
            InternalError: If the sniffer is already capturing packets.
        """
        if self._is_capturing.is_set():
            raise InternalError("Already capturing. Did you intend to do this?")
        self._set_packet_filter(filter_config)
        self._is_capturing.set()

    def collect(
        self, stop_condition: Callable[[Packet], bool] | None = None, timeout: float = 1.0
    ) -> list[Packet]:
        """Collect packets until timeout or stop condition is met.

        A `stop_condition` callback can be passed to trigger a capture stop as soon as the last
        desired packet has been captured. Without a stop condition, the specified `timeout` is  used
        to determine when to stop.

        Args:
            stop_condition: Callback which decides when to stop capturing packets.
            timeout: Time to wait after the last captured packet before stopping.

        Raises:
            InternalError: If the sniffer is not capturing any packets.
            TimeoutError: If a `stop_condition` has been set and was not met before timeout.

        Returns:
            A list of the captured packets.
        """
        if not self._is_capturing.is_set():
            raise InternalError("Already not capturing. Did you intend to do this?")

        collected_packets = []
        try:
            while packet := self._packets.get(timeout=timeout):
                collected_packets.append(packet)
                if stop_condition is not None and stop_condition(packet):
                    break
        except Empty:
            if stop_condition is not None:
                msg = "The stop condition was not met before timeout."
                raise TimeoutError(msg)

        return collected_packets

    def stop_capturing(self) -> None:
        """Stop packet capturing.

        It also drains the internal queue from uncollected packets.

        Raises:
            InternalError: If the sniffer is not capturing any packets.
        """
        if not self._is_capturing.is_set():
            raise InternalError("Already not capturing. Did you intend to do this?")

        self._is_capturing.clear()

        while True:
            try:
                self._packets.get_nowait()
            except Empty:
                break

    def stop_capturing_and_collect(
        self, stop_condition: Callable[[Packet], bool] | None = None, timeout: float = 1.0
    ) -> list[Packet]:
        """Stop packet capturing and collect all the captured packets in a list.

        A `stop_condition` callback can be passed to trigger a capture stop as soon as the last
        desired packet has been captured. Without a stop condition, the specified `timeout` is  used
        to determine when to stop.

        Args:
            stop_condition: Callback which decides when to stop capturing packets.
            timeout: Time to wait after the last captured packet before stopping.

        Raises:
            InternalError: If the sniffer is not capturing any packets.
            TimeoutError: If a `stop_condition` has been set and was not met before timeout.

        Returns:
            A list of the captured packets.
        """
        if not self._is_capturing.is_set():
            raise InternalError("Already not capturing. Did you intend to do this?")

        try:
            return self.collect(stop_condition, timeout)
        finally:
            self.stop_capturing()

    def start_application(self, prompt: str | None = None) -> None:
        """Overrides :meth:`framework.remote_session.interactive_shell.start_application`.

        Prepares the Python shell for scapy and starts the sniffing in a new thread.
        """
        super().start_application(prompt)
        self.send_command("from scapy.all import *")
        self._sniffer.start()
        self._is_sniffing.wait()

    def close(self) -> None:
        """Overrides :meth:`framework.remote_session.interactive_shell.start_application`.

        Sends a stop signal to the sniffer thread and waits until its exit before closing the shell.
        """
        self._is_sniffing.clear()
        self._sniffer.join()
        super().close()

    def _sniff(self, recv_port: Port) -> None:
        """Sniff packets and use events and queue to communicate with the main thread.

        Raises:
            InteractiveSSHSessionDeadError: If the SSH connection has been unexpectedly interrupted.
        """
        ready_prompt = "Ready."
        self.send_command(
            "sniff("
            f'iface="{recv_port.logical_name}", quiet=True, store=False, '
            "prn=lambda p: bytes_base64(p.build()).decode(), "
            f'started_callback=lambda: print("{ready_prompt}")'
            ")",
            prompt=ready_prompt,
        )
        self._ssh_channel.settimeout(1)

        self._logger.debug("Start sniffing.")
        self._is_sniffing.set()
        while self._is_sniffing.is_set():
            try:
                line = self._stdout.readline()
                if not line:
                    raise InteractiveSSHSessionDeadError(
                        self._node.main_session.interactive_session.hostname
                    )

                if self._is_capturing.is_set():
                    packet = Ether(base64_bytes(line.rstrip()))
                    if self._packet_filter is None or self._packet_filter(packet):
                        self._logger.debug(f"CAPTURING sniffed packet: {repr(packet)}")
                        self._packets.put(packet)
                    else:
                        self._logger.debug(f"DROPPING sniffed packet: {repr(packet)}")
            except TimeoutError:
                pass

        self._logger.debug("Stop sniffing.")
        self.send_command("\x03")  # send Ctrl+C to trigger a KeyboardInterrupt in `sniff`.

    def _set_packet_filter(self, filter_config: PacketFilteringConfig) -> None:
        """Make and set a filtering function from `filter_config`.

        Args:
            filter_config: Config class that specifies which filters should be applied.
        """

        def _filter(packet: Packet) -> bool:
            if ether := packet.getlayer(Ether):
                if filter_config.no_arp and ether.type == ETHER_TYPES.ARP:
                    return False

                if filter_config.no_lldp and ether.type == ETHER_TYPES.LLDP:
                    return False

            if ipv4 := packet.getlayer(IP):
                if filter_config.no_icmp and ipv4.proto == IP_PROTOS.icmp:
                    return False

            if ipv6 := packet.getlayer(IPv6):
                next_header = ipv6.nh

                if next_header == IP_PROTOS.hopopt:
                    next_header = ipv6.payload.nh

                if filter_config.no_icmp and next_header == IP_PROTOS.ipv6_icmp:
                    return False

            return True

        self._packet_filter = _filter


class ScapyTrafficGenerator(CapturingTrafficGenerator):
    """Provides access to scapy functions on a traffic generator node.

    This class extends the base with remote execution of scapy functions. All methods for
    processing packets are implemented using an underlying
    :class:`framework.remote_session.python_shell.PythonShell` which imports the Scapy library. This
    class also extends :class:`.capturing_traffic_generator.CapturingTrafficGenerator` to expose
    methods that utilize said packet processing functionality to test suites, which are delegated to
    a dedicated asynchronous packet sniffer with :class:`ScapyAsyncSniffer`.

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
    _shell: PythonShell
    _sniffer: ScapyAsyncSniffer

    #: Name of sniffer to ensure the same is used in all places
    _sniffer_name: ClassVar[str] = "scapy_sniffer"
    #: Name of variable that points to the list of packets inside the scapy shell.
    _send_packet_list_name: ClassVar[str] = "packets"
    #: Padding to add to the start of a line for python syntax compliance.
    _python_indentation: ClassVar[str] = " " * 4

    def __init__(self, tg_node: Node, config: ScapyTrafficGeneratorConfig, **kwargs: Any) -> None:
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

        super().__init__(tg_node=tg_node, config=config, **kwargs)

    def setup(self, topology: Topology) -> None:
        """Extends :meth:`.traffic_generator.TrafficGenerator.setup`.

        Binds the TG node ports to the kernel drivers and starts up the async sniffer.
        """
        topology.configure_ports("tg", "kernel")

        self._sniffer = ScapyAsyncSniffer(
            self._tg_node, topology.tg_port_ingress, self._sniffer_name
        )
        self._sniffer.start_application()

        self._shell = PythonShell(self._tg_node, "scapy", privileged=True)
        self._shell.start_application()
        self._shell.send_command("from scapy.all import *")
        self._shell.send_command("from scapy.contrib.lldp import *")

    def close(self) -> None:
        """Overrides :meth:`.traffic_generator.TrafficGenerator.close`.

        Stops the traffic generator and sniffer shells.
        """
        self._shell.close()
        self._sniffer.close()

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
        self._shell.send_command(f"\n{self._python_indentation}".join(send_command))

    def _send_packets_and_capture(
        self,
        packets: list[Packet],
        send_port: Port,
        _: Port,
        filter_config: PacketFilteringConfig,
        duration: float,
    ) -> list[Packet]:
        """Implementation for sending packets and capturing any received traffic.

        Returns:
            A list of packets received after sending `packets`.
        """
        self._sniffer.start_capturing(filter_config)
        self.send_packets(packets, send_port)
        return self._sniffer.stop_capturing_and_collect(timeout=duration)

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
        self._shell.send_command(
            f"{self._send_packet_list_name} = [{', '.join(map(Packet.command, packets))}]"
        )
