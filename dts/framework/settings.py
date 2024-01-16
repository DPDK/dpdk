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

.. option:: --test-cases
.. envvar:: DTS_TESTCASES

    A comma-separated list of test cases to execute. Unknown test cases will be silently ignored.

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
from collections.abc import Callable, Iterable, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, TypeVar

from .utils import DPDKGitTarball

_T = TypeVar("_T")


def _env_arg(env_var: str) -> Any:
    """A helper method augmenting the argparse Action with environment variables.

    If the supplied environment variable is defined, then the default value
    of the argument is modified. This satisfies the priority order of
    command line argument > environment variable > default value.

    Arguments with no values (flags) should be defined using the const keyword argument
    (True or False). When the argument is specified, it will be set to const, if not specified,
    the default will be stored (possibly modified by the corresponding environment variable).

    Other arguments work the same as default argparse arguments, that is using
    the default 'store' action.

    Returns:
          The modified argparse.Action.
    """

    class _EnvironmentArgument(argparse.Action):
        def __init__(
            self,
            option_strings: Sequence[str],
            dest: str,
            nargs: str | int | None = None,
            const: bool | None = None,
            default: Any = None,
            type: Callable[[str], _T | argparse.FileType | None] = None,
            choices: Iterable[_T] | None = None,
            required: bool = False,
            help: str | None = None,
            metavar: str | tuple[str, ...] | None = None,
        ) -> None:
            env_var_value = os.environ.get(env_var)
            default = env_var_value or default
            if const is not None:
                nargs = 0
                default = const if env_var_value else default
                type = None
                choices = None
                metavar = None
            super(_EnvironmentArgument, self).__init__(
                option_strings,
                dest,
                nargs=nargs,
                const=const,
                default=default,
                type=type,
                choices=choices,
                required=required,
                help=help,
                metavar=metavar,
            )

        def __call__(
            self,
            parser: argparse.ArgumentParser,
            namespace: argparse.Namespace,
            values: Any,
            option_string: str = None,
        ) -> None:
            if self.const is not None:
                setattr(namespace, self.dest, self.const)
            else:
                setattr(namespace, self.dest, values)

    return _EnvironmentArgument


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
    test_cases: list[str] = field(default_factory=list)
    #:
    re_run: int = 0


SETTINGS: Settings = Settings()


def _get_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run DPDK test suites. All options may be specified with the environment "
        "variables provided in brackets. Command line arguments have higher priority.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "--config-file",
        action=_env_arg("DTS_CFG_FILE"),
        default=SETTINGS.config_file_path,
        type=Path,
        help="[DTS_CFG_FILE] configuration file that describes the test cases, SUTs and targets.",
    )

    parser.add_argument(
        "--output-dir",
        "--output",
        action=_env_arg("DTS_OUTPUT_DIR"),
        default=SETTINGS.output_dir,
        help="[DTS_OUTPUT_DIR] Output directory where DTS logs and results are saved.",
    )

    parser.add_argument(
        "-t",
        "--timeout",
        action=_env_arg("DTS_TIMEOUT"),
        default=SETTINGS.timeout,
        type=float,
        help="[DTS_TIMEOUT] The default timeout for all DTS operations except for compiling DPDK.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action=_env_arg("DTS_VERBOSE"),
        default=SETTINGS.verbose,
        const=True,
        help="[DTS_VERBOSE] Specify to enable verbose output, logging all messages "
        "to the console.",
    )

    parser.add_argument(
        "-s",
        "--skip-setup",
        action=_env_arg("DTS_SKIP_SETUP"),
        const=True,
        help="[DTS_SKIP_SETUP] Specify to skip all setup steps on SUT and TG nodes.",
    )

    parser.add_argument(
        "--tarball",
        "--snapshot",
        "--git-ref",
        action=_env_arg("DTS_DPDK_TARBALL"),
        default=SETTINGS.dpdk_tarball_path,
        type=Path,
        help="[DTS_DPDK_TARBALL] Path to DPDK source code tarball or a git commit ID, "
        "tag ID or tree ID to test. To test local changes, first commit them, "
        "then use the commit ID with this option.",
    )

    parser.add_argument(
        "--compile-timeout",
        action=_env_arg("DTS_COMPILE_TIMEOUT"),
        default=SETTINGS.compile_timeout,
        type=float,
        help="[DTS_COMPILE_TIMEOUT] The timeout for compiling DPDK.",
    )

    parser.add_argument(
        "--test-cases",
        action=_env_arg("DTS_TESTCASES"),
        default="",
        help="[DTS_TESTCASES] Comma-separated list of test cases to execute. "
        "Unknown test cases will be silently ignored.",
    )

    parser.add_argument(
        "--re-run",
        "--re_run",
        action=_env_arg("DTS_RERUN"),
        default=SETTINGS.re_run,
        type=int,
        help="[DTS_RERUN] Re-run each test case the specified number of times "
        "if a test failure occurs",
    )

    return parser


def get_settings() -> Settings:
    """Create new settings with inputs from the user.

    The inputs are taken from the command line and from environment variables.
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
        test_cases=(parsed_args.test_cases.split(",") if parsed_args.test_cases else []),
        re_run=parsed_args.re_run,
    )
