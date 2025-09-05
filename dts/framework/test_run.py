# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Arm Limited

r"""Test run module.

The test run is implemented as a finite state machine which maintains a globally accessible
:class:`~.context.Context` and each state implements :class:`State`.

To spin up the test run state machine call :meth:`~TestRun.spin`.

The following graph represents all the states and steps of the state machine. Each node represents a
state labelled with the initials, e.g. ``TRS`` is represented by :class:`TestRunSetup`. States
represented by a double green circle are looping states. These states are only exited through:

    * **next** which progresses to the next test suite/case.
    * **end** which indicates that no more test suites/cases are available and
      the loop is terminated.

Red dashed links represent the path taken when an exception is
raised in the origin state. If a state does not have one, then the execution progresses as usual.
When :class:`~.exception.InternalError` is raised in any state, the state machine execution is
immediately terminated.
Orange dashed links represent exceptional conditions. Test suites and cases can be ``blocked`` or
``skipped`` in the following conditions:

    * If a *blocking* test suite fails, the ``blocked`` flag is raised.
    * If the user sends a ``SIGINT`` signal, the ``blocked`` flag is raised.
    * If a test suite and/or test case requires a capability unsupported by the test run, then this
      is ``skipped`` and the state restarts from the beginning.

Finally, test cases **retry** when they fail and DTS is configured to re-run.

.. only:: not graphviz

    ::

        +-----------------+---------------------------+-----------------------------------+
        | Current State   | Next Possible State       | Notes                             |
        +-----------------+---------------------------+-----------------------------------+
        | TRS             | TRE, TRT                  | Normal transition                 |
        | TRE             | TRT, TSS, TRE (self-loop) | "end" → TRT, "next" → TSS,        |
        |                 |                           | "next (skipped)" → TRE            |
        | TRT             | exit                      | Normal transition                 |
        | TSS             | TSE, TST                  | "next" → TSE, normal flow to TST  |
        | TSE             | TST, TCS, TSE (self-loop) | "end" → TST, "next" → TCS,        |
        |                 |                           | "next (blocked, skipped)" → TSE   |
        | TST             | TRE                       | Normal transition                 |
        | TCS             | TCE                       | Normal transition                 |
        | TCE             | TCT, TCE (self-loop)      | "next" → TCT, "retry" → TCE       |
        | TCT             | TSE                       | Normal transition                 |
        | InternalError   | exit                      | Error handling                    |
        | exit            | -                         | Final state                       |
        +-----------------+---------------------------+-----------------------------------+

.. only:: graphviz

    .. digraph:: test_run_fsm

        bgcolor=transparent
        nodesep=0.5
        ranksep=0.3

        node [fontname="sans-serif" fixedsize="true" width="0.7"]
        edge [fontname="monospace" color="gray30" fontsize=12]
        node [shape="circle"] "TRS" "TRT" "TSS" "TST" "TCS" "TCT"

        node [shape="doublecircle" style="bold" color="darkgreen"] "TRE" "TSE" "TCE"

        node [style="solid" shape="plaintext" fontname="monospace"
            fontsize=12 fixedsize="false"] "exit"

        "TRS" -> "TRE"
        "TRE":e -> "TRT":w [taillabel="end" labeldistance=1.5 labelangle=45]
        "TRT" -> "exit" [style="solid" color="gray30"]

        "TRE" -> "TSS" [headlabel="next" labeldistance=3 labelangle=320]
        "TSS" -> "TSE"
        "TSE" -> "TST" [label="end"]
        "TST" -> "TRE"

        "TSE" -> "TCS" [headlabel="next" labeldistance=3 labelangle=320]
        "TCS" -> "TCE" -> "TCT" -> "TSE":se


        edge [fontcolor="orange", color="orange" style="dashed"]
        "TRE":sw -> "TSE":nw [taillabel="next\n(blocked)" labeldistance=13]
        "TSE":ne -> "TRE" [taillabel="end\n(blocked)" labeldistance=7.5 labelangle=345]
        "TRE":w -> "TRE":nw [headlabel="next\n(skipped)" labeldistance=4]
        "TSE":e -> "TSE":e [taillabel="next\n(blocked)\n(skipped)" labelangle=325 labeldistance=7.5]
        "TCE":e -> "TCE":e [taillabel="retry" labelangle=5 labeldistance=2.5]

        edge [fontcolor="crimson" color="crimson"]
        "TRS" -> "TRT"
        "TSS":w -> "TST":n
        "TCS" -> "TCT"

        node [fontcolor="crimson" color="crimson"]
        "InternalError" -> "exit":ew
"""

