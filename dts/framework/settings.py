# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2024 Arm Limited

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

.. option:: --tarball, --snapshot
.. envvar:: DTS_DPDK_TARBALL

    Path to DPDK source code tarball to test.

.. option:: --revision, --rev, --git-ref
.. envvar:: DTS_DPDK_REVISION_ID

    Git revision ID to test. Could be commit, tag, tree ID etc.
    To test local changes, first commit them, then use their commit ID.

.. option:: --test-suite
.. envvar:: DTS_TEST_SUITES

        A test suite with test cases which may be specified multiple times.
        In the environment variable, the suites are joined with a comma.

.. option:: --re-run, --re_run
.. envvar:: DTS_RERUN

    Re-run each test case this many times in case of a failure.

.. option:: --random-seed
.. envvar:: DTS_RANDOM_SEED

    The seed to use with the pseudo-random generator. If not specified, the configuration value is
    used instead. If that's also not specified, a random seed is generated.

The module provides one key module-level variable:

Attributes:
    SETTINGS: The module level variable storing framework-wide DTS settings.

Typical usage example::

  from framework.settings import SETTINGS
  foo = SETTINGS.foo
"""

import argparse
import os
import sys
from argparse import Action, ArgumentDefaultsHelpFormatter, _get_action_name
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from .config import TestSuiteConfig
from .exception import ConfigurationError
from .utils import DPDKGitTarball, get_commit_id


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
    dpdk_tarball_path: Path | str = ""
    #:
    compile_timeout: float = 1200
    #:
    test_suites: list[TestSuiteConfig] = field(default_factory=list)
    #:
    re_run: int = 0
    #:
    random_seed: int | None = None


SETTINGS: Settings = Settings()


#: Attribute name representing the env variable name to augment :class:`~argparse.Action` with.
_ENV_VAR_NAME_ATTR = "env_var_name"
#: Attribute name representing the value origin to augment :class:`~argparse.Action` with.
_IS_FROM_ENV_ATTR = "is_from_env"

#: The prefix to be added to all of the environment variables.
_ENV_PREFIX = "DTS_"


def _make_env_var_name(action: Action, env_var_name: str | None) -> str:
    """Make and assign an environment variable name to the given action."""
    env_var_name = f"{_ENV_PREFIX}{env_var_name or action.dest.upper()}"
    setattr(action, _ENV_VAR_NAME_ATTR, env_var_name)
    return env_var_name


def _get_env_var_name(action: Action) -> str | None:
    """Get the environment variable name of the given action."""
    return getattr(action, _ENV_VAR_NAME_ATTR, None)


def _set_is_from_env(action: Action) -> None:
    """Make the environment the given action's value origin."""
    setattr(action, _IS_FROM_ENV_ATTR, True)


def _is_from_env(action: Action) -> bool:
    """Check if the given action's value originated from the environment."""
    return getattr(action, _IS_FROM_ENV_ATTR, False)


def _is_action_in_args(action: Action) -> bool:
    """Check if the action is invoked in the command line arguments."""
    for option in action.option_strings:
        if option in sys.argv:
            return True
    return False


def _add_env_var_to_action(
    action: Action,
    env_var_name: str | None = None,
) -> None:
    """Add an argument with an environment variable to the parser."""
    env_var_name = _make_env_var_name(action, env_var_name)

    if not _is_action_in_args(action):
        env_var_value = os.environ.get(env_var_name)
        if env_var_value is not None:
            _set_is_from_env(action)
            sys.argv[1:0] = [action.format_usage(), env_var_value]


class _DTSArgumentParser(argparse.ArgumentParser):
    """ArgumentParser with a custom error message.

    This custom version of ArgumentParser changes the error message to accurately reflect the origin
    of the value of its arguments. If it was supplied through the command line nothing changes, but
    if it was supplied as an environment variable this is correctly communicated.
    """

    def find_action(
        self, action_dest: str, filter_fn: Callable[[Action], bool] | None = None
    ) -> Action | None:
        """Find and return an action by its destination variable name.

        Arguments:
            action_dest: the destination variable name of the action to find.
            filter_fn: if an action is found it is passed to this filter function, which must
                return a boolean value.
        """
        it = (action for action in self._actions if action.dest == action_dest)
        action = next(it, None)

        if action and filter_fn:
            return action if filter_fn(action) else None

        return action

    def error(self, message):
        """Augments :meth:`~argparse.ArgumentParser.error` with environment variable awareness."""
        for action in self._actions:
            if _is_from_env(action):
                action_name = _get_action_name(action)
                env_var_name = _get_env_var_name(action)
                env_var_value = os.environ.get(env_var_name)

                message = message.replace(
                    f"argument {action_name}",
                    f"environment variable {env_var_name} (value: {env_var_value})",
                )

        print(f"{self.prog}: error: {message}\n", file=sys.stderr)
        self.exit(2, "For help and usage, " "run the command with the --help flag.\n")


class _EnvVarHelpFormatter(ArgumentDefaultsHelpFormatter):
    """Custom formatter to add environment variables to the help page."""

    def _get_help_string(self, action):
        """Overrides :meth:`ArgumentDefaultsHelpFormatter._get_help_string`."""
        help = super()._get_help_string(action)

        env_var_name = _get_env_var_name(action)
        if env_var_name is not None:
            help = f"[{env_var_name}] {help}"

            env_var_value = os.environ.get(env_var_name)
            if env_var_value is not None:
                help = f"{help} (env value: {env_var_value})"

        return help


def _parse_tarball_path(file_path: str) -> Path:
    """Validate whether `file_path` is valid and return a Path object."""
    path = Path(file_path)
    if not path.exists() or not path.is_file():
        raise argparse.ArgumentTypeError("The file path provided is not a valid file")
    return path


