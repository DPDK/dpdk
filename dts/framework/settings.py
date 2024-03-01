# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

"""Environment variables and command line arguments parsing.

This is a simple module utilizing the built-in argparse module to parse command line arguments,
augment them with values from environment variables and make them available across the framework.

The command line value takes precedence, followed by the environment variable value,
followed by the default value defined in this module.

The command line arguments along with the supported environment variables are:

.. option:: --config-file
.. envvar:: DTS_CFG_FILE

    The path to the YAML test run configuration file.

.. option:: --output-dir, --output
.. envvar:: DTS_OUTPUT_DIR

    The directory where DTS logs and results are saved.

.. option:: --compile-timeout
.. envvar:: DTS_COMPILE_TIMEOUT

    The timeout for compiling DPDK.

.. option:: -t, --timeout
.. envvar:: DTS_TIMEOUT

    The timeout for all DTS operation except for compiling DPDK.

.. option:: -v, --verbose
.. envvar:: DTS_VERBOSE

    Set to any value to enable logging everything to the console.

.. option:: -s, --skip-setup
.. envvar:: DTS_SKIP_SETUP

    Set to any value to skip building DPDK.

.. option:: --tarball, --snapshot, --git-ref
.. envvar:: DTS_DPDK_TARBALL

    The path to a DPDK tarball, git commit ID, tag ID or tree ID to test.

.. option:: --test-suite
.. envvar:: DTS_TEST_SUITES

        A test suite with test cases which may be specified multiple times.
        In the environment variable, the suites are joined with a comma.

.. option:: --re-run, --re_run
.. envvar:: DTS_RERUN

    Re-run each test case this many times in case of a failure.

The module provides one key module-level variable:

Attributes:
    SETTINGS: The module level variable storing framework-wide DTS settings.

Typical usage example::

  from framework.settings import SETTINGS
  foo = SETTINGS.foo
"""

import argparse
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .config import TestSuiteConfig
from .utils import DPDKGitTarball


@dataclass(slots=True)
class Settings:
    """Default framework-wide user settings.

    The defaults may be modified at the start of the run.
    """

    #:
    config_file_path: Path = Path(__file__).parent.parent.joinpath("conf.yaml")
    #:
    output_dir: str = "output"
    #:
    timeout: float = 15
    #:
    verbose: bool = False
    #:
    skip_setup: bool = False
    #:
    dpdk_tarball_path: Path | str = "dpdk.tar.xz"
    #:
    compile_timeout: float = 1200
    #:
    test_suites: list[TestSuiteConfig] = field(default_factory=list)
    #:
    re_run: int = 0


SETTINGS: Settings = Settings()