import random
from collections import deque
from collections.abc import Iterable
from dataclasses import dataclass
from functools import cached_property
from types import MethodType
from typing import ClassVar, Protocol, Union

from framework.config.test_run import TestRunConfiguration
from framework.context import Context, init_ctx
from framework.exception import InternalError, SkippedTestException, TestCaseVerifyError
from framework.logger import DTSLogger, get_dts_logger
from framework.remote_session.dpdk import DPDKBuildEnvironment, DPDKRuntimeEnvironment
from framework.settings import SETTINGS
from framework.test_result import Result, ResultNode, TestRunResult
from framework.test_suite import BaseConfig, TestCase, TestSuite
from framework.testbed_model.capability import (
    Capability,
    get_supported_capabilities,
    test_if_supported,
)
from framework.testbed_model.node import Node
from framework.testbed_model.topology import PortLink, Topology
from framework.testbed_model.traffic_generator import create_traffic_generator

TestScenario = tuple[type[TestSuite], BaseConfig, deque[type[TestCase]]]


class TestRun:
    r"""A class representing a test run.

    The class is responsible for running tests on testbeds defined in the test run configuration.
    Each setup or teardown of each stage is recorded in a :class:`~framework.test_result.DTSResult`
    or one of its subclasses. The test case results are also recorded.

    If an error occurs, the current stage is aborted, the error is recorded, everything in
    the inner stages is marked as blocked and the run continues in the next iteration
    of the same stage. The return code is the highest `severity` of all
    :class:`~.framework.exception.DTSError`\s.

    Example:
        An error occurs in a test suite setup. The current test suite is aborted,
        all its test cases are marked as blocked and the run continues
        with the next test suite. If the errored test suite was the last one in the
        given test run, the next test run begins.

    Attributes:
        config: The test run configuration.
        logger: A reference to the current logger.
        state: The current state of the state machine.
        ctx: The test run's runtime context.
        result: The test run's execution result.
        selected_tests: The test suites and cases selected in this test run.
        blocked: :data:`True` if the test run execution has been blocked.
        remaining_tests: The remaining tests in the execution of the test run.
        remaining_test_cases: The remaining test cases in the execution of a test suite within the
            test run's state machine.
        supported_capabilities: All the capabilities supported by this test run.
    """

    config: TestRunConfiguration
    logger: DTSLogger

    state: Union["State", None]
    ctx: Context
    result: TestRunResult
    selected_tests: list[TestScenario]

    blocked: bool
    remaining_tests: deque[TestScenario]
    remaining_test_cases: deque[type[TestCase]]
    supported_capabilities: set[Capability]

    def __init__(
        self,
        config: TestRunConfiguration,
        tests_config: dict[str, BaseConfig],
        nodes: Iterable[Node],
        result: TestRunResult,
    ) -> None:
        """Test run constructor.

        Args:
            config: The test run's own configuration.
            tests_config: The test run's test suites configurations.
            nodes: A reference to all the available nodes.
            result: A reference to the test run result object.
        """
        self.config = config
        self.logger = get_dts_logger()

        sut_node = next(n for n in nodes if n.name == config.system_under_test_node)
        tg_node = next(n for n in nodes if n.name == config.traffic_generator_node)

        topology = Topology.from_port_links(
            PortLink(sut_node.ports_by_name[link.sut_port], tg_node.ports_by_name[link.tg_port])
            for link in self.config.port_topology
        )

        dpdk_build_env = DPDKBuildEnvironment(config.dpdk.build, sut_node)
        dpdk_runtime_env = DPDKRuntimeEnvironment(config.dpdk, sut_node, dpdk_build_env)
        traffic_generator = create_traffic_generator(config.traffic_generator, tg_node)

        self.ctx = Context(
            sut_node, tg_node, topology, dpdk_build_env, dpdk_runtime_env, traffic_generator
        )
        self.result = result
        self.selected_tests = list(self.config.filter_tests(tests_config))
        self.blocked = False
        self.remaining_tests = deque()
        self.remaining_test_cases = deque()
        self.supported_capabilities = set()

        self.state = TestRunSetup(self, result)

    @cached_property
    def required_capabilities(self) -> set[Capability]:
        """The capabilities required to run this test run in its totality."""
        caps = set()

        for test_suite, _, test_cases in self.selected_tests:
            caps.update(test_suite.required_capabilities)
            for test_case in test_cases:
                caps.update(test_case.required_capabilities)

        return caps

    def spin(self) -> None:
        """Spin the internal state machine that executes the test run."""
        self.logger.info(f"Running test run with SUT '{self.ctx.sut_node.name}'.")

        while self.state is not None:
            try:
                self.state.before()
                next_state = self.state.next()
            except (KeyboardInterrupt, Exception) as e:
                next_state = self.state.handle_exception(e)
            finally:
                self.state.after()
            if next_state is not None:
                self.logger.debug(
                    f"FSM - moving from '{self.state.logger_name}' to '{next_state.logger_name}'"
                )
            self.state = next_state

    def init_random_seed(self) -> None:
        """Initialize the random seed to use for the test run."""
        seed = self.config.random_seed or random.randrange(0xFFFF_FFFF)
        self.logger.info(f"Initializing with random seed {seed}.")
        random.seed(seed)