def _parse_revision_id(rev_id: str) -> str:
    """Validate revision ID and retrieve corresponding commit ID."""
    try:
        return get_commit_id(rev_id)
    except ConfigurationError:
        raise argparse.ArgumentTypeError("The Git revision ID supplied is invalid or ambiguous")


def _get_parser() -> _DTSArgumentParser:
    """Create the argument parser for DTS.

    Command line options take precedence over environment variables, which in turn take precedence
    over default values.

    Returns:
        _DTSArgumentParser: The configured argument parser with defined options.
    """
    parser = _DTSArgumentParser(
        description="Run DPDK test suites. All options may be specified with the environment "
        "variables provided in brackets. Command line arguments have higher priority.",
        formatter_class=_EnvVarHelpFormatter,
        allow_abbrev=False,
    )

    action = parser.add_argument(
        "--config-file",
        default=SETTINGS.config_file_path,
        type=Path,
        help="The configuration file that describes the test cases, SUTs and targets.",
        metavar="FILE_PATH",
        dest="config_file_path",
    )
    _add_env_var_to_action(action, "CFG_FILE")

    action = parser.add_argument(
        "--output-dir",
        "--output",
        default=SETTINGS.output_dir,
        help="Output directory where DTS logs and results are saved.",
        metavar="DIR_PATH",
    )
    _add_env_var_to_action(action)

    action = parser.add_argument(
        "-t",
        "--timeout",
        default=SETTINGS.timeout,
        type=float,
        help="The default timeout for all DTS operations except for compiling DPDK.",
        metavar="SECONDS",
    )
    _add_env_var_to_action(action)

    action = parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=SETTINGS.verbose,
        help="Specify to enable verbose output, logging all messages to the console.",
    )
    _add_env_var_to_action(action)

    action = parser.add_argument(
        "-s",
        "--skip-setup",
        action="store_true",
        default=SETTINGS.skip_setup,
        help="Specify to skip all setup steps on SUT and TG nodes.",
    )
    _add_env_var_to_action(action)

    dpdk_source = parser.add_mutually_exclusive_group(required=True)

    action = dpdk_source.add_argument(
        "--tarball",
        "--snapshot",
        type=_parse_tarball_path,
        help="Path to DPDK source code tarball to test.",
        metavar="FILE_PATH",
        dest="dpdk_tarball_path",
    )
    _add_env_var_to_action(action, "DPDK_TARBALL")

    action = dpdk_source.add_argument(
        "--revision",
        "--rev",
        "--git-ref",
        type=_parse_revision_id,
        help="Git revision ID to test. Could be commit, tag, tree ID etc. "
        "To test local changes, first commit them, then use their commit ID.",
        metavar="ID",
        dest="dpdk_revision_id",
    )
    _add_env_var_to_action(action)

    action = parser.add_argument(
        "--compile-timeout",
        default=SETTINGS.compile_timeout,
        type=float,
        help="The timeout for compiling DPDK.",
        metavar="SECONDS",
    )
    _add_env_var_to_action(action)

    action = parser.add_argument(
        "--test-suite",
        action="append",
        nargs="+",
        metavar=("TEST_SUITE", "TEST_CASES"),
        default=SETTINGS.test_suites,
        help="A list containing a test suite with test cases. "
        "The first parameter is the test suite name, and the rest are test case names, "
        "which are optional. May be specified multiple times. To specify multiple test suites in "
        "the environment variable, join the lists with a comma. "
        "Examples: "
        "--test-suite suite case case --test-suite suite case ... | "
        "DTS_TEST_SUITES='suite case case, suite case, ...' | "
        "--test-suite suite --test-suite suite case ... | "
        "DTS_TEST_SUITES='suite, suite case, ...'",
        dest="test_suites",
    )
    _add_env_var_to_action(action)

    action = parser.add_argument(
        "--re-run",
        "--re_run",
        default=SETTINGS.re_run,
        type=int,
        help="Re-run each test case the specified number of times if a test failure occurs.",
        metavar="N_TIMES",
    )
    _add_env_var_to_action(action, "RERUN")

    action = parser.add_argument(
        "--random-seed",
        type=int,
        help="The seed to use with the pseudo-random generator. If not specified, the configuration"
        " value is used instead. If that's also not specified, a random seed is generated.",
        metavar="NUMBER",
    )
    _add_env_var_to_action(action)

    return parser


def _process_test_suites(
    parser: _DTSArgumentParser, args: list[list[str]]
) -> list[TestSuiteConfig]:
    """Process the given argument to a list of :class:`TestSuiteConfig` to execute.

    Args:
        args: The arguments to process. The args is a string from an environment variable
              or a list of from the user input containing tests suites with tests cases,
              each of which is a list of [test_suite, test_case, test_case, ...].

    Returns:
        A list of test suite configurations to execute.
    """
    if parser.find_action("test_suites", _is_from_env):
        # Environment variable in the form of "SUITE1 CASE1 CASE2, SUITE2 CASE1, SUITE3, ..."
        args = [suite_with_cases.split() for suite_with_cases in args[0][0].split(",")]

    return [TestSuiteConfig(test_suite, test_cases) for [test_suite, *test_cases] in args]


def get_settings() -> Settings:
    """Create new settings with inputs from the user.

    The inputs are taken from the command line and from environment variables.

    Returns:
        The new settings object.
    """
    parser = _get_parser()

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args()

    if args.dpdk_revision_id:
        args.dpdk_tarball_path = Path(DPDKGitTarball(args.dpdk_revision_id, args.output_dir))

    args.test_suites = _process_test_suites(parser, args.test_suites)

    kwargs = {k: v for k, v in vars(args).items() if hasattr(SETTINGS, k)}
    return Settings(**kwargs)
