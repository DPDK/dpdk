# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from pathlib import PurePath, PurePosixPath

from framework.config import Architecture
from framework.exception import DPDKBuildError, RemoteCommandExecutionError
from framework.settings import SETTINGS
from framework.utils import EnvVarsDict, MesonArgs

from .os_session import OSSession


class PosixSession(OSSession):
    """
    An intermediary class implementing the Posix compliant parts of
    Linux and other OS remote sessions.
    """

    @staticmethod
    def combine_short_options(**opts: bool) -> str:
        ret_opts = ""
        for opt, include in opts.items():
            if include:
                ret_opts = f"{ret_opts}{opt}"

        if ret_opts:
            ret_opts = f" -{ret_opts}"

        return ret_opts

    def guess_dpdk_remote_dir(self, remote_dir) -> PurePosixPath:
        remote_guess = self.join_remote_path(remote_dir, "dpdk-*")
        result = self.remote_session.send_command(f"ls -d {remote_guess} | tail -1")
        return PurePosixPath(result.stdout)

    def get_remote_tmp_dir(self) -> PurePosixPath:
        return PurePosixPath("/tmp")

    def get_dpdk_build_env_vars(self, arch: Architecture) -> dict:
        """
        Create extra environment variables needed for i686 arch build. Get information
        from the node if needed.
        """
        env_vars = {}
        if arch == Architecture.i686:
            # find the pkg-config path and store it in PKG_CONFIG_LIBDIR
            out = self.remote_session.send_command("find /usr -type d -name pkgconfig")
            pkg_path = ""
            res_path = out.stdout.split("\r\n")
            for cur_path in res_path:
                if "i386" in cur_path:
                    pkg_path = cur_path
                    break
            assert pkg_path != "", "i386 pkg-config path not found"

            env_vars["CFLAGS"] = "-m32"
            env_vars["PKG_CONFIG_LIBDIR"] = pkg_path

        return env_vars

    def join_remote_path(self, *args: str | PurePath) -> PurePosixPath:
        return PurePosixPath(*args)

    def copy_file(
        self,
        source_file: str | PurePath,
        destination_file: str | PurePath,
        source_remote: bool = False,
    ) -> None:
        self.remote_session.copy_file(source_file, destination_file, source_remote)

    def remove_remote_dir(
        self,
        remote_dir_path: str | PurePath,
        recursive: bool = True,
        force: bool = True,
    ) -> None:
        opts = PosixSession.combine_short_options(r=recursive, f=force)
        self.remote_session.send_command(f"rm{opts} {remote_dir_path}")

    def extract_remote_tarball(
        self,
        remote_tarball_path: str | PurePath,
        expected_dir: str | PurePath | None = None,
    ) -> None:
        self.remote_session.send_command(
            f"tar xfm {remote_tarball_path} "
            f"-C {PurePosixPath(remote_tarball_path).parent}",
            60,
        )
        if expected_dir:
            self.remote_session.send_command(f"ls {expected_dir}", verify=True)

    def build_dpdk(
        self,
        env_vars: EnvVarsDict,
        meson_args: MesonArgs,
        remote_dpdk_dir: str | PurePath,
        remote_dpdk_build_dir: str | PurePath,
        rebuild: bool = False,
        timeout: float = SETTINGS.compile_timeout,
    ) -> None:
        try:
            if rebuild:
                # reconfigure, then build
                self._logger.info("Reconfiguring DPDK build.")
                self.remote_session.send_command(
                    f"meson configure {meson_args} {remote_dpdk_build_dir}",
                    timeout,
                    verify=True,
                    env=env_vars,
                )
            else:
                # fresh build - remove target dir first, then build from scratch
                self._logger.info("Configuring DPDK build from scratch.")
                self.remove_remote_dir(remote_dpdk_build_dir)
                self.remote_session.send_command(
                    f"meson setup "
                    f"{meson_args} {remote_dpdk_dir} {remote_dpdk_build_dir}",
                    timeout,
                    verify=True,
                    env=env_vars,
                )

            self._logger.info("Building DPDK.")
            self.remote_session.send_command(
                f"ninja -C {remote_dpdk_build_dir}", timeout, verify=True, env=env_vars
            )
        except RemoteCommandExecutionError as e:
            raise DPDKBuildError(f"DPDK build failed when doing '{e.command}'.")

    def get_dpdk_version(self, build_dir: str | PurePath) -> str:
        out = self.remote_session.send_command(
            f"cat {self.join_remote_path(build_dir, 'VERSION')}", verify=True
        )
        return out.stdout
