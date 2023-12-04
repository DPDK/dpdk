# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""The Scapy traffic generator.

A traffic generator used for functional testing, implemented with
`the Scapy library <https://scapy.readthedocs.io/en/latest/>`_.
The traffic generator uses an XML-RPC server to run Scapy on the remote TG node.

The traffic generator uses the :mod:`xmlrpc.server` module to run an XML-RPC server
in an interactive remote Python SSH session. The communication with the server is facilitated
with a local server proxy from the :mod:`xmlrpc.client` module.
"""

import inspect
import marshal
import time
import types
import xmlrpc.client
from xmlrpc.server import SimpleXMLRPCServer

import scapy.all  # type: ignore[import]
from scapy.layers.l2 import Ether  # type: ignore[import]
from scapy.packet import Packet  # type: ignore[import]

from framework.config import OS, ScapyTrafficGeneratorConfig
from framework.remote_session import PythonShell
from framework.settings import SETTINGS
from framework.testbed_model.node import Node
from framework.testbed_model.port import Port

from .capturing_traffic_generator import (
    CapturingTrafficGenerator,
    _get_default_capture_name,
)

"""
========= BEGIN RPC FUNCTIONS =========

All of the functions in this section are intended to be exported to a python
shell which runs a scapy RPC server. These functions are made available via that
RPC server to the packet generator. To add a new function to the RPC server,
first write the function in this section. Then, if you need any imports, make sure to
add them to SCAPY_RPC_SERVER_IMPORTS as well. After that, add the function to the list
in EXPORTED_FUNCTIONS. Note that kwargs (keyword arguments) do not work via xmlrpc,
so you may need to construct wrapper functions around many scapy types.
"""

"""
Add the line needed to import something in a normal python environment
as an entry to this array. It will be imported before any functions are
sent to the server.
"""
SCAPY_RPC_SERVER_IMPORTS = [
    "from scapy.all import *",
    "import xmlrpc",
    "import sys",
    "from xmlrpc.server import SimpleXMLRPCServer",
    "import marshal",
    "import pickle",
    "import types",
    "import time",
]


def scapy_send_packets_and_capture(
    xmlrpc_packets: list[xmlrpc.client.Binary],
    send_iface: str,
    recv_iface: str,
    duration: float,
) -> list[bytes]:
    """The RPC function to send and capture packets.

    This function is meant to be executed on the remote TG node via the server proxy.

    Args:
        xmlrpc_packets: The packets to send. These need to be converted to
            :class:`~xmlrpc.client.Binary` objects before sending to the remote server.
        send_iface: The logical name of the egress interface.
        recv_iface: The logical name of the ingress interface.
        duration: Capture for this amount of time, in seconds.

    Returns:
        A list of bytes. Each item in the list represents one packet, which needs
        to be converted back upon transfer from the remote node.
    """
    scapy_packets = [scapy.all.Packet(packet.data) for packet in xmlrpc_packets]
    sniffer = scapy.all.AsyncSniffer(
        iface=recv_iface,
        store=True,
        started_callback=lambda *args: scapy.all.sendp(scapy_packets, iface=send_iface),
    )
    sniffer.start()
    time.sleep(duration)
    return [scapy_packet.build() for scapy_packet in sniffer.stop(join=True)]


def scapy_send_packets(xmlrpc_packets: list[xmlrpc.client.Binary], send_iface: str) -> None:
    """The RPC function to send packets.

    This function is meant to be executed on the remote TG node via the server proxy.
    It only sends `xmlrpc_packets`, without capturing them.

    Args:
        xmlrpc_packets: The packets to send. These need to be converted to
            :class:`~xmlrpc.client.Binary` objects before sending to the remote server.
        send_iface: The logical name of the egress interface.
    """
    scapy_packets = [scapy.all.Packet(packet.data) for packet in xmlrpc_packets]
    scapy.all.sendp(scapy_packets, iface=send_iface, realtime=True, verbose=True)


"""
Functions to be exposed by the scapy RPC server.
"""
RPC_FUNCTIONS = [
    scapy_send_packets,
    scapy_send_packets_and_capture,
]

"""
========= END RPC FUNCTIONS =========
"""


class QuittableXMLRPCServer(SimpleXMLRPCServer):
    """Basic XML-RPC server.

    The server may be augmented by functions serializable by the :mod:`marshal` module.
    """

    def __init__(self, *args, **kwargs):
        """Extend the XML-RPC server initialization.

        Args:
            args: The positional arguments that will be passed to the superclass's constructor.
            kwargs: The keyword arguments that will be passed to the superclass's constructor.
                The `allow_none` argument will be set to :data:`True`.
        """
        kwargs["allow_none"] = True
        super().__init__(*args, **kwargs)
        self.register_introspection_functions()
        self.register_function(self.quit)
        self.register_function(self.add_rpc_function)

    def quit(self) -> None:
        """Quit the server."""
        self._BaseServer__shutdown_request = True
        return None

    def add_rpc_function(self, name: str, function_bytes: xmlrpc.client.Binary) -> None:
        """Add a function to the server from the local server proxy.

        Args:
              name: The name of the function.
              function_bytes: The code of the function.
        """
        function_code = marshal.loads(function_bytes.data)
        function = types.FunctionType(function_code, globals(), name)
        self.register_function(function)

    def serve_forever(self, poll_interval: float = 0.5) -> None:
        """Extend the superclass method with an additional print.

        Once executed in the local server proxy, the print gives us a clear string to expect
        when starting the server. The print means this function was executed on the XML-RPC server.
        """
        print("XMLRPC OK")
        super().serve_forever(poll_interval)


class ScapyTrafficGenerator(CapturingTrafficGenerator):
    """Provides access to scapy functions via an RPC interface.

    This class extends the base with remote execution of scapy functions.

    Any packets sent to the remote server are first converted to bytes. They are received as
    :class:`~xmlrpc.client.Binary` objects on the server side. When the server sends the packets
    back, they are also received as :class:`~xmlrpc.client.Binary` objects on the client side, are
    converted back to :class:`~scapy.packet.Packet` objects and only then returned from the methods.

    Attributes:
        session: The exclusive interactive remote session created by the Scapy
            traffic generator where the XML-RPC server runs.
        rpc_server_proxy: The object used by clients to execute functions
            on the XML-RPC server.
    """

    session: PythonShell
    rpc_server_proxy: xmlrpc.client.ServerProxy
    _config: ScapyTrafficGeneratorConfig

    def __init__(self, tg_node: Node, config: ScapyTrafficGeneratorConfig):
        """Extend the constructor with Scapy TG specifics.

        The traffic generator first starts an XML-RPC on the remote `tg_node`.
        Then it populates the server with functions which use the Scapy library
        to send/receive traffic:

            * :func:`scapy_send_packets_and_capture`
            * :func:`scapy_send_packets`

        To enable verbose logging from the xmlrpc client, use the :option:`--verbose`
        command line argument or the :envvar:`DTS_VERBOSE` environment variable.

        Args:
            tg_node: The node where the traffic generator resides.
            config: The traffic generator's test run configuration.
        """
        super().__init__(tg_node, config)

        assert (
            self._tg_node.config.os == OS.linux
        ), "Linux is the only supported OS for scapy traffic generation"

        self.session = self._tg_node.create_interactive_shell(
            PythonShell, timeout=5, privileged=True
        )

        # import libs in remote python console
        for import_statement in SCAPY_RPC_SERVER_IMPORTS:
            self.session.send_command(import_statement)

        # start the server
        xmlrpc_server_listen_port = 8000
        self._start_xmlrpc_server_in_remote_python(xmlrpc_server_listen_port)

        # connect to the server
        server_url = f"http://{self._tg_node.config.hostname}:{xmlrpc_server_listen_port}"
        self.rpc_server_proxy = xmlrpc.client.ServerProxy(
            server_url, allow_none=True, verbose=SETTINGS.verbose
        )

        # add functions to the server
        for function in RPC_FUNCTIONS:
            # A slightly hacky way to move a function to the remote server.
            # It is constructed from the name and code on the other side.
            # Pickle cannot handle functions, nor can any of the other serialization
            # frameworks aside from the libraries used to generate pyc files, which
            # are even more messy to work with.
            function_bytes = marshal.dumps(function.__code__)
            self.rpc_server_proxy.add_rpc_function(function.__name__, function_bytes)

    def _start_xmlrpc_server_in_remote_python(self, listen_port: int) -> None:
        # load the source of the function
        src = inspect.getsource(QuittableXMLRPCServer)
        # Lines with only whitespace break the repl if in the middle of a function
        # or class, so strip all lines containing only whitespace
        src = "\n".join([line for line in src.splitlines() if not line.isspace() and line != ""])

        # execute it in the python terminal
        self.session.send_command(src + "\n")
        self.session.send_command(
            f"server = QuittableXMLRPCServer(('0.0.0.0', {listen_port}));server.serve_forever()",
            "XMLRPC OK",
        )

    def _send_packets(self, packets: list[Packet], port: Port) -> None:
        packets = [packet.build() for packet in packets]
        self.rpc_server_proxy.scapy_send_packets(packets, port.logical_name)

    def _send_packets_and_capture(
        self,
        packets: list[Packet],
        send_port: Port,
        receive_port: Port,
        duration: float,
        capture_name: str = _get_default_capture_name(),
    ) -> list[Packet]:
        binary_packets = [packet.build() for packet in packets]

        xmlrpc_packets: list[
            xmlrpc.client.Binary
        ] = self.rpc_server_proxy.scapy_send_packets_and_capture(
            binary_packets,
            send_port.logical_name,
            receive_port.logical_name,
            duration,
        )  # type: ignore[assignment]

        scapy_packets = [Ether(packet.data) for packet in xmlrpc_packets]
        return scapy_packets

    def close(self) -> None:
        """Close the traffic generator."""
        try:
            self.rpc_server_proxy.quit()
        except ConnectionRefusedError:
            # Because the python instance closes, we get no RPC response.
            # Thus, this error is expected
            pass
        self.session.close()
