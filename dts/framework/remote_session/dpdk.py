# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2025 Arm Limited

"""DPDK environment."""

import os
import time
from collections.abc import Iterable
from dataclasses import dataclass
from functools import cached_property
from pathlib import Path, PurePath
from typing import ClassVar, Final

from framework.config.test_run import (
    DPDKBuildConfiguration,
    DPDKBuildOptionsConfiguration,
    DPDKPrecompiledBuildConfiguration,
    DPDKRuntimeConfiguration,
    DPDKUncompiledBuildConfiguration,
    LocalDPDKTarballLocation,
    LocalDPDKTreeLocation,
    RemoteDPDKTarballLocation,
    RemoteDPDKTreeLocation,
)
from framework.exception import ConfigurationError, RemoteFileNotFoundError
from framework.logger import DTSLogger, get_dts_logger
from framework.params.eal import EalParams
from framework.remote_session.remote_session import CommandResult
from framework.testbed_model.cpu import LogicalCore, LogicalCoreCount, LogicalCoreList, lcore_filter
from framework.testbed_model.node import Node
from framework.testbed_model.os_session import OSSession
from framework.testbed_model.virtual_device import VirtualDevice
from framework.utils import MesonArgs, TarCompressionFormat


@dataclass(slots=True, frozen=True)
class DPDKBuildInfo:
    """Various versions and other information about a DPDK build.

    Attributes:
        dpdk_version: The DPDK version that was built.
        compiler_version: The version of the compiler used to build DPDK.
    """

    dpdk_version: str | None
    compiler_version: str | None


