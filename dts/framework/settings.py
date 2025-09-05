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

.. option:: --test-run-config-file
.. envvar:: DTS_TEST_RUN_CFG_FILE

    The path to the YAML configuration file of the test run.

.. option:: --nodes-config-file
.. envvar:: DTS_NODES_CFG_FILE

    The path to the YAML configuration file of the nodes.

.. option:: --tests-config-file
.. envvar:: DTS_TESTS_CFG_FILE

    The path to the YAML configuration file of the test suites.

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

.. option:: --dpdk-tree
.. envvar:: DTS_DPDK_TREE

    The path to the DPDK source tree directory to test. Cannot be used in conjunction with
    --tarball.

.. option:: --tarball, --snapshot
.. envvar:: DTS_DPDK_TARBALL

    The path to the DPDK source tarball to test. DPDK must be contained in a folder with the same
    name as the tarball file. Cannot be used in conjunction with --dpdk-tree.

.. option:: --remote-source
.. envvar:: DTS_REMOTE_SOURCE

    Set this option if either the DPDK source tree or tarball to be used are located on the SUT
    node. Can only be used with --dpdk-tree or --tarball.

.. option:: --precompiled-build-dir
.. envvar:: DTS_PRECOMPILED_BUILD_DIR

    Define the subdirectory under the DPDK tree root directory or tarball where the pre-compiled
    binaries are located.

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
from typing import Callable, NoReturn

from pydantic import ValidationError

from .config.test_run import (
    DPDKLocation,
    LocalDPDKTarballLocation,
    LocalDPDKTreeLocation,
    RemoteDPDKTarballLocation,
    RemoteDPDKTreeLocation,
    TestSuiteConfig,
)


@dataclass(slots=True)
class Settings:
    """Default framework-wide user settings.

    The defaults may be modified at the start of the run.
    """

    #:
    test_run_config_path: Path = Path(__file__).parent.parent.joinpath("test_run.yaml")
    #:
    nodes_config_path: Path = Path(__file__).parent.parent.joinpath("nodes.yaml")
    #:
    tests_config_path: Path | None = None
    #:
    output_dir: str = "output"
    #:
    timeout: float = 15
    #:
    verbose: bool = False
    #:
    dpdk_location: DPDKLocation | None = None
    #:
    precompiled_build_dir: str | None = None
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


def _get_env_var_name(action: Action | None) -> str | None:
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

    def error(self, message) -> NoReturn:
        """Augments :meth:`~argparse.ArgumentParser.error` with environment variable awareness."""
        for action in self._actions:
            if _is_from_env(action):
                action_name = _get_action_name(action)
                env_var_name = _get_env_var_name(action)
                assert (
                    env_var_name is not None
                ), "Action was set from environment, but no environment variable name was found."
                env_var_value = os.environ.get(env_var_name)

                message = message.replace(
                    f"argument {action_name}",
                    f"environment variable {env_var_name} (value: {env_var_value})",
                )

        print(f"{self.prog}: error: {message}\n", file=sys.stderr)
        self.exit(2, "For help and usage, " "run the command with the --help flag.\n")


class _EnvVarHelpFormatter(ArgumentDefaultsHelpFormatter):
    """Custom formatter to add environment variables to the help page."""

    def _get_help_string(self, action: Action) -> str | None:
        """Overrides :meth:`ArgumentDefaultsHelpFormatter._get_help_string`."""
        help = super()._get_help_string(action)

        env_var_name = _get_env_var_name(action)
        if env_var_name is not None:
            help = f"[{env_var_name}] {help}"

            env_var_value = os.environ.get(env_var_name)
            if env_var_value is not None:
                help = f"{help} (env value: {env_var_value})"

        return help


