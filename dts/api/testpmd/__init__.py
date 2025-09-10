# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Testpmd interactive shell.

Typical usage example in a TestSuite::

    testpmd = TestPmd(self.sut_node)
    devices = testpmd.get_devices()
    for device in devices:
        print(device)
    testpmd.close()
"""

import functools
import re
import time
from collections.abc import MutableSet
from enum import Flag
from pathlib import PurePath
from typing import (
    Any,
    Callable,
    ClassVar,
    Concatenate,
    ParamSpec,
    Tuple,
)

from typing_extensions import Unpack

from api.capabilities import LinkTopology, NicCapability
from api.testpmd.config import PortTopology, SimpleForwardingModes, TestPmdParams
from api.testpmd.types import (
    ChecksumOffloadOptions,
    DeviceCapabilitiesFlag,
    FlowRule,
    RxOffloadCapabilities,
    RxOffloadCapability,
    TestPmdDevice,
    TestPmdPort,
    TestPmdPortFlowCtrl,
    TestPmdPortStats,
    TestPmdQueueInfo,
    TestPmdRxqInfo,
    TestPmdVerbosePacket,
    VLANOffloadFlag,
)
from framework.context import get_ctx
from framework.exception import InteractiveCommandExecutionError, InternalError
from framework.params.types import TestPmdParamsDict
from framework.remote_session.dpdk_shell import DPDKShell
from framework.remote_session.interactive_shell import only_active
from framework.settings import SETTINGS

P = ParamSpec("P")
TestPmdMethod = Callable[Concatenate["TestPmd", P], Any]


def _requires_stopped_ports(func: TestPmdMethod) -> TestPmdMethod:
    """Decorator for :class:`TestPmd` commands methods that require stopped ports.

    If the decorated method is called while the ports are started, then these are stopped before
    continuing.

    Args:
        func: The :class:`TestPmd` method to decorate.
    """

    @functools.wraps(func)
    def _wrapper(self: "TestPmd", *args: P.args, **kwargs: P.kwargs) -> Any:
        if self.ports_started:
            self._logger.debug("Ports need to be stopped to continue.")
            self.stop_all_ports()

        return func(self, *args, **kwargs)

    return _wrapper


def _requires_started_ports(func: TestPmdMethod) -> TestPmdMethod:
    """Decorator for :class:`TestPmd` commands methods that require started ports.

    If the decorated method is called while the ports are stopped, then these are started before
    continuing.

    Args:
        func: The :class:`TestPmd` method to decorate.
    """

    @functools.wraps(func)
    def _wrapper(self: "TestPmd", *args: P.args, **kwargs: P.kwargs) -> Any:
        if not self.ports_started:
            self._logger.debug("Ports need to be started to continue.")
            self.start_all_ports()

        return func(self, *args, **kwargs)

    return _wrapper


def _add_remove_mtu(mtu: int = 1500) -> Callable[[TestPmdMethod], TestPmdMethod]:
    """Configure MTU to `mtu` on all ports, run the decorated function, then revert.

    Args:
        mtu: The MTU to configure all ports on.

    Returns:
        The method decorated with setting and reverting MTU.
    """

    def decorator(func: TestPmdMethod) -> TestPmdMethod:
        @functools.wraps(func)
        def wrapper(self: "TestPmd", *args: P.args, **kwargs: P.kwargs) -> Any:
            original_mtu = self.ports[0].mtu
            self.set_port_mtu_all(mtu=mtu, verify=False)
            retval = func(self, *args, **kwargs)
            self.set_port_mtu_all(original_mtu if original_mtu else 1500, verify=False)
            return retval

        return wrapper

    return decorator


class TestPmd(DPDKShell):
    """Testpmd interactive shell.

    The testpmd shell users should never use
    the :meth:`~.interactive_shell.InteractiveShell.send_command` method directly, but rather
    call specialized methods. If there isn't one that satisfies a need, it should be added.

    Attributes:
        ports_started: Indicates whether the ports are started.
    """

    _app_params: TestPmdParams
    _ports: list[TestPmdPort] | None

    #: The testpmd's prompt.
    _default_prompt: ClassVar[str] = "testpmd>"

    #: This forces the prompt to appear after sending a command.
    _command_extra_chars: ClassVar[str] = "\n"

    ports_started: bool

    def __init__(
        self,
        name: str | None = None,
        privileged: bool = True,
        **app_params: Unpack[TestPmdParamsDict],
    ) -> None:
        """Overrides :meth:`~.dpdk_shell.DPDKShell.__init__`. Changes app_params to kwargs."""
        if "port_topology" not in app_params and get_ctx().topology.type is LinkTopology.ONE_LINK:
            app_params["port_topology"] = PortTopology.loop
        super().__init__(name, privileged, app_params=TestPmdParams(**app_params))
        self.ports_started = not self._app_params.disable_device_start
        self._ports = None

    @property
    def path(self) -> PurePath:
        """The path to the testpmd executable."""
        return PurePath("app/dpdk-testpmd")

    @property
    def ports(self) -> list[TestPmdPort]:
        """The ports of the instance.

        This caches the ports returned by :meth:`show_port_info_all`.
        To force an update of port information, execute :meth:`show_port_info_all` or
        :meth:`show_port_info`.

        Returns: The list of known testpmd ports.
        """
        if self._ports is None:
            return self.show_port_info_all()
        return self._ports

    @_requires_started_ports
    def start(self, verify: bool = True) -> None:
        """Start packet forwarding with the current configuration.

        Args:
            verify: If :data:`True` , a second start command will be sent in an attempt to verify
                packet forwarding started as expected.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and forwarding fails to
                start or ports fail to come up.
        """
        self.send_command("start")
        if verify:
            # If forwarding was already started, sending "start" again should tell us
            start_cmd_output = self.send_command("start")
            if "Packet forwarding already started" not in start_cmd_output:
                self._logger.debug(f"Failed to start packet forwarding: \n{start_cmd_output}")
                raise InteractiveCommandExecutionError("Testpmd failed to start packet forwarding.")

    def stop(self, verify: bool = True) -> str:
        """Stop packet forwarding.

        Args:
            verify: If :data:`True` , the output of the stop command is scanned to verify that
                forwarding was stopped successfully or not started. If neither is found, it is
                considered an error.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the command to stop
                forwarding results in an error.

        Returns:
            Output gathered from the stop command and all other preceding logs in the buffer. This
            output is most often used to view forwarding statistics that are displayed when this
            command is sent as well as any verbose packet information that hasn't been consumed
            prior to calling this method.
        """
        stop_cmd_output = self.send_command("stop")
        if verify:
            if (
                "Done." not in stop_cmd_output
                and "Packet forwarding not started" not in stop_cmd_output
            ):
                self._logger.debug(f"Failed to stop packet forwarding: \n{stop_cmd_output}")
                raise InteractiveCommandExecutionError("Testpmd failed to stop packet forwarding.")
        return stop_cmd_output

    def get_devices(self) -> list[TestPmdDevice]:
        """Get a list of device names that are known to testpmd.

        Uses the device info listed in testpmd and then parses the output.

        Returns:
            A list of devices.
        """
        dev_info: str = self.send_command("show device info all")
        dev_list: list[TestPmdDevice] = []
        for line in dev_info.split("\n"):
            if "device name:" in line.lower():
                dev_list.append(TestPmdDevice(line))
        return dev_list

    def wait_link_status_up(self, port_id: int, timeout=SETTINGS.timeout) -> bool:
        """Wait until the link status on the given port is "up".

        Arguments:
            port_id: Port to check the link status on.
            timeout: Time to wait for the link to come up. The default value for this
                argument may be modified using the :option:`--timeout` command-line argument
                or the :envvar:`DTS_TIMEOUT` environment variable.

        Returns:
            Whether the link came up in time or not.
        """
        time_to_stop = time.time() + timeout
        port_info: str = ""
        while time.time() < time_to_stop:
            port_info = self.send_command(f"show port info {port_id}")
            if "Link status: up" in port_info:
                break
            time.sleep(0.5)
        else:
            self._logger.error(f"The link for port {port_id} did not come up in the given timeout.")
        return "Link status: up" in port_info

    def set_forward_mode(self, mode: SimpleForwardingModes, verify: bool = True) -> None:
        """Set packet forwarding mode.

        Args:
            mode: The forwarding mode to use.
            verify: If :data:`True` the output of the command will be scanned in an attempt to
                verify that the forwarding mode was set to `mode` properly.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the forwarding mode
                fails to update.
        """
        set_fwd_output = self.send_command(f"set fwd {mode.value}")
        if verify:
            if f"Set {mode.value} packet forwarding mode" not in set_fwd_output:
                self._logger.debug(f"Failed to set fwd mode to {mode.value}:\n{set_fwd_output}")
                raise InteractiveCommandExecutionError(
                    f"Test pmd failed to set fwd mode to {mode.value}"
                )

    def stop_all_ports(self, verify: bool = True) -> None:
        """Stops all the ports.

        Args:
            verify: If :data:`True`, the output of the command will be checked for a successful
                execution.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the ports were not
                stopped successfully.
        """
        self._logger.debug("Stopping all the ports...")
        output = self.send_command("port stop all")
        if verify and not output.strip().endswith("Done"):
            raise InteractiveCommandExecutionError("Ports were not stopped successfully.")

        self.ports_started = False

    def start_all_ports(self, verify: bool = True) -> None:
        """Starts all the ports.

        Args:
            verify: If :data:`True`, the output of the command will be checked for a successful
                execution.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the ports were not
                started successfully.
        """
        self._logger.debug("Starting all the ports...")
        output = self.send_command("port start all")
        if verify and not output.strip().endswith("Done"):
            raise InteractiveCommandExecutionError("Ports were not started successfully.")

        self.ports_started = True

    @_requires_stopped_ports
    def set_ports_queues(self, number_of: int) -> None:
        """Sets the number of queues per port.

        Args:
            number_of: The number of RX/TX queues to create per port.

        Raises:
            InternalError: If `number_of` is invalid.
        """
        if number_of < 1:
            raise InternalError("The number of queues must be positive and non-zero.")

        self.send_command(f"port config all rxq {number_of}")
        self.send_command(f"port config all txq {number_of}")

    @_requires_stopped_ports
    def close_all_ports(self, verify: bool = True) -> None:
        """Close all ports.

        Args:
            verify: If :data:`True` the output of the close command will be scanned in an attempt
                to verify that all ports were stopped successfully. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and at lease one port
                failed to close.
        """
        port_close_output = self.send_command("port close all")
        if verify:
            num_ports = len(self.ports)
            if not all(f"Port {p_id} is closed" in port_close_output for p_id in range(num_ports)):
                raise InteractiveCommandExecutionError("Ports were not closed successfully.")

    def show_port_info_all(self) -> list[TestPmdPort]:
        """Returns the information of all the ports.

        Returns:
            list[TestPmdPort]: A list containing all the ports information as `TestPmdPort`.
        """
        output = self.send_command("show port info all")

        # Sample output of the "all" command looks like:
        #
        # <start>
        #
        #   ********************* Infos for port 0 *********************
        #   Key: value
        #
        #   ********************* Infos for port 1 *********************
        #   Key: value
        # <end>
        #
        # Takes advantage of the double new line in between ports as end delimiter. But we need to
        # artificially add a new line at the end to pick up the last port. Because commands are
        # executed on a pseudo-terminal created by paramiko on the remote node, lines end with CRLF.
        # Therefore we also need to take the carriage return into account.
        iter = re.finditer(r"\*{21}.*?[\r\n]{4}", output + "\r\n", re.S)
        self._ports = [TestPmdPort.parse(block.group(0)) for block in iter]
        return self._ports

    def show_port_info(self, port_id: int) -> TestPmdPort:
        """Returns the given port information.

        Args:
            port_id: The port ID to gather information for.

        Raises:
            InteractiveCommandExecutionError: If `port_id` is invalid.

        Returns:
            TestPmdPort: An instance of `TestPmdPort` containing the given port's information.
        """
        output = self.send_command(f"show port info {port_id}", skip_first_line=True)
        if output.startswith("Invalid port"):
            raise InteractiveCommandExecutionError("invalid port given")

        port = TestPmdPort.parse(output)
        self._update_port(port)
        return port

    def _update_port(self, port: TestPmdPort) -> None:
        if self._ports:
            self._ports = [
                existing_port if port.id != existing_port.id else port
                for existing_port in self._ports
            ]

    def set_mac_addr(self, port_id: int, mac_address: str, add: bool, verify: bool = True) -> None:
        """Add or remove a mac address on a given port's Allowlist.

        Args:
            port_id: The port ID the mac address is set on.
            mac_address: The mac address to be added to or removed from the specified port.
            add: If :data:`True`, add the specified mac address. If :data:`False`, remove specified
                mac address.
            verify: If :data:'True', assert that the 'mac_addr' operation was successful. If
                :data:'False', run the command and skip this assertion.

        Raises:
            InteractiveCommandExecutionError: If the set mac address operation fails.
        """
        mac_cmd = "add" if add else "remove"
        output = self.send_command(f"mac_addr {mac_cmd} {port_id} {mac_address}")
        if "Bad arguments" in output:
            self._logger.debug("Invalid argument provided to mac_addr")
            raise InteractiveCommandExecutionError("Invalid argument provided")

        if verify:
            if "mac_addr_cmd error:" in output:
                self._logger.debug(f"Failed to {mac_cmd} {mac_address} on port {port_id}")
                raise InteractiveCommandExecutionError(
                    f"Failed to {mac_cmd} {mac_address} on port {port_id} \n{output}"
                )

    def set_multicast_mac_addr(
        self, port_id: int, multi_addr: str, add: bool, verify: bool = True
    ) -> None:
        """Add or remove multicast mac address to a specified port's allow list.

        Args:
            port_id: The port ID the multicast address is set on.
            multi_addr: The multicast address to be added or removed from the filter.
            add: If :data:'True', add the specified multicast address to the port filter.
                If :data:'False', remove the specified multicast address from the port filter.
            verify: If :data:'True', assert that the 'mcast_addr' operations was successful.
                If :data:'False', execute the 'mcast_addr' operation and skip the assertion.

        Raises:
            InteractiveCommandExecutionError: If either the 'add' or 'remove' operations fails.
        """
        mcast_cmd = "add" if add else "remove"
        output = self.send_command(f"mcast_addr {mcast_cmd} {port_id} {multi_addr}")
        if "Bad arguments" in output:
            self._logger.debug("Invalid arguments provided to mcast_addr")
            raise InteractiveCommandExecutionError("Invalid argument provided")

        if verify:
            if (
                "Invalid multicast_addr" in output
                or f"multicast address {'already' if add else 'not'} filtered by port" in output
            ):
                self._logger.debug(f"Failed to {mcast_cmd} {multi_addr} on port {port_id}")
                raise InteractiveCommandExecutionError(
                    f"Failed to {mcast_cmd} {multi_addr} on port {port_id} \n{output}"
                )

    def show_port_stats_all(self) -> Tuple[list[TestPmdPortStats], str]:
        """Returns the statistics of all the ports.

        Returns:
            Tuple[str, list[TestPmdPortStats]]: A tuple where the first element is the stats of all
            ports as `TestPmdPortStats` and second is the raw testpmd output that was collected
            from the sent command.
        """
        output = self.send_command("show port stats all")

        # Sample output of the "all" command looks like:
        #
        #   ########### NIC statistics for port 0 ###########
        #   values...
        #   #################################################
        #
        #   ########### NIC statistics for port 1 ###########
        #   values...
        #   #################################################
        #
        iter = re.finditer(r"(^  #*.+#*$[^#]+)^  #*\r$", output, re.MULTILINE)
        return ([TestPmdPortStats.parse(block.group(1)) for block in iter], output)

    def show_port_stats(self, port_id: int) -> TestPmdPortStats:
        """Returns the given port statistics.

        Args:
            port_id: The port ID to gather information for.

        Raises:
            InteractiveCommandExecutionError: If `port_id` is invalid.

        Returns:
            TestPmdPortStats: An instance of `TestPmdPortStats` containing the given port's stats.
        """
        output = self.send_command(f"show port stats {port_id}", skip_first_line=True)
        if output.startswith("Invalid port"):
            raise InteractiveCommandExecutionError("invalid port given")

        return TestPmdPortStats.parse(output)

    def set_multicast_all(self, on: bool, verify: bool = True) -> None:
        """Turns multicast mode on/off for the specified port.

        Args:
            on: If :data:`True`, turns multicast mode on, otherwise turns off.
            verify: If :data:`True` an additional command will be sent to verify
                that multicast mode is properly set. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and multicast
                mode is not properly set.
        """
        multicast_cmd_output = self.send_command(f"set allmulti all {'on' if on else 'off'}")
        if verify:
            port_stats = self.show_port_info_all()
            if on ^ all(stats.is_allmulticast_mode_enabled for stats in port_stats):
                self._logger.debug(
                    f"Failed to set multicast mode on all ports.: \n{multicast_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    "Testpmd failed to set multicast mode on all ports."
                )

    @_requires_stopped_ports
    def csum_set_hw(
        self, layers: ChecksumOffloadOptions, port_id: int, verify: bool = True
    ) -> None:
        """Enables hardware checksum offloading on the specified layer.

        Args:
            layers: The layer/layers that checksum offloading should be enabled on.
            port_id: The port number to enable checksum offloading on, should be within 0-32.
            verify: If :data:`True` the output of the command will be scanned in an attempt to
                verify that checksum offloading was enabled on the port.

        Raises:
            InteractiveCommandExecutionError: If checksum offload is not enabled successfully.
        """
        for name, offload in ChecksumOffloadOptions.__members__.items():
            if offload in layers:
                name = name.replace("_", "-")
                csum_output = self.send_command(f"csum set {name} hw {port_id}")
                if verify:
                    if (
                        "Bad arguments" in csum_output
                        or f"Please stop port {port_id} first" in csum_output
                        or f"checksum offload is not supported by port {port_id}" in csum_output
                    ):
                        self._logger.debug(f"Csum set hw error:\n{csum_output}")
                        raise InteractiveCommandExecutionError(
                            f"Failed to set csum hw {name} mode on port {port_id}"
                        )
                success = False
                if f"{name} checksum offload is hw" in csum_output.lower():
                    success = True
                if not success and verify:
                    self._logger.debug(
                        f"Failed to set csum hw mode on port {port_id}:\n{csum_output}"
                    )
                    raise InteractiveCommandExecutionError(
                        f"""Failed to set csum hw mode on port
                                                           {port_id}:\n{csum_output}"""
                    )

    def flow_create(self, flow_rule: FlowRule, port_id: int) -> int:
        """Creates a flow rule in the testpmd session.

        This command is implicitly verified as needed to return the created flow rule id.

        Args:
            flow_rule: :class:`FlowRule` object used for creating testpmd flow rule.
            port_id: Integer representing the port to use.

        Raises:
            InteractiveCommandExecutionError: If flow rule is invalid.

        Returns:
            Id of created flow rule.
        """
        flow_output = self.send_command(f"flow create {port_id} {flow_rule}")
        match = re.search(r"#(\d+)", flow_output)
        if match is not None:
            match_str = match.group(1)
            flow_id = int(match_str)
            return flow_id
        else:
            self._logger.debug(f"Failed to create flow rule:\n{flow_output}")
            raise InteractiveCommandExecutionError(f"Failed to create flow rule:\n{flow_output}")

    def flow_validate(self, flow_rule: FlowRule, port_id: int) -> bool:
        """Validates a flow rule in the testpmd session.

        Args:
            flow_rule: :class:`FlowRule` object used for validating testpmd flow rule.
            port_id: Integer representing the port to use.

        Returns:
            Boolean representing whether rule is valid or not.
        """
        flow_output = self.send_command(f"flow validate {port_id} {flow_rule}")
        if "Flow rule validated" in flow_output:
            return True
        return False

    def flow_delete(self, flow_id: int, port_id: int, verify: bool = True) -> None:
        """Deletes the specified flow rule from the testpmd session.

        Args:
            flow_id: ID of the flow to remove.
            port_id: Integer representing the port to use.
            verify: If :data:`True`, the output of the command is scanned
                to ensure the flow rule was deleted successfully.

        Raises:
            InteractiveCommandExecutionError: If flow rule is invalid.
        """
        flow_output = self.send_command(f"flow destroy {port_id} rule {flow_id}")
        if verify:
            if "destroyed" not in flow_output:
                self._logger.debug(f"Failed to delete flow rule:\n{flow_output}")
                raise InteractiveCommandExecutionError(
                    f"Failed to delete flow rule:\n{flow_output}"
                )

    @_requires_started_ports
    @_requires_stopped_ports
    def set_port_mtu(self, port_id: int, mtu: int, verify: bool = True) -> None:
        """Change the MTU of a port using testpmd.

        Some PMDs require that the port be stopped before changing the MTU, and it does no harm to
        stop the port before configuring in cases where it isn't required, so ports are stopped
        prior to changing their MTU. On the other hand, some PMDs require that the port had already
        been started once since testpmd startup. Therefore, ports are also started before stopping
        them to ensure this has happened.

        Args:
            port_id: ID of the port to adjust the MTU on.
            mtu: Desired value for the MTU to be set to.
            verify: If `verify` is :data:`True` then the output will be scanned in an attempt to
                verify that the mtu was properly set on the port. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the MTU was not
                properly updated on the port matching `port_id`.
        """
        set_mtu_output = self.send_command(f"port config mtu {port_id} {mtu}")
        if verify and (f"MTU: {mtu}" not in self.send_command(f"show port info {port_id}")):
            self._logger.debug(
                f"Failed to set mtu to {mtu} on port {port_id}. Output was:\n{set_mtu_output}"
            )
            raise InteractiveCommandExecutionError(
                f"Test pmd failed to update mtu of port {port_id} to {mtu}"
            )

    def set_port_mtu_all(self, mtu: int, verify: bool = True) -> None:
        """Change the MTU of all ports using testpmd.

        Runs :meth:`set_port_mtu` for every port that testpmd is aware of.

        Args:
            mtu: Desired value for the MTU to be set to.
            verify: Whether to verify that setting the MTU on each port was successful or not.
                Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the MTU was not
                properly updated on at least one port.
        """
        for port in self.ports:
            self.set_port_mtu(port.id, mtu, verify)

    @staticmethod
    def extract_verbose_output(output: str) -> list[TestPmdVerbosePacket]:
        """Extract the verbose information present in given testpmd output.

        This method extracts sections of verbose output that begin with the line
        "port X/queue Y: sent/received Z packets" and end with the ol_flags of a packet.

        Args:
            output: Testpmd output that contains verbose information

        Returns:
            List of parsed packet information gathered from verbose information in `output`.
        """
        out: list[TestPmdVerbosePacket] = []
        prev_header: str = ""
        iter = re.finditer(
            r"(?P<HEADER>(?:port \d+/queue \d+: (?:received|sent) \d+ packets)?)\s*"
            r"(?P<PACKET>src=[\w\s=:-]+?ol_flags: [\w ]+)",
            output,
        )
        for match in iter:
            if match.group("HEADER"):
                prev_header = match.group("HEADER")
            out.append(TestPmdVerbosePacket.parse(f"{prev_header}\n{match.group('PACKET')}"))
        return out

    @_requires_stopped_ports
    def set_vlan_filter(self, port: int, enable: bool, verify: bool = True) -> None:
        """Set vlan filter on.

        Args:
            port: The port number to enable VLAN filter on.
            enable: Enable the filter on `port` if :data:`True`, otherwise disable it.
            verify: If :data:`True`, the output of the command and show port info
                is scanned to verify that vlan filtering was set successfully.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the filter
                fails to update.
        """
        filter_cmd_output = self.send_command(f"vlan set filter {'on' if enable else 'off'} {port}")
        if verify:
            vlan_settings = self.show_port_info(port_id=port).vlan_offload
            if enable ^ (vlan_settings is not None and VLANOffloadFlag.FILTER in vlan_settings):
                self._logger.debug(
                    f"""Failed to {"enable" if enable else "disable"}
                                   filter on port {port}: \n{filter_cmd_output}"""
                )
                raise InteractiveCommandExecutionError(
                    f"""Failed to {"enable" if enable else "disable"}
                    filter on port {port}"""
                )

    def set_mac_address(self, port: int, mac_address: str, verify: bool = True) -> None:
        """Set port's MAC address.

        Args:
            port: The number of the requested port.
            mac_address: The MAC address to set.
            verify: If :data:`True`, the output of the command is scanned to verify that
                the mac address is set in the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the command
                fails to execute.
        """
        output = self.send_command(f"mac_addr set {port} {mac_address}", skip_first_line=True)
        if verify:
            if output.strip():
                self._logger.debug(
                    f"Testpmd failed to set MAC address {mac_address} on port {port}."
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set MAC address {mac_address} on port {port}."
                )

    def set_flow_control(
        self, port: int, flow_ctrl: TestPmdPortFlowCtrl, verify: bool = True
    ) -> None:
        """Set the given `port`'s flow control.

        Args:
            port: The number of the requested port.
            flow_ctrl: The requested flow control parameters.
            verify: If :data:`True`, the output of the command is scanned to verify that
                the flow control in the specified port is set.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the command
                fails to execute.
        """
        output = self.send_command(f"set flow_ctrl {flow_ctrl} {port}", skip_first_line=True)
        if verify:
            if output.strip():
                self._logger.debug(f"Testpmd failed to set the {flow_ctrl} in port {port}.")
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set the {flow_ctrl} in port {port}."
                )

    def show_port_flow_info(self, port: int) -> TestPmdPortFlowCtrl | None:
        """Show port info flow.

        Args:
            port: The number of the requested port.

        Returns:
            The current port flow control parameters if supported, otherwise :data:`None`.
        """
        output = self.send_command(f"show port {port} flow_ctrl")
        if "Flow control infos" in output:
            return TestPmdPortFlowCtrl.parse(output)
        return None

    @_requires_stopped_ports
    def rx_vlan(self, vlan: int, port: int, add: bool, verify: bool = True) -> None:
        """Add specified vlan tag to the filter list on a port. Requires vlan filter to be on.

        Args:
            vlan: The vlan tag to add, should be within 1-1005.
            port: The port number to add the tag on.
            add: Adds the tag if :data:`True`, otherwise removes the tag.
            verify: If :data:`True`, the output of the command is scanned to verify that
                the vlan tag was added to the filter list on the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the tag
                is not added.
        """
        rx_cmd_output = self.send_command(f"rx_vlan {'add' if add else 'rm'} {vlan} {port}")
        if verify:
            if (
                "VLAN-filtering disabled" in rx_cmd_output
                or "Invalid vlan_id" in rx_cmd_output
                or "Bad arguments" in rx_cmd_output
            ):
                self._logger.debug(
                    f"""Failed to {"add" if add else "remove"} tag {vlan}
                    port {port}: \n{rx_cmd_output}"""
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to {'add' if add else 'remove'} tag {vlan} on port {port}."
                )

    @_requires_stopped_ports
    def set_vlan_strip(self, port: int, enable: bool, verify: bool = True) -> None:
        """Enable or disable vlan stripping on the specified port.

        Args:
            port: The port number to use.
            enable: If :data:`True`, will turn vlan stripping on, otherwise will turn off.
            verify: If :data:`True`, the output of the command and show port info
                is scanned to verify that vlan stripping was enabled on the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and stripping
                fails to update.
        """
        strip_cmd_output = self.send_command(f"vlan set strip {'on' if enable else 'off'} {port}")
        if verify:
            vlan_settings = self.show_port_info(port_id=port).vlan_offload
            if enable ^ (vlan_settings is not None and VLANOffloadFlag.STRIP in vlan_settings):
                self._logger.debug(
                    f"""Failed to set strip {"on" if enable else "off"}
                    port {port}: \n{strip_cmd_output}"""
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set strip {'on' if enable else 'off'} port {port}."
                )

    @_requires_stopped_ports
    def tx_vlan_set(
        self, port: int, enable: bool, vlan: int | None = None, verify: bool = True
    ) -> None:
        """Set hardware insertion of vlan tags in packets sent on a port.

        Args:
            port: The port number to use.
            enable: Sets vlan tag insertion if :data:`True`, and resets if :data:`False`.
            vlan: The vlan tag to insert if enable is :data:`True`.
            verify: If :data:`True`, the output of the command is scanned to verify that
                vlan insertion was enabled on the specified port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the insertion
                tag is not set.
        """
        if enable:
            tx_vlan_cmd_output = self.send_command(f"tx_vlan set {port} {vlan}")
            if verify:
                if (
                    "Please stop port" in tx_vlan_cmd_output
                    or "Invalid vlan_id" in tx_vlan_cmd_output
                    or "Invalid port" in tx_vlan_cmd_output
                ):
                    self._logger.debug(
                        f"Failed to set vlan tag {vlan} on port {port}:\n{tx_vlan_cmd_output}"
                    )
                    raise InteractiveCommandExecutionError(
                        f"Testpmd failed to set vlan insertion tag {vlan} on port {port}."
                    )
        else:
            tx_vlan_cmd_output = self.send_command(f"tx_vlan reset {port}")
            if verify:
                if "Please stop port" in tx_vlan_cmd_output or "Invalid port" in tx_vlan_cmd_output:
                    self._logger.debug(
                        f"Failed to reset vlan insertion on port {port}: \n{tx_vlan_cmd_output}"
                    )
                    raise InteractiveCommandExecutionError(
                        f"Testpmd failed to reset vlan insertion on port {port}."
                    )

    def set_promisc(self, port: int, enable: bool, verify: bool = True) -> None:
        """Enable or disable promiscuous mode for the specified port.

        Args:
            port: Port number to use.
            enable: If :data:`True`, turn promiscuous mode on, otherwise turn off.
            verify: If :data:`True` an additional command will be sent to verify that
                promiscuous mode is properly set. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and promiscuous mode
                is not correctly set.
        """
        promisc_cmd_output = self.send_command(f"set promisc {port} {'on' if enable else 'off'}")
        if verify:
            stats = self.show_port_info(port_id=port)
            if enable ^ stats.is_promiscuous_mode_enabled:
                self._logger.debug(
                    f"Failed to set promiscuous mode on port {port}: \n{promisc_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set promiscuous mode on port {port}."
                )

    def set_verbose(self, level: int, verify: bool = True) -> None:
        """Set debug verbosity level.

        Args:
            level: 0 - silent except for error
                1 - fully verbose except for Tx packets
                2 - fully verbose except for Rx packets
                >2 - fully verbose
            verify: If :data:`True` the command output will be scanned to verify that verbose level
                is properly set. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and verbose level
            is not correctly set.
        """
        verbose_cmd_output = self.send_command(f"set verbose {level}")
        if verify:
            if "Change verbose level" not in verbose_cmd_output:
                self._logger.debug(
                    f"Failed to set verbose level to {level}: \n{verbose_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    f"Testpmd failed to set verbose level to {level}."
                )

    def rx_vxlan(self, vxlan_id: int, port_id: int, enable: bool, verify: bool = True) -> None:
        """Add or remove vxlan id to/from filter list.

        Args:
            vxlan_id: VXLAN ID to add to port filter list.
            port_id: ID of the port to modify VXLAN filter of.
            enable: If :data:`True`, adds specified VXLAN ID, otherwise removes it.
            verify: If :data:`True`, the output of the command is checked to verify
                the VXLAN ID was successfully added/removed from the port.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and VXLAN ID
                is not successfully added or removed.
        """
        action = "add" if enable else "rm"
        vxlan_output = self.send_command(f"rx_vxlan_port {action} {vxlan_id} {port_id}")
        if verify:
            if "udp tunneling add error" in vxlan_output:
                self._logger.debug(f"Failed to set VXLAN:\n{vxlan_output}")
                raise InteractiveCommandExecutionError(f"Failed to set VXLAN:\n{vxlan_output}")

    def clear_port_stats(self, port_id: int, verify: bool = True) -> None:
        """Clear statistics of a given port.

        Args:
            port_id: ID of the port to clear the statistics on.
            verify: If :data:`True` the output of the command will be scanned to verify that it was
                successful, otherwise failures will be ignored. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and testpmd fails to
                clear the statistics of the given port.
        """
        clear_output = self.send_command(f"clear port stats {port_id}")
        if verify and f"NIC statistics for port {port_id} cleared" not in clear_output:
            raise InteractiveCommandExecutionError(
                f"Test pmd failed to set clear forwarding stats on port {port_id}"
            )

    def clear_port_stats_all(self, verify: bool = True) -> None:
        """Clear the statistics of all ports that testpmd is aware of.

        Args:
            verify: If :data:`True` the output of the command will be scanned to verify that all
                ports had their statistics cleared, otherwise failures will be ignored. Defaults to
                :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and testpmd fails to
                clear the statistics of any of its ports.
        """
        clear_output = self.send_command("clear port stats all")
        if verify:
            if type(self._app_params.port_numa_config) is list:
                for port_id in range(len(self._app_params.port_numa_config)):
                    if f"NIC statistics for port {port_id} cleared" not in clear_output:
                        raise InteractiveCommandExecutionError(
                            f"Test pmd failed to set clear forwarding stats on port {port_id}"
                        )

    @only_active
    def close(self) -> None:
        """Overrides :meth:`~.interactive_shell.close`."""
        self.stop()
        self.send_command("quit", "Bye...")
        return super().close()

    """
    ====== Capability retrieval methods ======
    """

    def get_capabilities_rx_offload(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get all rx offload capabilities and divide them into supported and unsupported.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._logger.debug("Getting rx offload capabilities.")
        command = f"show port {self.ports[0].id} rx_offload capabilities"
        rx_offload_capabilities_out = self.send_command(command)
        rx_offload_capabilities = RxOffloadCapabilities.parse(rx_offload_capabilities_out)
        self._update_capabilities_from_flag(
            supported_capabilities,
            unsupported_capabilities,
            RxOffloadCapability,
            rx_offload_capabilities.per_port | rx_offload_capabilities.per_queue,
        )

    def get_port_queue_info(
        self, port_id: int, queue_id: int, is_rx_queue: bool
    ) -> TestPmdQueueInfo:
        """Returns the current state of the specified queue."""
        command = f"show {'rxq' if is_rx_queue else 'txq'} info {port_id} {queue_id}"
        queue_info = TestPmdQueueInfo.parse(self.send_command(command))
        return queue_info

    def setup_port_queue(self, port_id: int, queue_id: int, is_rx_queue: bool) -> None:
        """Setup a given queue on a port.

        This functionality cannot be verified because the setup action only takes effect when the
        queue is started.

        Args:
            port_id: ID of the port where the queue resides.
            queue_id: ID of the queue to setup.
            is_rx_queue: Type of queue to setup. If :data:`True` an RX queue will be setup,
                otherwise a TX queue will be setup.
        """
        self.send_command(f"port {port_id} {'rxq' if is_rx_queue else 'txq'} {queue_id} setup")

    def stop_port_queue(
        self, port_id: int, queue_id: int, is_rx_queue: bool, verify: bool = True
    ) -> None:
        """Stops a given queue on a port.

        Args:
            port_id: ID of the port that the queue belongs to.
            queue_id: ID of the queue to stop.
            is_rx_queue: Type of queue to stop. If :data:`True` an RX queue will be stopped,
                otherwise a TX queue will be stopped.
            verify: If :data:`True` an additional command will be sent to verify the queue stopped.
                Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the queue fails to
                stop.
        """
        port_type = "rxq" if is_rx_queue else "txq"
        stop_cmd_output = self.send_command(f"port {port_id} {port_type} {queue_id} stop")
        if verify:
            queue_started = self.get_port_queue_info(
                port_id, queue_id, is_rx_queue
            ).is_queue_started
            if queue_started:
                self._logger.debug(
                    f"Failed to stop {port_type} {queue_id} on port {port_id}:\n{stop_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    f"Test pmd failed to stop {port_type} {queue_id} on port {port_id}"
                )

    def start_port_queue(
        self, port_id: int, queue_id: int, is_rx_queue: bool, verify: bool = True
    ) -> None:
        """Starts a given queue on a port.

        First sets up the port queue, then starts it.

        Args:
            port_id: ID of the port that the queue belongs to.
            queue_id: ID of the queue to start.
            is_rx_queue: Type of queue to start. If :data:`True` an RX queue will be started,
                otherwise a TX queue will be started.
            verify: if :data:`True` an additional command will be sent to verify that the queue was
                started. Defaults to :data:`True`.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and the queue fails to
                start.
        """
        port_type = "rxq" if is_rx_queue else "txq"
        self.setup_port_queue(port_id, queue_id, is_rx_queue)
        start_cmd_output = self.send_command(f"port {port_id} {port_type} {queue_id} start")
        if verify:
            queue_started = self.get_port_queue_info(
                port_id, queue_id, is_rx_queue
            ).is_queue_started
            if not queue_started:
                self._logger.debug(
                    f"Failed to start {port_type} {queue_id} on port {port_id}:\n{start_cmd_output}"
                )
                raise InteractiveCommandExecutionError(
                    f"Test pmd failed to start {port_type} {queue_id} on port {port_id}"
                )

    def get_queue_ring_size(self, port_id: int, queue_id: int, is_rx_queue: bool) -> int:
        """Returns the current size of the ring on the specified queue."""
        command = f"show {'rxq' if is_rx_queue else 'txq'} info {port_id} {queue_id}"
        queue_info = TestPmdQueueInfo.parse(self.send_command(command))
        return queue_info.ring_size

    def set_queue_ring_size(
        self,
        port_id: int,
        queue_id: int,
        size: int,
        is_rx_queue: bool,
        verify: bool = True,
    ) -> None:
        """Update the ring size of an Rx/Tx queue on a given port.

        Queue is setup after setting the ring size so that the queue info reflects this change and
        it can be verified.

        Args:
            port_id: The port that the queue resides on.
            queue_id: The ID of the queue on the port.
            size: The size to update the ring size to.
            is_rx_queue: Whether to modify an RX or TX queue. If :data:`True` an RX queue will be
                updated, otherwise a TX queue will be updated.
            verify: If :data:`True` an additional command will be sent to check the ring size of
                the queue in an attempt to validate that the size was changes properly.

        Raises:
            InteractiveCommandExecutionError: If `verify` is :data:`True` and there is a failure
                when updating ring size.
        """
        queue_type = "rxq" if is_rx_queue else "txq"
        self.send_command(f"port config {port_id} {queue_type} {queue_id} ring_size {size}")
        self.setup_port_queue(port_id, queue_id, is_rx_queue)
        if verify:
            curr_ring_size = self.get_queue_ring_size(port_id, queue_id, is_rx_queue)
            if curr_ring_size != size:
                self._logger.debug(
                    f"Failed up update ring size of queue {queue_id} on port {port_id}. Current"
                    f" ring size is {curr_ring_size}."
                )
                raise InteractiveCommandExecutionError(
                    f"Failed to update ring size of queue {queue_id} on port {port_id}"
                )

    @_requires_stopped_ports
    def set_queue_deferred_start(
        self, port_id: int, queue_id: int, is_rx_queue: bool, on: bool
    ) -> None:
        """Set the deferred start attribute of the specified queue on/off.

        Args:
            port_id: The port that the queue resides on.
            queue_id: The ID of the queue on the port.
            is_rx_queue: Whether to modify an RX or TX queue. If :data:`True` an RX queue will be
                updated, otherwise a TX queue will be updated.
            on: Whether to set deferred start mode on or off. If :data:`True` deferred start will
                be turned on, otherwise it will be turned off.
        """
        queue_type = "rxq" if is_rx_queue else "txq"
        action = "on" if on else "off"
        self.send_command(f"port {port_id} {queue_type} {queue_id} deferred_start {action}")

    def _update_capabilities_from_flag(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
        flag_class: type[Flag],
        supported_flags: Flag,
    ) -> None:
        """Divide all flags from `flag_class` into supported and unsupported."""
        for flag in flag_class:
            if flag in supported_flags:
                supported_capabilities.add(NicCapability[str(flag.name)])
            else:
                unsupported_capabilities.add(NicCapability[str(flag.name)])

    @_requires_started_ports
    def get_capabilities_rxq_info(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get all rxq capabilities and divide them into supported and unsupported.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._logger.debug("Getting rxq capabilities.")
        command = f"show rxq info {self.ports[0].id} 0"
        rxq_info = TestPmdRxqInfo.parse(self.send_command(command))
        if rxq_info.scattered_packets:
            supported_capabilities.add(NicCapability.SCATTERED_RX_ENABLED)
        else:
            unsupported_capabilities.add(NicCapability.SCATTERED_RX_ENABLED)

    def get_capabilities_show_port_info(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get all capabilities from show port info and divide them into supported and unsupported.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._update_capabilities_from_flag(
            supported_capabilities,
            unsupported_capabilities,
            DeviceCapabilitiesFlag,
            self.ports[0].device_capabilities,
        )

    def get_capabilities_mcast_filtering(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get multicast filtering capability from mcast_addr add and check for testpmd error code.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._logger.debug("Getting mcast filter capabilities.")
        command = f"mcast_addr add {self.ports[0].id} 01:00:5E:00:00:00"
        output = self.send_command(command)
        if "diag=-95" in output:
            unsupported_capabilities.add(NicCapability.MCAST_FILTERING)
        else:
            supported_capabilities.add(NicCapability.MCAST_FILTERING)
            command = str.replace(command, "add", "remove", 1)
            self.send_command(command)

    def get_capabilities_flow_ctrl(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Get flow control capability and check for testpmd failure.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        self._logger.debug("Getting flow ctrl capabilities.")
        command = f"show port {self.ports[0].id} flow_ctrl"
        output = self.send_command(command)
        if "Flow control infos" in output:
            supported_capabilities.add(NicCapability.FLOW_CTRL)
        else:
            unsupported_capabilities.add(NicCapability.FLOW_CTRL)

    def get_capabilities_physical_function(
        self,
        supported_capabilities: MutableSet["NicCapability"],
        unsupported_capabilities: MutableSet["NicCapability"],
    ) -> None:
        """Store capability representing a physical function test run.

        Args:
            supported_capabilities: Supported capabilities will be added to this set.
            unsupported_capabilities: Unsupported capabilities will be added to this set.
        """
        ctx = get_ctx()
        if ctx.topology.vf_ports == []:
            supported_capabilities.add(NicCapability.PHYSICAL_FUNCTION)
        else:
            unsupported_capabilities.add(NicCapability.PHYSICAL_FUNCTION)
