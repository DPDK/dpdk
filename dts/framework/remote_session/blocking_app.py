# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Class to run blocking apps in the background.

The class won't automatically start the app. The start-up is done as part of the
:meth:`BlockingApp.wait_until_ready` method, which will return execution to the caller only
when the desired stdout has been returned by the app. Usually this is used to detect when the app
has been loaded and ready to be used.

This module also provides the class :class:`BlockingDPDKApp` useful to run any DPDK app from the
DPDK build dir.

Example:
    ..code:: python

        pdump = BlockingDPDKApp(
            PurePath("app/dpdk-pdump"),
            app_params="--pdump 'port=0,queue=*,rx-dev=/tmp/rx-dev.pcap'"
        )
        pdump.wait_until_ready("65535") # start app

        # pdump is now ready to capture

        pdump.close() # stop/close app
"""

from pathlib import PurePath
from typing import Generic, TypeVar, cast

from typing_extensions import Self

from framework.context import get_ctx
from framework.params import Params
from framework.params.eal import EalParams
from framework.remote_session.dpdk_shell import compute_eal_params
from framework.remote_session.interactive_shell import InteractiveShell
from framework.testbed_model.node import Node

P = TypeVar("P", bound=Params)


class BlockingApp(InteractiveShell, Generic[P]):
    """Class to manage generic blocking apps."""

    _app_params: P

    def __init__(
        self,
        node: Node,
        path: PurePath,
        name: str | None = None,
        privileged: bool = False,
        app_params: P | str = "",
    ) -> None:
        """Constructor.

        Args:
            node: The node to run the app on.
            path: Path to the application on the node.
            name: Name to identify this application.
            privileged: Run as privileged user.
            app_params: The application parameters. Can be of any type inheriting :class:`Params` or
                a plain string.
        """
        if isinstance(app_params, str):
            params = Params()
            params.append_str(app_params)
            app_params = cast(P, params)

        self._path = path

        super().__init__(node, name, privileged, app_params)

    @property
    def path(self) -> PurePath:
        """The path of the DPDK app relative to the DPDK build folder."""
        return self._path

    def wait_until_ready(self, end_token: str) -> Self:
        """Start app and wait until ready.

        Args:
            end_token: The string at the end of a line that indicates the app is ready.

        Returns:
            Itself.
        """
        self.start_application(end_token)
        return self

    def close(self) -> None:
        """Close the application.

        Sends a SIGINT to close the application.
        """
        self.send_command("\x03")
        super().close()


PE = TypeVar("PE", bound=EalParams)


class BlockingDPDKApp(BlockingApp, Generic[PE]):
    """Class to manage blocking DPDK apps on the SUT."""

    _app_params: PE

    def __init__(
        self,
        path: PurePath,
        name: str | None = None,
        privileged: bool = True,
        app_params: PE | str = "",
    ) -> None:
        """Constructor.

        Args:
            path: Path relative to the DPDK build to the executable.
            name: Name to identify this application.
            privileged: Run as privileged user.
            app_params: The application parameters. If a string or an incomplete :class:`EalParams`
                object are passed, the EAL params are computed based on the current context.
        """
        if isinstance(app_params, str):
            eal_params = compute_eal_params()
            eal_params.append_str(app_params)
            app_params = cast(PE, eal_params)
        else:
            app_params = cast(PE, compute_eal_params(app_params))

        node = get_ctx().sut_node
        path = PurePath(get_ctx().dpdk_build.remote_dpdk_build_dir).joinpath(self.path)

        super().__init__(node, path, name, privileged, app_params)
