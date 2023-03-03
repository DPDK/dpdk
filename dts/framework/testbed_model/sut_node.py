# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.

import os
import tarfile
from pathlib import PurePath

from framework.config import BuildTargetConfiguration, NodeConfiguration
from framework.settings import SETTINGS
from framework.utils import EnvVarsDict, MesonArgs

from .node import Node


class SutNode(Node):
    """
    A class for managing connections to the System under Test, providing
    methods that retrieve the necessary information about the node (such as
    CPU, memory and NIC details) and configuration capabilities.
    Another key capability is building DPDK according to given build target.
    """

    _build_target_config: BuildTargetConfiguration | None
    _env_vars: EnvVarsDict
    _remote_tmp_dir: PurePath
    __remote_dpdk_dir: PurePath | None
    _dpdk_version: str | None
    _app_compile_timeout: float

    def __init__(self, node_config: NodeConfiguration):
        super(SutNode, self).__init__(node_config)
        self._build_target_config = None
        self._env_vars = EnvVarsDict()
        self._remote_tmp_dir = self.main_session.get_remote_tmp_dir()
        self.__remote_dpdk_dir = None
        self._dpdk_version = None
        self._app_compile_timeout = 90

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

    def _guess_dpdk_remote_dir(self) -> PurePath:
        return self.main_session.guess_dpdk_remote_dir(self._remote_tmp_dir)

    def _set_up_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> None:
        """
        Setup DPDK on the SUT node.
        """
        self._configure_build_target(build_target_config)
        self._copy_dpdk_tarball()
        self._build_dpdk()

    def _configure_build_target(
        self, build_target_config: BuildTargetConfiguration
    ) -> None:
        """
        Populate common environment variables and set build target config.
        """
        self._env_vars = EnvVarsDict()
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
        self.main_session.copy_file(SETTINGS.dpdk_tarball_path, self._remote_tmp_dir)

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