class State(Protocol):
    """Protocol indicating the state of the test run."""

    logger_name: ClassVar[str]
    test_run: TestRun
    result: TestRunResult | ResultNode

    def before(self) -> None:
        """Hook before the state is processed."""
        self.logger.set_stage(self.logger_name)

    def after(self) -> None:
        """Hook after the state is processed."""
        return

    @property
    def description(self) -> str:
        """State description."""

    @cached_property
    def logger(self) -> DTSLogger:
        """A reference to the root logger."""
        return get_dts_logger()

    def next(self) -> Union["State", None]:
        """Next state."""

    def on_error(self, ex: BaseException) -> Union["State", None]:
        """Next state on error."""

    def handle_exception(self, ex: BaseException) -> Union["State", None]:
        """Handles an exception raised by `next`."""
        next_state = self.on_error(Exception(ex))

        match ex:
            case InternalError():
                self.logger.error(
                    f"A CRITICAL ERROR has occurred during {self.description}. "
                    "Unrecoverable state reached, shutting down."
                )
                raise
            case KeyboardInterrupt():
                self.logger.info(
                    f"{self.description.capitalize()} INTERRUPTED by user! "
                    "Shutting down gracefully."
                )
                self.test_run.blocked = True
            case _:
                self.logger.error(f"An unexpected ERROR has occurred during {self.description}.")
                self.logger.exception(ex)

        return next_state