def _required_with_one_of(parser: _DTSArgumentParser, action: Action, *required_dests: str) -> None:
    """Verify that `action` is listed together with at least one of `required_dests`.

    Verify that when `action` is among the command-line arguments or
    environment variables, at least one of `required_dests` is also among
    the command-line arguments or environment variables.

    Args:
        parser: The custom ArgumentParser object which contains `action`.
        action: The action to be verified.
        *required_dests: Destination variable names of the required arguments.

    Raises:
        argparse.ArgumentTypeError: When none of the required_dest are defined.

    Example:
        We have ``--option1`` and we only want it to be a passed alongside
        either ``--option2`` or ``--option3`` (meaning if ``--option1`` is
        passed without either ``--option2`` or ``--option3``, that's an error).

        parser = _DTSArgumentParser()
        option1_arg = parser.add_argument('--option1', dest='option1', action='store_true')
        option2_arg = parser.add_argument('--option2', dest='option2', action='store_true')
        option2_arg = parser.add_argument('--option3', dest='option3', action='store_true')

        _required_with_one_of(parser, option1_arg, 'option2', 'option3')
    """
    if _is_action_in_args(action):
        for required_dest in required_dests:
            required_action = parser.find_action(required_dest)
            if required_action is None:
                continue

            if _is_action_in_args(required_action):
                return None

        raise argparse.ArgumentTypeError(
            f"The '{action.dest}' is required at least with one of '{', '.join(required_dests)}'."
        )


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
        "--test-run-config-file",
        default=SETTINGS.test_run_config_path,
        type=Path,
        help="The configuration file that describes the test cases and DPDK build options.",
        metavar="FILE_PATH",
        dest="test_run_config_path",
    )
    _add_env_var_to_action(action, "TEST_RUN_CFG_FILE")

    action = parser.add_argument(
        "--nodes-config-file",
        default=SETTINGS.nodes_config_path,
        type=Path,
        help="The configuration file that describes the SUT and TG nodes.",
        metavar="FILE_PATH",
        dest="nodes_config_path",
    )
    _add_env_var_to_action(action, "NODES_CFG_FILE")

    action = parser.add_argument(
        "--tests-config-file",
        default=SETTINGS.tests_config_path,
        type=Path,
        help="Configuration file used to override variable values inside specific test suites.",
        metavar="FILE_PATH",
        dest="tests_config_path",
    )
    _add_env_var_to_action(action, "TESTS_CFG_FILE")

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

    dpdk_build = parser.add_argument_group(
        "DPDK Build Options",
        description="Arguments in this group (and subgroup) will be applied to a "
        "DPDKLocation when the DPDK tree, tarball or revision will be provided, "
        "other arguments like remote source and build dir are optional. A DPDKLocation "
        "from settings are used instead of from config if construct successful.",
    )

    dpdk_source = dpdk_build.add_mutually_exclusive_group()
    action = dpdk_source.add_argument(
        "--dpdk-tree",
        help="The path to the DPDK source tree directory to test. Cannot be used in conjunction "
        "with --tarball.",
        metavar="DIR_PATH",
        dest="dpdk_tree_path",
    )
    _add_env_var_to_action(action, "DPDK_TREE")

    action = dpdk_source.add_argument(
        "--tarball",
        "--snapshot",
        help="The path to the DPDK source tarball to test. DPDK must be contained in a folder with "
        "the same name as the tarball file. Cannot be used in conjunction with --dpdk-tree.",
        metavar="FILE_PATH",
        dest="dpdk_tarball_path",
    )
    _add_env_var_to_action(action, "DPDK_TARBALL")

    action = dpdk_build.add_argument(
        "--remote-source",
        action="store_true",
        default=False,
        help="Set this option if either the DPDK source tree or tarball to be used are located on "
        "the SUT node. Can only be used with --dpdk-tree or --tarball.",
    )
    _add_env_var_to_action(action)
    _required_with_one_of(parser, action, "dpdk_tarball_path", "dpdk_tree_path")

    action = dpdk_build.add_argument(
        "--precompiled-build-dir",
        help="Define the subdirectory under the DPDK tree root directory or tarball where the "
        "pre-compiled binaries are located.",
        metavar="DIR_NAME",
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


def _process_dpdk_location(
    parser: _DTSArgumentParser,
    dpdk_tree: str | None,
    tarball: str | None,
    remote: bool,
) -> DPDKLocation | None:
    """Process and validate DPDK build arguments.

    Ensures that either `dpdk_tree` or `tarball` is provided. Validate existence and format of
    `dpdk_tree` or `tarball` on local filesystem, if `remote` is False. Constructs and returns
    any valid :class:`DPDKLocation` with the provided parameters if validation is successful.

    Args:
        parser: The instance of the arguments parser.
        dpdk_tree: The path to the DPDK source tree directory.
        tarball: The path to the DPDK tarball.
        remote: If :data:`True`, `dpdk_tree` or `tarball` is located on the SUT node, instead of the
            execution host.

    Returns:
        A DPDK location if construction is successful, otherwise None.
    """
    if dpdk_tree:
        action = parser.find_action("dpdk_tree", _is_from_env)

        try:
            if remote:
                return RemoteDPDKTreeLocation.model_validate({"dpdk_tree": dpdk_tree})
            else:
                return LocalDPDKTreeLocation.model_validate({"dpdk_tree": dpdk_tree})
        except ValidationError as e:
            print(
                "An error has occurred while validating the DPDK tree supplied in the "
                f"{'environment variable' if action else 'arguments'}:",
                file=sys.stderr,
            )
            print(e, file=sys.stderr)
            sys.exit(1)

    if tarball:
        action = parser.find_action("tarball", _is_from_env)

        try:
            if remote:
                return RemoteDPDKTarballLocation.model_validate({"tarball": tarball})
            else:
                return LocalDPDKTarballLocation.model_validate({"tarball": tarball})
        except ValidationError as e:
            print(
                "An error has occurred while validating the DPDK tarball supplied in the "
                f"{'environment variable' if action else 'arguments'}:",
                file=sys.stderr,
            )
            print(e, file=sys.stderr)
            sys.exit(1)

    return None


def _process_test_suites(
    parser: _DTSArgumentParser, args: list[list[str]]
) -> list[TestSuiteConfig]:
    """Process the given argument to a list of :class:`TestSuiteConfig` to execute.

    Args:
        parser: The instance of the arguments parser.
        args: The arguments to process. The args is a string from an environment variable
              or a list of from the user input containing tests suites with tests cases,
              each of which is a list of [test_suite, test_case, test_case, ...].

    Returns:
        A list of test suite configurations to execute.
    """
    action = parser.find_action("test_suites", _is_from_env)
    if action:
        # Environment variable in the form of "SUITE1 CASE1 CASE2, SUITE2 CASE1, SUITE3, ..."
        args = [suite_with_cases.split() for suite_with_cases in args[0][0].split(",")]

    try:
        return [
            TestSuiteConfig(test_suite=test_suite, test_cases=test_cases)
            for [test_suite, *test_cases] in args
        ]
    except ValidationError as e:
        print(
            "An error has occurred while validating the test suites supplied in the "
            f"{'environment variable' if action else 'arguments'}:",
            file=sys.stderr,
        )
        print(e, file=sys.stderr)
        sys.exit(1)


def get_settings() -> Settings:
    """Create new settings with inputs from the user.

    The inputs are taken from the command line and from environment variables.

    Returns:
        The new settings object.
    """
    parser = _get_parser()

    args = parser.parse_args()

    args.dpdk_location = _process_dpdk_location(
        parser, args.dpdk_tree_path, args.dpdk_tarball_path, args.remote_source
    )
    args.test_suites = _process_test_suites(parser, args.test_suites)

    kwargs = {k: v for k, v in vars(args).items() if hasattr(SETTINGS, k)}
    return Settings(**kwargs)
