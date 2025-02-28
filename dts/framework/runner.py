# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

"""Test suite runner module.

The module is responsible for preparing DTS and running the test run.
"""

import os
import sys
import textwrap

from framework.config.common import ValidationContext
from framework.exception import ConfigurationError
from framework.test_run import TestRun
from framework.testbed_model.node import Node

from .config import (
    Configuration,
    load_config,
)
from .logger import DTSLogger, get_dts_logger
from .settings import SETTINGS
from .test_result import (
    DTSResult,
    Result,
)


class DTSRunner:
    """Test suite runner class."""

    _configuration: Configuration
    _logger: DTSLogger
    _result: DTSResult

    def __init__(self):
        """Initialize the instance with configuration, logger, result and string constants."""
        try:
            self._configuration = load_config(ValidationContext(settings=SETTINGS))
        except ConfigurationError as e:
            if e.__cause__:
                print(f"{e} Reason:", file=sys.stderr)
                print(file=sys.stderr)
                print(textwrap.indent(str(e.__cause__), prefix=" " * 2), file=sys.stderr)
            else:
                print(e, file=sys.stderr)
            sys.exit(e.severity)

        self._logger = get_dts_logger()
        if not os.path.exists(SETTINGS.output_dir):
            os.makedirs(SETTINGS.output_dir)
        self._logger.add_dts_root_logger_handlers(SETTINGS.verbose, SETTINGS.output_dir)
        self._result = DTSResult(SETTINGS.output_dir, self._logger)

    def run(self) -> None:
        """Run DTS.

        Prepare all the nodes ahead of the test run execution, which is subsequently run as
        configured.
        """
        nodes: list[Node] = []
        try:
            # check the python version of the server that runs dts
            self._check_dts_python_version()
            self._result.update_setup(Result.PASS)

            for node_config in self._configuration.nodes:
                nodes.append(Node(node_config))

            test_run_result = self._result.add_test_run(self._configuration.test_run)
            test_run = TestRun(
                self._configuration.test_run,
                self._configuration.tests_config,
                nodes,
                test_run_result,
            )
            test_run.spin()

        except Exception as e:
            self._logger.exception("An unexpected error has occurred.")
            self._result.add_error(e)
            # raise

        finally:
            try:
                self._logger.set_stage("post_run")
                for node in nodes:
                    node.close()
                self._result.update_teardown(Result.PASS)
            except Exception as e:
                self._logger.exception("The final cleanup of nodes failed.")
                self._result.update_teardown(Result.ERROR, e)

        # we need to put the sys.exit call outside the finally clause to make sure
        # that unexpected exceptions will propagate
        # in that case, the error that should be reported is the uncaught exception as
        # that is a severe error originating from the framework
        # at that point, we'll only have partial results which could be impacted by the
        # error causing the uncaught exception, making them uninterpretable
        self._exit_dts()

    def _check_dts_python_version(self) -> None:
        """Check the required Python version - v3.10."""
        if sys.version_info.major < 3 or (
            sys.version_info.major == 3 and sys.version_info.minor < 10
        ):
            self._logger.warning(
                "DTS execution node's python version is lower than Python 3.10, "
                "is deprecated and will not work in future releases."
            )
            self._logger.warning("Please use Python >= 3.10 instead.")

    def _exit_dts(self) -> None:
        """Process all errors and exit with the proper exit code."""
        self._result.process()

        if self._logger:
            self._logger.info("DTS execution has ended.")

        sys.exit(self._result.get_return_code())