@dataclass
class TestRunSetup(State):
    """Test run setup."""

    logger_name: ClassVar[str] = "test_run_setup"
    test_run: TestRun
    result: TestRunResult

    @property
    def description(self) -> str:
        """State description."""
        return "test run setup"

    def next(self) -> State | None:
        """Process state and return the next one."""
        test_run = self.test_run
        init_ctx(test_run.ctx)

        self.logger.info(f"Running on SUT node '{test_run.ctx.sut_node.name}'.")
        test_run.init_random_seed()
        test_run.remaining_tests = deque(test_run.selected_tests)

        test_run.ctx.sut_node.setup()
        test_run.ctx.tg_node.setup()
        test_run.ctx.dpdk.setup()
        test_run.ctx.topology.setup()

        if test_run.config.use_virtual_functions:
            test_run.ctx.topology.instantiate_vf_ports()

        test_run.ctx.topology.configure_ports("sut", "dpdk")
        test_run.ctx.tg.setup(test_run.ctx.topology)

        self.result.ports = [
            port.to_dict()
            for port in test_run.ctx.topology.sut_ports + test_run.ctx.topology.tg_ports
        ]
        self.result.sut_session_info = test_run.ctx.sut_node.node_info
        self.result.dpdk_build_info = test_run.ctx.dpdk_build.get_dpdk_build_info()

        self.logger.debug(f"Found capabilities to check: {test_run.required_capabilities}")
        test_run.supported_capabilities = get_supported_capabilities(
            test_run.ctx.sut_node, test_run.ctx.topology, test_run.required_capabilities
        )
        return TestRunExecution(test_run, self.result)

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.test_run.result.add_error(ex)
        return TestRunTeardown(self.test_run, self.result)


@dataclass
class TestRunExecution(State):
    """Test run execution."""

    logger_name: ClassVar[str] = "test_run"
    test_run: TestRun
    result: TestRunResult

    @property
    def description(self) -> str:
        """State description."""
        return "test run execution"

    def next(self) -> State | None:
        """Next state."""
        test_run = self.test_run
        try:
            test_suite_class, test_config, test_run.remaining_test_cases = (
                test_run.remaining_tests.popleft()
            )

            test_suite = test_suite_class(test_config)
            test_suite_result = self.result.test_suites.add_child(test_suite.name)
            if test_run.blocked:
                test_suite_result.mark_result_as(Result.BLOCK)
                self.logger.warning(f"Test suite '{test_suite.name}' was BLOCKED.")
                # Continue to allow the rest to mark as blocked, no need to setup.
                return TestSuiteExecution(test_run, test_suite, test_suite_result)

            try:
                test_if_supported(test_suite_class, test_run.supported_capabilities)
            except SkippedTestException as e:
                self.logger.info(f"Test suite '{test_suite.name}' execution SKIPPED: {e}")
                test_suite_result.mark_result_as(Result.SKIP, e)

                return self

            test_run.ctx.local.reset()
            test_run.ctx.local.current_test_suite = test_suite
            return TestSuiteSetup(test_run, test_suite, test_suite_result)
        except IndexError:
            # No more test suites. We are done here.
            return TestRunTeardown(test_run, self.result)

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.test_run.result.add_error(ex)
        return TestRunTeardown(self.test_run, self.result)


@dataclass
class TestRunTeardown(State):
    """Test run teardown."""

    logger_name: ClassVar[str] = "test_run_teardown"
    test_run: TestRun
    result: TestRunResult

    @property
    def description(self) -> str:
        """State description."""
        return "test run teardown"

    def next(self) -> State | None:
        """Next state."""
        if self.test_run.config.use_virtual_functions:
            self.test_run.ctx.topology.delete_vf_ports()

        self.test_run.ctx.shell_pool.terminate_current_pool()
        self.test_run.ctx.tg.teardown()
        self.test_run.ctx.topology.teardown()
        self.test_run.ctx.dpdk.teardown()
        self.test_run.ctx.tg_node.teardown()
        self.test_run.ctx.sut_node.teardown()
        return None

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.test_run.result.add_error(ex)
        self.logger.warning(
            "The environment may have not been cleaned up correctly. "
            "The subsequent tests could be affected!"
        )
        return None


@dataclass
class TestSuiteState(State):
    """A test suite state template."""

    test_run: TestRun
    test_suite: TestSuite
    result: ResultNode