def _get_parser() -> argparse.ArgumentParser:
    """Create the argument parser for DTS.

    Command line options take precedence over environment variables, which in turn take precedence
    over default values.

    Returns:
        argparse.ArgumentParser: The configured argument parser with defined options.
    """

    def env_arg(env_var: str, default: Any) -> Any:
        """A helper function augmenting the argparse with environment variables.

        If the supplied environment variable is defined, then the default value
        of the argument is modified. This satisfies the priority order of
        command line argument > environment variable > default value.

        Args:
            env_var: Environment variable name.
            default: Default value.

        Returns:
            Environment variable or default value.
        """
        return os.environ.get(env_var) or default

    parser = argparse.ArgumentParser(
        description="Run DPDK test suites. All options may be specified with the environment "
        "variables provided in brackets. Command line arguments have higher priority.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "--config-file",
        default=env_arg("DTS_CFG_FILE", SETTINGS.config_file_path),
        type=Path,
        help="[DTS_CFG_FILE] The configuration file that describes the test cases, "
        "SUTs and targets.",
    )

    parser.add_argument(
        "--output-dir",
        "--output",
        default=env_arg("DTS_OUTPUT_DIR", SETTINGS.output_dir),
        help="[DTS_OUTPUT_DIR] Output directory where DTS logs and results are saved.",
    )

    parser.add_argument(
        "-t",
        "--timeout",
        default=env_arg("DTS_TIMEOUT", SETTINGS.timeout),
        type=float,
        help="[DTS_TIMEOUT] The default timeout for all DTS operations except for compiling DPDK.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=env_arg("DTS_VERBOSE", SETTINGS.verbose),
        help="[DTS_VERBOSE] Specify to enable verbose output, logging all messages "
        "to the console.",
    )

    parser.add_argument(
        "-s",
        "--skip-setup",
        action="store_true",
        default=env_arg("DTS_SKIP_SETUP", SETTINGS.skip_setup),
        help="[DTS_SKIP_SETUP] Specify to skip all setup steps on SUT and TG nodes.",
    )

    parser.add_argument(
        "--tarball",
        "--snapshot",
        "--git-ref",
        default=env_arg("DTS_DPDK_TARBALL", SETTINGS.dpdk_tarball_path),
        type=Path,
        help="[DTS_DPDK_TARBALL] Path to DPDK source code tarball or a git commit ID, "
        "tag ID or tree ID to test. To test local changes, first commit them, "
        "then use the commit ID with this option.",
    )

    parser.add_argument(
        "--compile-timeout",
        default=env_arg("DTS_COMPILE_TIMEOUT", SETTINGS.compile_timeout),
        type=float,
        help="[DTS_COMPILE_TIMEOUT] The timeout for compiling DPDK.",
    )

    parser.add_argument(
        "--test-suite",
        action="append",
        nargs="+",
        metavar=("TEST_SUITE", "TEST_CASES"),
        default=env_arg("DTS_TEST_SUITES", SETTINGS.test_suites),
        help="[DTS_TEST_SUITES] A list containing a test suite with test cases. "
        "The first parameter is the test suite name, and the rest are test case names, "
        "which are optional. May be specified multiple times. To specify multiple test suites in "
        "the environment variable, join the lists with a comma. "
        "Examples: "
        "--test-suite suite case case --test-suite suite case ... | "
        "DTS_TEST_SUITES='suite case case, suite case, ...' | "
        "--test-suite suite --test-suite suite case ... | "
        "DTS_TEST_SUITES='suite, suite case, ...'",
    )

    parser.add_argument(
        "--re-run",
        "--re_run",
        default=env_arg("DTS_RERUN", SETTINGS.re_run),
        type=int,
        help="[DTS_RERUN] Re-run each test case the specified number of times "
        "if a test failure occurs.",
    )

    return parser


def _process_test_suites(args: str | list[list[str]]) -> list[TestSuiteConfig]:
    """Process the given argument to a list of :class:`TestSuiteConfig` to execute.

    Args:
        args: The arguments to process. The args is a string from an environment variable
              or a list of from the user input containing tests suites with tests cases,
              each of which is a list of [test_suite, test_case, test_case, ...].

    Returns:
        A list of test suite configurations to execute.
    """
    if isinstance(args, str):
        # Environment variable in the form of "suite case case, suite case, suite, ..."
        args = [suite_with_cases.split() for suite_with_cases in args.split(",")]

    test_suites_to_run = []
    for suite_with_cases in args:
        test_suites_to_run.append(
            TestSuiteConfig(test_suite=suite_with_cases[0], test_cases=suite_with_cases[1:])
        )

    return test_suites_to_run


def get_settings() -> Settings:
    """Create new settings with inputs from the user.

    The inputs are taken from the command line and from environment variables.

    Returns:
        The new settings object.
    """
    parsed_args = _get_parser().parse_args()
    return Settings(
        config_file_path=parsed_args.config_file,
        output_dir=parsed_args.output_dir,
        timeout=parsed_args.timeout,
        verbose=parsed_args.verbose,
        skip_setup=parsed_args.skip_setup,
        dpdk_tarball_path=Path(
            Path(DPDKGitTarball(parsed_args.tarball, parsed_args.output_dir))
            if not os.path.exists(parsed_args.tarball)
            else Path(parsed_args.tarball)
        ),
        compile_timeout=parsed_args.compile_timeout,
        test_suites=_process_test_suites(parsed_args.test_suite),
        re_run=parsed_args.re_run,
    )
