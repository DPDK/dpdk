# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

"""Class to run blocking DPDK apps in the background.

The class won't automatically start the app. The start-up is done as part of the
:meth:`BlockingDPDKApp.wait_until_ready` method, which will return execution to the caller only
when the desired stdout has been returned by the app. Usually this is used to detect when the app
has been loaded and ready to be used.

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

from framework.params.eal import EalParams
from framework.remote_session.dpdk_shell import DPDKShell


class BlockingDPDKApp(DPDKShell):
    """Class to manage blocking DPDK apps."""

    def __init__(
        self,
        path: PurePath,
        name: str | None = None,
        privileged: bool = True,
        app_params: EalParams | str = "",
    ) -> None:
        """Constructor.

        Overrides :meth:`~.dpdk_shell.DPDKShell.__init__`.

        Args:
            path: Path relative to the DPDK build to the executable.
            name: Name to identify this application.
            privileged: Run as privileged user.
            app_params: The application parameters. If a string or an incomplete :class:`EalParams`
                object are passed, the EAL params are computed based on the current context.
        """
        if isinstance(app_params, str):
            eal_params = EalParams()
            eal_params.append_str(app_params)
            app_params = eal_params

        super().__init__(name, privileged, path, app_params)

    def wait_until_ready(self, end_token: str) -> None:
        """Start app and wait until ready.

        Args:
            end_token: The string at the end of a line that indicates the app is ready.
        """
        self._start_application(end_token)

    def close(self) -> None:
        """Close the application.

        Sends a SIGINT to close the application.
        """
        self.send_command("\x03")
        self._close()