@dataclass
class TestSuiteSetup(TestSuiteState):
    """Test suite setup."""

    logger_name: ClassVar[str] = "test_suite_setup"

    def before(self) -> None:
        """Hook before the state is processed."""
        super().before()
        self.logger.set_custom_log_file(self.test_suite.name)

    @property
    def description(self) -> str:
        """State description."""
        return f"test suite '{self.test_suite.name}' setup"

    def next(self) -> State | None:
        """Next state."""
        self.test_run.ctx.shell_pool.start_new_pool()
        sut_ports_drivers = self.test_suite.sut_ports_drivers or "dpdk"
        self.test_run.ctx.topology.configure_ports("sut", sut_ports_drivers)

        self.test_suite.set_up_suite()
        self.result.mark_step_as("setup", Result.PASS)
        return TestSuiteExecution(
            test_run=self.test_run,
            test_suite=self.test_suite,
            result=self.result,
        )

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.result.mark_step_as("setup", Result.ERROR, ex)
        return TestSuiteTeardown(self.test_run, self.test_suite, self.result)


@dataclass
class TestSuiteExecution(TestSuiteState):
    """Test suite execution."""

    logger_name: ClassVar[str] = "test_suite"

    @property
    def description(self) -> str:
        """State description."""
        return f"test suite '{self.test_suite.name}' execution"

    def next(self) -> State | None:
        """Next state."""
        try:
            test_case = self.test_run.remaining_test_cases.popleft()
            test_case_result = self.result.add_child(test_case.name)
            if self.test_run.blocked:
                test_case_result.mark_result_as(Result.BLOCK)
                self.logger.warning(f"Test case '{test_case.name}' execution was BLOCKED.")
                return TestSuiteExecution(
                    test_run=self.test_run,
                    test_suite=self.test_suite,
                    result=self.result,
                )

            try:
                test_if_supported(test_case, self.test_run.supported_capabilities)
            except SkippedTestException as e:
                self.logger.info(f"Test case '{test_case.name}' execution SKIPPED: {e}")
                test_case_result.mark_result_as(Result.SKIP, e)
                return self

            self.test_run.ctx.local.current_test_case = test_case
            return TestCaseSetup(
                self.test_run,
                self.test_suite,
                test_case,
                test_case_result,
            )
        except IndexError:
            if self.test_run.blocked and self.result.get_overall_result() == Result.BLOCK:
                # Skip teardown if the test case AND suite were blocked.
                return TestRunExecution(self.test_run, self.test_run.result)
            else:
                # No more test cases. We are done here.
                return TestSuiteTeardown(self.test_run, self.test_suite, self.result)

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.test_run.result.add_error(ex)
        return TestSuiteTeardown(self.test_run, self.test_suite, self.result)


@dataclass
class TestSuiteTeardown(TestSuiteState):
    """Test suite teardown."""

    logger_name: ClassVar[str] = "test_suite_teardown"

    @property
    def description(self) -> str:
        """State description."""
        return f"test suite '{self.test_suite.name}' teardown"

    def next(self) -> State | None:
        """Next state."""
        self.test_suite.tear_down_suite()
        self.test_run.ctx.dpdk.kill_cleanup_dpdk_apps()
        self.test_run.ctx.shell_pool.terminate_current_pool()
        self.result.mark_step_as("teardown", Result.PASS)
        return TestRunExecution(self.test_run, self.test_run.result)

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.logger.warning(
            "The environment may have not been cleaned up correctly. "
            "The subsequent tests could be affected!"
        )
        self.result.mark_step_as("teardown", Result.ERROR, ex)
        return TestRunExecution(self.test_run, self.test_run.result)

    def after(self) -> None:
        """Hook after state is processed."""
        if (
            self.result.get_overall_result() in [Result.FAIL, Result.ERROR]
            and self.test_suite.is_blocking
        ):
            self.logger.warning(
                f"An error occurred within blocking {self.test_suite.name}. "
                "The remaining test suites will be skipped."
            )
            self.test_run.blocked = True
        self.logger.set_custom_log_file(None)


