# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""System under test (DPDK + hardware) node.

A system under test (SUT) is the combination of DPDK
and the hardware we're testing with DPDK (NICs, crypto and other devices).
An SUT node is where this SUT runs.
"""

import os
import time
from dataclasses import dataclass
from pathlib import Path, PurePath

from framework.config.node import (
    SutNodeConfiguration,
)
from framework.config.test_run import (
    DPDKBuildConfiguration,
    DPDKBuildOptionsConfiguration,
    DPDKPrecompiledBuildConfiguration,
    DPDKUncompiledBuildConfiguration,
    LocalDPDKTarballLocation,
    LocalDPDKTreeLocation,
    RemoteDPDKTarballLocation,
    RemoteDPDKTreeLocation,
    TestRunConfiguration,
)
from framework.exception import ConfigurationError, RemoteFileNotFoundError
from framework.params.eal import EalParams
from framework.remote_session.remote_session import CommandResult
from framework.utils import MesonArgs, TarCompressionFormat

from .cpu import LogicalCore, LogicalCoreList
from .node import Node
from .os_session import OSSession, OSSessionInfo
from .virtual_device import VirtualDevice


@dataclass(slots=True, frozen=True)
class DPDKBuildInfo:
    """Various versions and other information about a DPDK build.

    Attributes:
        dpdk_version: The DPDK version that was built.
        compiler_version: The version of the compiler used to build DPDK.
    """

    dpdk_version: str | None
    compiler_version: str | None


class SutNode(Node):
    """The system under test node.

    The SUT node extends :class:`Node` with DPDK specific features:

        * Managing DPDK source tree on the remote SUT,
        * Building the DPDK from source or using a pre-built version,
        * Gathering of DPDK build info,
        * The running of DPDK apps, interactively or one-time execution,
        * DPDK apps cleanup.

    Building DPDK from source uses `build` configuration inside `dpdk_build` of configuration.

    Attributes:
        config: The SUT node configuration.
        virtual_devices: The virtual devices used on the node.
    """

    config: SutNodeConfiguration
    virtual_devices: list[VirtualDevice]
    dpdk_prefix_list: list[str]
    dpdk_timestamp: str
    _env_vars: dict
    _remote_tmp_dir: PurePath
    __remote_dpdk_tree_path: str | PurePath | None
    _remote_dpdk_build_dir: PurePath | None
    _app_compile_timeout: float
    _dpdk_kill_session: OSSession | None
    _dpdk_version: str | None
    _node_info: OSSessionInfo | None
    _compiler_version: str | None
    _path_to_devbind_script: PurePath | None
    _ports_bound_to_dpdk: bool

    def __init__(self, node_config: SutNodeConfiguration):
        """Extend the constructor with SUT node specifics.

        Args:
            node_config: The SUT node's test run configuration.
        """
        super().__init__(node_config)
        self.lcores = self.filter_lcores(LogicalCoreList(self.config.dpdk_config.lcores))
        if LogicalCore(lcore=0, core=0, socket=0, node=0) in self.lcores:
            self._logger.info(
                """
                WARNING: First core being used;
                using the first core is considered risky and should only
                be done by advanced users.
                """
            )
        else:
            self._logger.info("Not using first core")
        self.virtual_devices = []
        self.dpdk_prefix_list = []
        self._env_vars = {}
        self._remote_tmp_dir = self.main_session.get_remote_tmp_dir()
        self.__remote_dpdk_tree_path = None
        self._remote_dpdk_build_dir = None
        self._app_compile_timeout = 90
        self._dpdk_kill_session = None
        self.dpdk_timestamp = (
            f"{str(os.getpid())}_{time.strftime('%Y%m%d%H%M%S', time.localtime())}"
        )
        self._dpdk_version = None
        self._node_info = None
        self._compiler_version = None
        self._path_to_devbind_script = None
        self._ports_bound_to_dpdk = False
        self._logger.info(f"Created node: {self.name}")

    @property
    def _remote_dpdk_tree_path(self) -> str | PurePath:
        """The remote DPDK tree path."""
        if self.__remote_dpdk_tree_path:
            return self.__remote_dpdk_tree_path

        self._logger.warning(
            "Failed to get remote dpdk tree path because we don't know the "
            "location on the SUT node."
        )
        return ""

    @property
    def remote_dpdk_build_dir(self) -> str | PurePath:
        """The remote DPDK build dir path."""
        if self._remote_dpdk_build_dir:
            return self._remote_dpdk_build_dir

        self._logger.warning(
            "Failed to get remote dpdk build dir because we don't know the "
            "location on the SUT node."
        )
        return ""

    @property
    def dpdk_version(self) -> str | None:
        """Last built DPDK version."""
        if self._dpdk_version is None:
            self._dpdk_version = self.main_session.get_dpdk_version(self._remote_dpdk_tree_path)
        return self._dpdk_version

    @property
    def node_info(self) -> OSSessionInfo:
        """Additional node information."""
        if self._node_info is None:
            self._node_info = self.main_session.get_node_info()
        return self._node_info

    @property
    def compiler_version(self) -> str | None:
        """The node's compiler version."""
        if self._compiler_version is None:
            self._logger.warning("The `compiler_version` is None because a pre-built DPDK is used.")

        return self._compiler_version

    @compiler_version.setter
    def compiler_version(self, value: str) -> None:
        """Set the `compiler_version` used on the SUT node.

        Args:
            value: The node's compiler version.
        """
        self._compiler_version = value

    @property
    def path_to_devbind_script(self) -> PurePath | str:
        """The path to the dpdk-devbind.py script on the node."""
        if self._path_to_devbind_script is None:
            self._path_to_devbind_script = self.main_session.join_remote_path(
                self._remote_dpdk_tree_path, "usertools", "dpdk-devbind.py"
            )
        return self._path_to_devbind_script

    def get_dpdk_build_info(self) -> DPDKBuildInfo:
        """Get additional DPDK build information.

        Returns:
            The DPDK build information,
        """
        return DPDKBuildInfo(dpdk_version=self.dpdk_version, compiler_version=self.compiler_version)

    def set_up_test_run(
        self,
        test_run_config: TestRunConfiguration,
        dpdk_build_config: DPDKBuildConfiguration,
    ) -> None:
        """Extend the test run setup with vdev config and DPDK build set up.

        This method extends the setup process by configuring virtual devices and preparing the DPDK
        environment based on the provided configuration.

        Args:
            test_run_config: A test run configuration according to which
                the setup steps will be taken.
            dpdk_build_config: The build configuration of DPDK.
        """
        super().set_up_test_run(test_run_config, dpdk_build_config)
        for vdev in test_run_config.vdevs:
            self.virtual_devices.append(VirtualDevice(vdev))
        self._set_up_dpdk(dpdk_build_config)

    def tear_down_test_run(self) -> None:
        """Extend the test run teardown with virtual device teardown and DPDK teardown."""
        super().tear_down_test_run()
        self.virtual_devices = []
        self._tear_down_dpdk()

    def _set_up_dpdk(
        self,
        dpdk_build_config: DPDKBuildConfiguration,
    ) -> None:
        """Set up DPDK the SUT node and bind ports.

        DPDK setup includes setting all internals needed for the build, the copying of DPDK
        sources and then building DPDK or using the exist ones from the `dpdk_location`. The drivers
        are bound to those that DPDK needs.

        Args:
            dpdk_build_config: A DPDK build configuration to test.
        """
        match dpdk_build_config.dpdk_location:
            case RemoteDPDKTreeLocation(dpdk_tree=dpdk_tree):
                self._set_remote_dpdk_tree_path(dpdk_tree)
            case LocalDPDKTreeLocation(dpdk_tree=dpdk_tree):
                self._copy_dpdk_tree(dpdk_tree)
            case RemoteDPDKTarballLocation(tarball=tarball):
                self._validate_remote_dpdk_tarball(tarball)
                self._prepare_and_extract_dpdk_tarball(tarball)
            case LocalDPDKTarballLocation(tarball=tarball):
                remote_tarball = self._copy_dpdk_tarball_to_remote(tarball)
                self._prepare_and_extract_dpdk_tarball(remote_tarball)

        match dpdk_build_config:
            case DPDKPrecompiledBuildConfiguration(precompiled_build_dir=build_dir):
                self._set_remote_dpdk_build_dir(build_dir)
            case DPDKUncompiledBuildConfiguration(build_options=build_options):
                self._configure_dpdk_build(build_options)
                self._build_dpdk()

        self.bind_ports_to_driver()

    def _tear_down_dpdk(self) -> None:
        """Reset DPDK variables and bind port driver to the OS driver."""
        self._env_vars = {}
        self.__remote_dpdk_tree_path = None
        self._remote_dpdk_build_dir = None
        self._dpdk_version = None
        self.compiler_version = None
        self.bind_ports_to_driver(for_dpdk=False)

    def _set_remote_dpdk_tree_path(self, dpdk_tree: PurePath):
        """Set the path to the remote DPDK source tree based on the provided DPDK location.

        Verify DPDK source tree existence on the SUT node, if exists sets the
        `_remote_dpdk_tree_path` property, otherwise sets nothing.

        Args:
            dpdk_tree: The path to the DPDK source tree directory.

        Raises:
            RemoteFileNotFoundError: If the DPDK source tree is expected to be on the SUT node but
                is not found.
            ConfigurationError: If the remote DPDK source tree specified is not a valid directory.
        """
        if not self.main_session.remote_path_exists(dpdk_tree):
            raise RemoteFileNotFoundError(
                f"Remote DPDK source tree '{dpdk_tree}' not found in SUT node."
            )
        if not self.main_session.is_remote_dir(dpdk_tree):
            raise ConfigurationError(f"Remote DPDK source tree '{dpdk_tree}' must be a directory.")

        self.__remote_dpdk_tree_path = dpdk_tree

    def _copy_dpdk_tree(self, dpdk_tree_path: Path) -> None:
        """Copy the DPDK source tree to the SUT.

        Args:
            dpdk_tree_path: The path to DPDK source tree on local filesystem.
        """
        self._logger.info(
            f"Copying DPDK source tree to SUT: '{dpdk_tree_path}' into '{self._remote_tmp_dir}'."
        )
        self.main_session.copy_dir_to(
            dpdk_tree_path,
            self._remote_tmp_dir,
            exclude=[".git", "*.o"],
            compress_format=TarCompressionFormat.gzip,
        )

        self.__remote_dpdk_tree_path = self.main_session.join_remote_path(
            self._remote_tmp_dir, PurePath(dpdk_tree_path).name
        )

    def _validate_remote_dpdk_tarball(self, dpdk_tarball: PurePath) -> None:
        """Validate the DPDK tarball on the SUT node.

        Args:
            dpdk_tarball: The path to the DPDK tarball on the SUT node.

        Raises:
            RemoteFileNotFoundError: If the `dpdk_tarball` is expected to be on the SUT node but is
                not found.
            ConfigurationError: If the `dpdk_tarball` is a valid path but not a valid tar archive.
        """
        if not self.main_session.remote_path_exists(dpdk_tarball):
            raise RemoteFileNotFoundError(f"Remote DPDK tarball '{dpdk_tarball}' not found in SUT.")
        if not self.main_session.is_remote_tarfile(dpdk_tarball):
            raise ConfigurationError(f"Remote DPDK tarball '{dpdk_tarball}' must be a tar archive.")

    def _copy_dpdk_tarball_to_remote(self, dpdk_tarball: Path) -> PurePath:
        """Copy the local DPDK tarball to the SUT node.

        Args:
            dpdk_tarball: The local path to the DPDK tarball.

        Returns:
            The path of the copied tarball on the SUT node.
        """
        self._logger.info(
            f"Copying DPDK tarball to SUT: '{dpdk_tarball}' into '{self._remote_tmp_dir}'."
        )
        self.main_session.copy_to(dpdk_tarball, self._remote_tmp_dir)
        return self.main_session.join_remote_path(self._remote_tmp_dir, dpdk_tarball.name)

    def _prepare_and_extract_dpdk_tarball(self, remote_tarball_path: PurePath) -> None:
        """Prepare the remote DPDK tree path and extract the tarball.

        This method extracts the remote tarball and sets the `_remote_dpdk_tree_path` property to
        the path of the extracted DPDK tree on the SUT node.

        Args:
            remote_tarball_path: The path to the DPDK tarball on the SUT node.
        """

        def remove_tarball_suffix(remote_tarball_path: PurePath) -> PurePath:
            """Remove the tarball suffix from the path.

            Args:
                remote_tarball_path: The path to the remote tarball.

            Returns:
                The path without the tarball suffix.
            """
            if len(remote_tarball_path.suffixes) > 1:
                if remote_tarball_path.suffixes[-2] == ".tar":
                    suffixes_to_remove = "".join(remote_tarball_path.suffixes[-2:])
                    return PurePath(str(remote_tarball_path).replace(suffixes_to_remove, ""))
            return remote_tarball_path.with_suffix("")

        tarball_top_dir = self.main_session.get_tarball_top_dir(remote_tarball_path)
        self.__remote_dpdk_tree_path = self.main_session.join_remote_path(
            remote_tarball_path.parent,
            tarball_top_dir or remove_tarball_suffix(remote_tarball_path),
        )

        self._logger.info(
            "Extracting DPDK tarball on SUT: "
            f"'{remote_tarball_path}' into '{self._remote_dpdk_tree_path}'."
        )
        self.main_session.extract_remote_tarball(
            remote_tarball_path,
            self._remote_dpdk_tree_path,
        )

    def _set_remote_dpdk_build_dir(self, build_dir: str):
        """Set the `remote_dpdk_build_dir` on the SUT.

        Check existence on the SUT node and sets the
        `remote_dpdk_build_dir` property by joining the `_remote_dpdk_tree_path` and `build_dir`.
        Otherwise, sets nothing.

        Args:
            build_dir: DPDK has been pre-built and the build directory is located
                in a subdirectory of `dpdk_tree` or `tarball` root directory.

        Raises:
            RemoteFileNotFoundError: If the `build_dir` is expected but does not exist on the SUT
                node.
        """
        remote_dpdk_build_dir = self.main_session.join_remote_path(
            self._remote_dpdk_tree_path, build_dir
        )
        if not self.main_session.remote_path_exists(remote_dpdk_build_dir):
            raise RemoteFileNotFoundError(
                f"Remote DPDK build dir '{remote_dpdk_build_dir}' not found in SUT node."
            )

        self._remote_dpdk_build_dir = PurePath(remote_dpdk_build_dir)

    def _configure_dpdk_build(self, dpdk_build_config: DPDKBuildOptionsConfiguration) -> None:
        """Populate common environment variables and set the DPDK build related properties.

        This method sets `compiler_version` for additional information and `remote_dpdk_build_dir`
        from DPDK build config name.

        Args:
            dpdk_build_config: A DPDK build configuration to test.
        """
        self._env_vars = {}
        self._env_vars.update(self.main_session.get_dpdk_build_env_vars(self.arch))
        if compiler_wrapper := dpdk_build_config.compiler_wrapper:
            self._env_vars["CC"] = f"'{compiler_wrapper} {dpdk_build_config.compiler.name}'"
        else:
            self._env_vars["CC"] = dpdk_build_config.compiler.name

        self.compiler_version = self.main_session.get_compiler_version(
            dpdk_build_config.compiler.name
        )

        build_dir_name = f"{self.arch}-{self.config.os}-{dpdk_build_config.compiler}"

        self._remote_dpdk_build_dir = self.main_session.join_remote_path(
            self._remote_dpdk_tree_path, build_dir_name
        )

    def _build_dpdk(self) -> None:
        """Build DPDK.

        Uses the already configured DPDK build configuration. Assumes that the
        `_remote_dpdk_tree_path` has already been set on the SUT node.
        """
        self.main_session.build_dpdk(
            self._env_vars,
            MesonArgs(default_library="static", enable_kmods=True, libdir="lib"),
            self._remote_dpdk_tree_path,
            self.remote_dpdk_build_dir,
        )

    def build_dpdk_app(self, app_name: str, **meson_dpdk_args: str | bool) -> PurePath:
        """Build one or all DPDK apps.

        Requires DPDK to be already built on the SUT node.

        Args:
            app_name: The name of the DPDK app to build.
                When `app_name` is ``all``, build all example apps.
            meson_dpdk_args: The arguments found in ``meson_options.txt`` in root DPDK directory.
                Do not use ``-D`` with them.

        Returns:
            The directory path of the built app. If building all apps, return
            the path to the examples directory (where all apps reside).
        """
        self.main_session.build_dpdk(
            self._env_vars,
            MesonArgs(examples=app_name, **meson_dpdk_args),  # type: ignore [arg-type]
            # ^^ https://github.com/python/mypy/issues/11583
            self._remote_dpdk_tree_path,
            self.remote_dpdk_build_dir,
            rebuild=True,
            timeout=self._app_compile_timeout,
        )

        if app_name == "all":
            return self.main_session.join_remote_path(self.remote_dpdk_build_dir, "examples")
        return self.main_session.join_remote_path(
            self.remote_dpdk_build_dir, "examples", f"dpdk-{app_name}"
        )

    def kill_cleanup_dpdk_apps(self) -> None:
        """Kill all dpdk applications on the SUT, then clean up hugepages."""
        if self._dpdk_kill_session and self._dpdk_kill_session.is_alive():
            # we can use the session if it exists and responds
            self._dpdk_kill_session.kill_cleanup_dpdk_apps(self.dpdk_prefix_list)
        else:
            # otherwise, we need to (re)create it
            self._dpdk_kill_session = self.create_session("dpdk_kill")
        self.dpdk_prefix_list = []

    def run_dpdk_app(
        self, app_path: PurePath, eal_params: EalParams, timeout: float = 30
    ) -> CommandResult:
        """Run DPDK application on the remote node.

        The application is not run interactively - the command that starts the application
        is executed and then the call waits for it to finish execution.

        Args:
            app_path: The remote path to the DPDK application.
            eal_params: EAL parameters to run the DPDK application with.
            timeout: Wait at most this long in seconds for `command` execution to complete.

        Returns:
            The result of the DPDK app execution.
        """
        return self.main_session.send_command(
            f"{app_path} {eal_params}", timeout, privileged=True, verify=True
        )

    def bind_ports_to_driver(self, for_dpdk: bool = True) -> None:
        """Bind all ports on the SUT to a driver.

        Args:
            for_dpdk: If :data:`True`, binds ports to os_driver_for_dpdk.
                If :data:`False`, binds to os_driver.
        """
        if self._ports_bound_to_dpdk == for_dpdk:
            return

        for port in self.ports:
            driver = port.os_driver_for_dpdk if for_dpdk else port.os_driver
            self.main_session.send_command(
                f"{self.path_to_devbind_script} -b {driver} --force {port.pci}",
                privileged=True,
                verify=True,
            )
        self._ports_bound_to_dpdk = for_dpdk