class DPDKBuildEnvironment:
    """Class handling a DPDK build environment."""

    config: DPDKBuildConfiguration
    _node: Node
    _session: OSSession
    _logger: DTSLogger
    _remote_dpdk_build_dir: PurePath | None
    _app_compile_timeout: float
    _remote_tmp_dpdk_tree_dir: ClassVar[str] = "dpdk"

    compiler_version: str | None

    def __init__(self, config: DPDKBuildConfiguration, node: Node) -> None:
        """DPDK build environment class constructor."""
        self.config = config
        self._node = node
        self._logger = get_dts_logger()
        self._session = node.create_session("dpdk_build")

        self._remote_dpdk_build_dir = None
        self._app_compile_timeout = 90

        self.compiler_version = None

    def setup(self) -> None:
        """Set up the DPDK build on the target node.

        DPDK setup includes setting all internals needed for the build, the copying of DPDK
        sources and then building DPDK or using the exist ones from the `dpdk_location`. The drivers
        are bound to those that DPDK needs.
        """
        if not isinstance(self.config.dpdk_location, RemoteDPDKTreeLocation):
            self._node.main_session.create_directory(self.remote_dpdk_tree_path)

        match self.config.dpdk_location:
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

        match self.config:
            case DPDKPrecompiledBuildConfiguration(precompiled_build_dir=build_dir):
                self._set_remote_dpdk_build_dir(build_dir)
            case DPDKUncompiledBuildConfiguration(build_options=build_options):
                self._configure_dpdk_build(build_options)
                self._build_dpdk()

    def teardown(self) -> None:
        """Teardown the DPDK build on the target node.

        Removes the DPDK tree and/or build directory/tarball depending on the configuration.
        """
        match self.config.dpdk_location:
            case LocalDPDKTreeLocation():
                self._node.main_session.remove_remote_dir(self.remote_dpdk_tree_path)
            case LocalDPDKTarballLocation(tarball=tarball):
                self._node.main_session.remove_remote_dir(self.remote_dpdk_tree_path)
                tarball_path = self._node.main_session.join_remote_path(
                    self._node.tmp_dir, tarball.name
                )
                self._node.main_session.remove_remote_file(tarball_path)

    def _set_remote_dpdk_tree_path(self, dpdk_tree: PurePath) -> None:
        """Set the path to the remote DPDK source tree based on the provided DPDK location.

        Verify DPDK source tree existence on the SUT node, if exists sets the
        `remote_dpdk_tree_path` property, otherwise sets nothing.

        Args:
            dpdk_tree: The path to the DPDK source tree directory.

        Raises:
            RemoteFileNotFoundError: If the DPDK source tree is expected to be on the SUT node but
                is not found.
            ConfigurationError: If the remote DPDK source tree specified is not a valid directory.
        """
        if not self._session.remote_path_exists(dpdk_tree):
            raise RemoteFileNotFoundError(
                f"Remote DPDK source tree '{dpdk_tree}' not found in SUT node."
            )
        if not self._session.is_remote_dir(dpdk_tree):
            raise ConfigurationError(f"Remote DPDK source tree '{dpdk_tree}' must be a directory.")

        self.remote_dpdk_tree_path = dpdk_tree

    def _copy_dpdk_tree(self, dpdk_tree_path: Path) -> None:
        """Copy the DPDK source tree to the SUT.

        Args:
            dpdk_tree_path: The path to DPDK source tree on local filesystem.
        """
        self._logger.info(
            f"Copying DPDK source tree to SUT: '{dpdk_tree_path}' into "
            f"'{self.remote_dpdk_tree_path}'."
        )
        self._session.copy_dir_to(
            dpdk_tree_path,
            self.remote_dpdk_tree_path,
            exclude=[".git", "*.o"],
            compress_format=TarCompressionFormat.gzip,
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
        if not self._session.remote_path_exists(dpdk_tarball):
            raise RemoteFileNotFoundError(f"Remote DPDK tarball '{dpdk_tarball}' not found in SUT.")
        if not self._session.is_remote_tarfile(dpdk_tarball):
            raise ConfigurationError(f"Remote DPDK tarball '{dpdk_tarball}' must be a tar archive.")

    def _copy_dpdk_tarball_to_remote(self, dpdk_tarball: Path) -> PurePath:
        """Copy the local DPDK tarball to the SUT node.

        Args:
            dpdk_tarball: The local path to the DPDK tarball.

        Returns:
            The path of the copied tarball on the SUT node.
        """
        self._logger.info(
            f"Copying DPDK tarball to SUT: '{dpdk_tarball}' into '{self._node.tmp_dir}'."
        )
        self._session.copy_to(dpdk_tarball, self._node.tmp_dir)
        return self._session.join_remote_path(self._node.tmp_dir, dpdk_tarball.name)

    def _prepare_and_extract_dpdk_tarball(self, remote_tarball_path: PurePath) -> None:
        """Prepare the remote DPDK tree path and extract the tarball.

        Args:
            remote_tarball_path: The path to the DPDK tarball on the SUT node.
        """
        self._logger.info(
            "Extracting DPDK tarball on SUT: "
            f"'{remote_tarball_path}' into '{self.remote_dpdk_tree_path}'."
        )
        self._session.extract_remote_tarball(
            remote_tarball_path,
            self.remote_dpdk_tree_path,
            strip_root_dir=True,
        )

    def _set_remote_dpdk_build_dir(self, build_dir: str) -> None:
        """Set the `remote_dpdk_build_dir` on the SUT.

        Check existence on the SUT node and sets the
        `remote_dpdk_build_dir` property by joining the `remote_dpdk_tree_path` and `build_dir`.
        Otherwise, sets nothing.

        Args:
            build_dir: DPDK has been pre-built and the build directory is located
                in a subdirectory of `dpdk_tree` or `tarball` root directory.

        Raises:
            RemoteFileNotFoundError: If the `build_dir` is expected but does not exist on the SUT
                node.
        """
        remote_dpdk_build_dir = self._session.join_remote_path(
            self.remote_dpdk_tree_path, build_dir
        )
        if not self._session.remote_path_exists(remote_dpdk_build_dir):
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
        self._env_vars.update(self._session.get_dpdk_build_env_vars(self._node.arch))
        if compiler_wrapper := dpdk_build_config.compiler_wrapper:
            self._env_vars["CC"] = f"'{compiler_wrapper} {dpdk_build_config.compiler.name}'"
        else:
            self._env_vars["CC"] = dpdk_build_config.compiler.name

        self.compiler_version = self._session.get_compiler_version(dpdk_build_config.compiler.name)

        build_dir_name = f"{self._node.arch}-{self._node.config.os}-{dpdk_build_config.compiler}"

        self._remote_dpdk_build_dir = self._session.join_remote_path(
            self.remote_dpdk_tree_path, build_dir_name
        )

    def _build_dpdk(self) -> None:
        """Build DPDK.

        Uses the already configured DPDK build configuration. Assumes that the
        `remote_dpdk_tree_path` has already been set on the SUT node.
        """
        self._session.build_dpdk(
            self._env_vars,
            MesonArgs(default_library="static", libdir="lib"),
            self.remote_dpdk_tree_path,
            self.remote_dpdk_build_dir,
        )

    def get_app(self, app_name: str) -> PurePath:
        """Retrieve path for a DPDK app."""
        return self._session.join_remote_path(self.remote_dpdk_build_dir, "app", f"dpdk-{app_name}")

    @cached_property
    def remote_dpdk_tree_path(self) -> PurePath:
        """The remote DPDK tree path."""
        return self._node.tmp_dir.joinpath(self._remote_tmp_dpdk_tree_dir)

    @property
    def remote_dpdk_build_dir(self) -> PurePath:
        """The remote DPDK build dir path."""
        if self._remote_dpdk_build_dir:
            return self._remote_dpdk_build_dir

        self._logger.warning(
            "Failed to get remote dpdk build dir because we don't know the "
            "location on the SUT node."
        )
        return PurePath("")

    @cached_property
    def dpdk_version(self) -> str | None:
        """Last built DPDK version."""
        return self._session.get_dpdk_version(self.remote_dpdk_tree_path)

    def get_dpdk_build_info(self) -> DPDKBuildInfo:
        """Get additional DPDK build information.

        Returns:
            The DPDK build information,
        """
        return DPDKBuildInfo(dpdk_version=self.dpdk_version, compiler_version=self.compiler_version)


class DPDKRuntimeEnvironment:
    """Class handling a DPDK runtime environment."""

    config: Final[DPDKRuntimeConfiguration]
    build: Final[DPDKBuildEnvironment | None]
    _node: Final[Node]
    _logger: Final[DTSLogger]

    timestamp: Final[str]
    _virtual_devices: list[VirtualDevice]
    _lcores: list[LogicalCore]

    _env_vars: dict
    _kill_session: OSSession | None
    prefix_list: list[str]

    def __init__(
        self,
        config: DPDKRuntimeConfiguration,
        node: Node,
        build_env: DPDKBuildEnvironment | None = None,
    ) -> None:
        """DPDK environment constructor.

        Args:
            config: The configuration of DPDK.
            node: The target node to manage a DPDK environment.
            build_env: The DPDK build environment, if any.
        """
        self.config = config
        self.build = build_env
        self._node = node
        self._logger = get_dts_logger()

        self.timestamp = f"{str(os.getpid())}_{time.strftime('%Y%m%d%H%M%S', time.localtime())}"
        self._virtual_devices = [VirtualDevice(vdev) for vdev in config.vdevs]

        self._lcores = node.lcores
        self._lcores = self.filter_lcores(LogicalCoreList(self.config.lcores))
        if LogicalCore(lcore=0, core=0, socket=0, node=0) in self._lcores:
            self._logger.warning(
                "First core being used; "
                "the first core is considered risky and should only be done by advanced users."
            )
        else:
            self._logger.info("Not using first core")

        self.prefix_list = []
        self._env_vars = {}
        self._ports_bound_to_dpdk = False
        self._kill_session = None

    def setup(self) -> None:
        """Set up the DPDK runtime on the target node."""
        if self.build:
            self.build.setup()

    def teardown(self) -> None:
        """Reset DPDK variables and bind port driver to the OS driver."""
        if self.build:
            self.build.teardown()

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
        return self._node.main_session.send_command(
            f"{app_path} {eal_params}", timeout, privileged=True, verify=True
        )

    def filter_lcores(
        self,
        filter_specifier: LogicalCoreCount | LogicalCoreList,
        ascending: bool = True,
    ) -> list[LogicalCore]:
        """Filter the node's logical cores that DTS can use.

        Logical cores that DTS can use are the ones that are present on the node, but filtered
        according to the test run configuration. The `filter_specifier` will filter cores from
        those logical cores.

        Args:
            filter_specifier: Two different filters can be used, one that specifies the number
                of logical cores per core, cores per socket and the number of sockets,
                and another one that specifies a logical core list.
            ascending: If :data:`True`, use cores with the lowest numerical id first and continue
                in ascending order. If :data:`False`, start with the highest id and continue
                in descending order. This ordering affects which sockets to consider first as well.

        Returns:
            The filtered logical cores.
        """
        self._logger.debug(f"Filtering {filter_specifier} from {self._lcores}.")
        return lcore_filter(
            self._lcores,
            filter_specifier,
            ascending,
        ).filter()

    def kill_cleanup_dpdk_apps(self) -> None:
        """Kill all dpdk applications on the SUT, then clean up hugepages."""
        if self._kill_session and self._kill_session.is_alive():
            # we can use the session if it exists and responds
            self._kill_session.kill_cleanup_dpdk_apps(self.prefix_list)
        else:
            # otherwise, we need to (re)create it
            self._kill_session = self._node.create_session("dpdk_kill")
        self.prefix_list = []

    def get_virtual_devices(self) -> Iterable[VirtualDevice]:
        """The available DPDK virtual devices."""
        return (v for v in self._virtual_devices)