@dataclass
class TestCaseState(State):
    """A test case state template."""

    test_run: TestRun
    test_suite: TestSuite
    test_case: type[TestCase]
    result: ResultNode


@dataclass
class TestCaseSetup(TestCaseState):
    """Test case setup."""

    logger_name: ClassVar[str] = "test_case_setup"

    @property
    def description(self) -> str:
        """State description."""
        return f"test case '{self.test_case.name}' setup"

    def next(self) -> State | None:
        """Next state."""
        self.test_run.ctx.shell_pool.start_new_pool()
        sut_ports_drivers = (
            self.test_case.sut_ports_drivers or self.test_suite.sut_ports_drivers or "dpdk"
        )
        self.test_run.ctx.topology.configure_ports("sut", sut_ports_drivers)

        self.test_suite.set_up_test_case()
        self.result.mark_step_as("setup", Result.PASS)
        return TestCaseExecution(
            self.test_run,
            self.test_suite,
            self.test_case,
            self.result,
            SETTINGS.re_run,
        )

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.result.mark_step_as("setup", Result.ERROR, ex)
        self.result.mark_result_as(Result.BLOCK)
        return TestCaseTeardown(
            self.test_run,
            self.test_suite,
            self.test_case,
            self.result,
        )


@dataclass
class TestCaseExecution(TestCaseState):
    """Test case execution."""

    logger_name: ClassVar[str] = "test_case"
    reattempts_left: int

    @property
    def description(self) -> str:
        """State description."""
        return f"test case '{self.test_case.name}' execution"

    def next(self) -> State | None:
        """Next state."""
        self.logger.info(f"Running test case '{self.test_case.name}'.")
        run_test_case = MethodType(self.test_case, self.test_suite)
        try:
            run_test_case()
        except TestCaseVerifyError as e:
            self.logger.error(f"{self.description.capitalize()} FAILED: {e}")

            self.reattempts_left -= 1
            if self.reattempts_left > 0:
                self.logger.info(f"Re-attempting. {self.reattempts_left} attempts left.")
                return self

            self.result.mark_result_as(Result.FAIL, e)
        except SkippedTestException as e:
            self.logger.info(f"{self.description.capitalize()} SKIPPED: {e}")
            self.result.mark_result_as(Result.SKIP, e)
        else:
            self.result.mark_result_as(Result.PASS)
            self.logger.info(f"{self.description.capitalize()} PASSED.")

        return TestCaseTeardown(
            self.test_run,
            self.test_suite,
            self.test_case,
            self.result,
        )

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.result.mark_result_as(Result.ERROR, ex)
        return TestCaseTeardown(
            self.test_run,
            self.test_suite,
            self.test_case,
            self.result,
        )


@dataclass
class TestCaseTeardown(TestCaseState):
    """Test case teardown."""

    logger_name: ClassVar[str] = "test_case_teardown"

    @property
    def description(self) -> str:
        """State description."""
        return f"test case '{self.test_case.name}' teardown"

    def next(self) -> State | None:
        """Next state."""
        self.test_suite.tear_down_test_case()
        self.test_run.ctx.shell_pool.terminate_current_pool()
        self.result.mark_step_as("teardown", Result.PASS)
        assert self.result.parent is not None
        return TestSuiteExecution(
            test_run=self.test_run,
            test_suite=self.test_suite,
            result=self.result.parent,
        )

    def on_error(self, ex: BaseException) -> State | None:
        """Next state on error."""
        self.logger.warning(
            "The environment may have not been cleaned up correctly. "
            "The subsequent tests could be affected!"
        )
        self.result.mark_step_as("teardown", Result.ERROR, ex)

        assert self.result.parent is not None
        return TestSuiteExecution(
            test_run=self.test_run,
            test_suite=self.test_suite,
            result=self.result.parent,
        )
