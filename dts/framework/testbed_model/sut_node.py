# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

import os
import tarfile
import time
from pathlib import PurePath
from typing import Type

from framework.config import (
    BuildTargetConfiguration,
    BuildTargetInfo,
    NodeInfo,
    SutNodeConfiguration,
)
from framework.remote_session import CommandResult, InteractiveShellType, OSSession
from framework.settings import SETTINGS
from framework.utils import MesonArgs

from .hw import LogicalCoreCount, LogicalCoreList, VirtualDevice
from .node import Node


class EalParameters(object):
    def __init__(
        self,
        lcore_list: LogicalCoreList,
        memory_channels: int,
        prefix: str,
        no_pci: bool,
        vdevs: list[VirtualDevice],
        other_eal_param: str,
    ):
        """
        Generate eal parameters character string;
        :param lcore_list: the list of logical cores to use.
        :param memory_channels: the number of memory channels to use.
        :param prefix: set file prefix string, eg:
                        prefix='vf'
        :param no_pci: switch of disable PCI bus eg:
                        no_pci=True
        :param vdevs: virtual device list, eg:
                        vdevs=[
                            VirtualDevice('net_ring0'),
                            VirtualDevice('net_ring1')
                        ]
        :param other_eal_param: user defined DPDK eal parameters, eg:
                        other_eal_param='--single-file-segments'
        """
        self._lcore_list = f"-l {lcore_list}"
        self._memory_channels = f"-n {memory_channels}"
        self._prefix = prefix
        if prefix:
            self._prefix = f"--file-prefix={prefix}"
        self._no_pci = "--no-pci" if no_pci else ""
        self._vdevs = " ".join(f"--vdev {vdev}" for vdev in vdevs)
        self._other_eal_param = other_eal_param

    def __str__(self) -> str:
        return (
            f"{self._lcore_list} "
            f"{self._memory_channels} "
            f"{self._prefix} "
            f"{self._no_pci} "
            f"{self._vdevs} "
            f"{self._other_eal_param}"
        )


class SutNode(Node):
    """
    A class for managing connections to the System under Test, providing
    methods that retrieve the necessary information about the node (such as
    CPU, memory and NIC details) and configuration capabilities.
    Another key capability is building DPDK according to given build target.
    """

    config: SutNodeConfiguration
    _dpdk_prefix_list: list[str]
    _dpdk_timestamp: str
    _build_target_config: BuildTargetConfiguration | None
    _env_vars: dict
    _remote_tmp_dir: PurePath
    __remote_dpdk_dir: PurePath | None
    _app_compile_timeout: float
    _dpdk_kill_session: OSSession | None
    _dpdk_version: str | None
    _node_info: NodeInfo | None
    _compiler_version: str | None
    _path_to_devbind_script: PurePath | None

    def __init__(self, node_config: SutNodeConfiguration):
        super(SutNode, self).__init__(node_config)
        self._dpdk_prefix_list = []
        self._build_target_config = None
        self._env_vars = {}
        self._remote_tmp_dir = self.main_session.get_remote_tmp_dir()
        self.__remote_dpdk_dir = None
        self._app_compile_timeout = 90
        self._dpdk_kill_session = None
        self._dpdk_timestamp = (
            f"{str(os.getpid())}_{time.strftime('%Y%m%d%H%M%S', time.localtime())}"
        )
        self._dpdk_version = None
        self._node_info = None
        self._compiler_version = None
        self._path_to_devbind_script = None
        self._logger.info(f"Created node: {self.name}")

    @property
    def _remote_dpdk_dir(self) -> PurePath:
        if self.__remote_dpdk_dir is None:
            self.__remote_dpdk_dir = self._guess_dpdk_remote_dir()
        return self.__remote_dpdk_dir

    @_remote_dpdk_dir.setter
    def _remote_dpdk_dir(self, value: PurePath) -> None:
        self.__remote_dpdk_dir = value

    @property
    def remote_dpdk_build_dir(self) -> PurePath:
        if self._build_target_config:
            return self.main_session.join_remote_path(
                self._remote_dpdk_dir, self._build_target_config.name
            )
        else:
            return self.main_session.join_remote_path(self._remote_dpdk_dir, "build")

    @property
    def dpdk_version(self) -> str:
        if self._dpdk_version is None:
            self._dpdk_version = self.main_session.get_dpdk_version(
                self._remote_dpdk_dir
            )
        return self._dpdk_version

    @property
    def node_info(self) -> NodeInfo:
        if self._node_info is None:
            self._node_info = self.main_session.get_node_info()
        return self._node_info

    @property
    def compiler_version(self) -> str:
        if self._compiler_version is None:
            if self._build_target_config is not None:
                self._compiler_version = self.main_session.get_compiler_version(
                    self._build_target_config.compiler.name
                )
            else:
                self._logger.warning(
                    "Failed to get compiler version because"
                    "_build_target_config is None."
                )
                return ""
        return self._compiler_version

    @property
    def path_to_devbind_script(self) -> PurePath:
        if self._path_to_devbind_script is None:
            self._path_to_devbind_script = self.main_session.join_remote_path(
                self._remote_dpdk_dir, "usertools", "dpdk-devbind.py"
            )
        return self._path_to_devbind_script

    def get_build_target_info(self) -> BuildTargetInfo:
        return BuildTargetInfo(
            dpdk_version=self.dpdk_version, compiler_version=self.compiler_version
        )

    def _guess_dpdk_remote_dir(self) -> PurePath:
        return self.main_session.guess_dpdk_remote_dir(self._remote_tmp_dir)

    def _set_up_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> None:
        """
        Setup DPDK on the SUT node.
        """
        # we want to ensure that dpdk_version and compiler_version is reset for new
        # build targets
        self._dpdk_version = None
        self._compiler_version = None
        self._configure_build_target(build_target_config)
        self._copy_dpdk_tarball()
        self._build_dpdk()
        self.bind_ports_to_driver()

    def _tear_down_build_target(self) -> None:
        """
        This method exists to be optionally overwritten by derived classes and
        is not decorated so that the derived class doesn't have to use the decorator.
        """
        self.bind_ports_to_driver(for_dpdk=False)

    def _configure_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> None:
        """
        Populate common environment variables and set build target config.
        """
        self._env_vars = {}
        self._build_target_config = build_target_config
        self._env_vars.update(
            self.main_session.get_dpdk_build_env_vars(build_target_config.arch)
        )
        self._env_vars["CC"] = build_target_config.compiler.name
        if build_target_config.compiler_wrapper:
            self._env_vars["CC"] = (
                f"'{build_target_config.compiler_wrapper} "
                f"{build_target_config.compiler.name}'"
            )

    @Node.skip_setup
    def _copy_dpdk_tarball(self) -> None:
        """
        Copy to and extract DPDK tarball on the SUT node.
        """
        self._logger.info("Copying DPDK tarball to SUT.")
        self.main_session.copy_to(SETTINGS.dpdk_tarball_path, self._remote_tmp_dir)

        # construct remote tarball path
        # the basename is the same on local host and on remote Node
        remote_tarball_path = self.main_session.join_remote_path(
            self._remote_tmp_dir, os.path.basename(SETTINGS.dpdk_tarball_path)
        )

        # construct remote path after extracting
        with tarfile.open(SETTINGS.dpdk_tarball_path) as dpdk_tar:
            dpdk_top_dir = dpdk_tar.getnames()[0]
        self._remote_dpdk_dir = self.main_session.join_remote_path(
            self._remote_tmp_dir, dpdk_top_dir
        )

        self._logger.info(
            f"Extracting DPDK tarball on SUT: "
            f"'{remote_tarball_path}' into '{self._remote_dpdk_dir}'."
        )
        # clean remote path where we're extracting
        self.main_session.remove_remote_dir(self._remote_dpdk_dir)

        # then extract to remote path
        self.main_session.extract_remote_tarball(
            remote_tarball_path, self._remote_dpdk_dir
        )

    @Node.skip_setup
    def _build_dpdk(self) -> None:
        """
        Build DPDK. Uses the already configured target. Assumes that the tarball has
        already been copied to and extracted on the SUT node.
        """
        self.main_session.build_dpdk(
            self._env_vars,
            MesonArgs(default_library="static", enable_kmods=True, libdir="lib"),
            self._remote_dpdk_dir,
            self.remote_dpdk_build_dir,
        )

    def build_dpdk_app(self, app_name: str, **meson_dpdk_args: str | bool) -> PurePath:
        """
        Build one or all DPDK apps. Requires DPDK to be already built on the SUT node.
        When app_name is 'all', build all example apps.
        When app_name is any other string, tries to build that example app.
        Return the directory path of the built app. If building all apps, return
        the path to the examples directory (where all apps reside).
        The meson_dpdk_args are keyword arguments
        found in meson_option.txt in root DPDK directory. Do not use -D with them,
        for example: enable_kmods=True.
        """
        self.main_session.build_dpdk(
            self._env_vars,
            MesonArgs(examples=app_name, **meson_dpdk_args),  # type: ignore [arg-type]
            # ^^ https://github.com/python/mypy/issues/11583
            self._remote_dpdk_dir,
            self.remote_dpdk_build_dir,
            rebuild=True,
            timeout=self._app_compile_timeout,
        )

        if app_name == "all":
            return self.main_session.join_remote_path(
                self.remote_dpdk_build_dir, "examples"
            )
        return self.main_session.join_remote_path(
            self.remote_dpdk_build_dir, "examples", f"dpdk-{app_name}"
        )

    def kill_cleanup_dpdk_apps(self) -> None:
        """
        Kill all dpdk applications on the SUT. Cleanup hugepages.
        """
        if self._dpdk_kill_session and self._dpdk_kill_session.is_alive():
            # we can use the session if it exists and responds
            self._dpdk_kill_session.kill_cleanup_dpdk_apps(self._dpdk_prefix_list)
        else:
            # otherwise, we need to (re)create it
            self._dpdk_kill_session = self.create_session("dpdk_kill")
        self._dpdk_prefix_list = []

    def create_eal_parameters(
        self,
        lcore_filter_specifier: LogicalCoreCount | LogicalCoreList = LogicalCoreCount(),
        ascending_cores: bool = True,
        prefix: str = "dpdk",
        append_prefix_timestamp: bool = True,
        no_pci: bool = False,
        vdevs: list[VirtualDevice] = None,
        other_eal_param: str = "",
    ) -> "EalParameters":
        """
        Generate eal parameters character string;
        :param lcore_filter_specifier: a number of lcores/cores/sockets to use
                        or a list of lcore ids to use.
                        The default will select one lcore for each of two cores
                        on one socket, in ascending order of core ids.
        :param ascending_cores: True, use cores with the lowest numerical id first
                        and continue in ascending order. If False, start with the
                        highest id and continue in descending order. This ordering
                        affects which sockets to consider first as well.
        :param prefix: set file prefix string, eg:
                        prefix='vf'
        :param append_prefix_timestamp: if True, will append a timestamp to
                        DPDK file prefix.
        :param no_pci: switch of disable PCI bus eg:
                        no_pci=True
        :param vdevs: virtual device list, eg:
                        vdevs=[
                            VirtualDevice('net_ring0'),
                            VirtualDevice('net_ring1')
                        ]
        :param other_eal_param: user defined DPDK eal parameters, eg:
                        other_eal_param='--single-file-segments'
        :return: eal param string, eg:
                '-c 0xf -a 0000:88:00.0 --file-prefix=dpdk_1112_20190809143420';
        """

        lcore_list = LogicalCoreList(
            self.filter_lcores(lcore_filter_specifier, ascending_cores)
        )

        if append_prefix_timestamp:
            prefix = f"{prefix}_{self._dpdk_timestamp}"
        prefix = self.main_session.get_dpdk_file_prefix(prefix)
        if prefix:
            self._dpdk_prefix_list.append(prefix)

        if vdevs is None:
            vdevs = []

        return EalParameters(
            lcore_list=lcore_list,
            memory_channels=self.config.memory_channels,
            prefix=prefix,
            no_pci=no_pci,
            vdevs=vdevs,
            other_eal_param=other_eal_param,
        )

    def run_dpdk_app(
        self, app_path: PurePath, eal_args: "EalParameters", timeout: float = 30
    ) -> CommandResult:
        """
        Run DPDK application on the remote node.
        """
        return self.main_session.send_command(
            f"{app_path} {eal_args}", timeout, privileged=True, verify=True
        )

    def configure_ipv4_forwarding(self, enable: bool) -> None:
        self.main_session.configure_ipv4_forwarding(enable)

    def create_interactive_shell(
        self,
        shell_cls: Type[InteractiveShellType],
        timeout: float = SETTINGS.timeout,
        privileged: bool = False,
        eal_parameters: EalParameters | str | None = None,
    ) -> InteractiveShellType:
        """Factory method for creating a handler for an interactive session.

        Instantiate shell_cls according to the remote OS specifics.

        Args:
            shell_cls: The class of the shell.
            timeout: Timeout for reading output from the SSH channel. If you are
                reading from the buffer and don't receive any data within the timeout
                it will throw an error.
            privileged: Whether to run the shell with administrative privileges.
            eal_parameters: List of EAL parameters to use to launch the app. If this
                isn't provided or an empty string is passed, it will default to calling
                create_eal_parameters().
        Returns:
            Instance of the desired interactive application.
        """
        if not eal_parameters:
            eal_parameters = self.create_eal_parameters()

        # We need to append the build directory for DPDK apps
        if shell_cls.dpdk_app:
            shell_cls.path = self.main_session.join_remote_path(
                self.remote_dpdk_build_dir, shell_cls.path
            )

        return super().create_interactive_shell(
            shell_cls, timeout, privileged, str(eal_parameters)
        )

    def bind_ports_to_driver(self, for_dpdk: bool = True) -> None:
        """Bind all ports on the SUT to a driver.

        Args:
            for_dpdk: Boolean that, when True, binds ports to os_driver_for_dpdk
            or, when False, binds to os_driver. Defaults to True.
        """
        for port in self.ports:
            driver = port.os_driver_for_dpdk if for_dpdk else port.os_driver
            self.main_session.send_command(
                f"{self.path_to_devbind_script} -b {driver} --force {port.pci}",
                privileged=True,
                verify=True,
            )
