# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire
# Copyright(c) 2024 Arm Limited

r"""Record and process DTS results.

The results are recorded in a hierarchical manner:

    * :class:`TestRunResult` contains
    * :class:`ResultNode` may contain itself or
    * :class:`ResultLeaf`

Each result may contain many intermediate steps, e.g. there are multiple
:class:`ResultNode`\s in a :class:`ResultNode`.

The :option:`--output` command line argument and the :envvar:`DTS_OUTPUT_DIR` environment
variable modify the directory where the files with results will be stored.
"""

import sys
from collections import Counter
from enum import IntEnum, auto
from io import StringIO
from pathlib import Path
from typing import Any, ClassVar, Literal, TextIO, Union

from pydantic import (
    BaseModel,
    ConfigDict,
    Field,
    computed_field,
    field_serializer,
    model_serializer,
)
from typing_extensions import OrderedDict

from framework.remote_session.dpdk import DPDKBuildInfo
from framework.settings import SETTINGS
from framework.testbed_model.os_session import OSSessionInfo

from .exception import DTSError, ErrorSeverity, InternalError


class Result(IntEnum):
    """The possible states that a setup, a teardown or a test case may end up in."""

    #:
    PASS = auto()
    #:
    SKIP = auto()
    #:
    BLOCK = auto()
    #:
    FAIL = auto()
    #:
    ERROR = auto()

    def __bool__(self) -> bool:
        """Only :attr:`PASS` is True."""
        return self is self.PASS


class ResultLeaf(BaseModel):
    """Class representing a result in the results tree.

    A leaf node that can contain the results for a :class:`~.test_suite.TestSuite`,
    :class:`.test_suite.TestCase` or a DTS execution step.

    Attributes:
        result: The actual result.
        reason: The reason of the result.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    result: Result
    reason: DTSError | None = None

    def __lt__(self, other: object) -> bool:
        """Compare another instance of the same class by :attr:`~ResultLeaf.result`."""
        if isinstance(other, ResultLeaf):
            return self.result < other.result
        return True

    def __eq__(self, other: object) -> bool:
        """Compare equality with compatible classes by :attr:`~ResultLeaf.result`."""
        match other:
            case ResultLeaf(result=result):
                return self.result == result
            case Result():
                return self.result == other
            case _:
                return False


ExecutionStep = Literal["setup", "teardown"]
"""Predefined execution steps."""


class ResultNode(BaseModel):
    """Class representing a node in the tree of results.

    Each node contains a label and a list of children, which can be either :class:`~.ResultNode`, or
    :class:`~.ResultLeaf`. This node is serialized as a dictionary of the children. The key of each
    child is either ``result`` in the case of a :class:`~.ResultLeaf`, or it is the value of
    :attr:`~.ResultNode.label`.

    Attributes:
        label: The name of the node.
        children: A list of either :class:`~.ResultNode` or :class:`~.ResultLeaf`.
        parent: The parent node, if any.
    """

    __ignore_steps: ClassVar[list[ExecutionStep]] = ["setup", "teardown"]

    label: str
    children: list[Union["ResultNode", ResultLeaf]] = Field(default_factory=list)
    parent: Union["ResultNode", None] = None

    def add_child(self, label: str) -> "ResultNode":
        """Creates and append a child node to the model.

        Args:
            label: The name of the node.
        """
        child = ResultNode(label=label, parent=self)
        self.children.append(child)
        return child

    def mark_result_as(self, result: Result, ex: BaseException | None = None) -> None:
        """Mark result for the current step.

        Args:
            result: The result of the current step.
            ex: The exception if any occurred. If this is not an instance of DTSError, it is wrapped
                with an InternalError.
        """
        if ex is None or isinstance(ex, DTSError):
            reason = ex
        else:
            reason = InternalError(f"Unhandled exception raised: {ex}")

        result_leaf = next((child for child in self.children if type(child) is ResultLeaf), None)
        if result_leaf:
            result_leaf.result = result
            result_leaf.reason = reason
        else:
            self.children.append(ResultLeaf(result=result, reason=reason))

    def mark_step_as(
        self, step: ExecutionStep, result: Result, ex: BaseException | None = None
    ) -> None:
        """Mark an execution step with the given result.

        Args:
            step: Step to mark, e.g.: setup, teardown.
            result: The result of the execution step.
            ex: The exception if any occurred. If this is not an instance of DTSError, it is wrapped
                with an InternalError.
        """
        try:
            step_node = next(
                child
                for child in self.children
                if type(child) is ResultNode and child.label == step
            )
        except StopIteration:
            step_node = self.add_child(step)
        step_node.mark_result_as(result, ex)

    @model_serializer
    def serialize_model(self) -> dict[str, Any]:
        """Serializes model output."""
        obj: dict[str, Any] = OrderedDict()

        for child in self.children:
            match child:
                case ResultNode(label=label):
                    obj[label] = child
                case ResultLeaf(result=result, reason=reason):
                    obj["result"] = result.name
                    if reason is not None:
                        obj["reason"] = str(reason)

        return obj

    def get_overall_result(self) -> ResultLeaf:
        """The overall result of the underlying results."""

        def extract_result(value: ResultNode | ResultLeaf) -> ResultLeaf:
            match value:
                case ResultNode():
                    return value.get_overall_result()
                case ResultLeaf():
                    return value

        return max(
            (extract_result(child) for child in self.children),
            default=ResultLeaf(result=Result.PASS),
        )

    def make_summary(self) -> Counter[Result]:
        """Make the summary of the underlying results while ignoring special nodes."""
        counter: Counter[Result] = Counter()
        for child in self.children:
            match child:
                case ResultNode(label=label) if label not in self.__ignore_steps:
                    counter += child.make_summary()
                case ResultLeaf(result=result):
                    counter[result] += 1
        return counter

    def print_results(
        self, file: TextIO = sys.stdout, indent_level: int = 0, indent_width: int = 2
    ) -> None:
        """Print the results in a textual tree format."""

        def indent(extra_level: int = 0) -> str:
            return (indent_level + extra_level) * indent_width * " "

        overall_result = self.get_overall_result()
        if self.label in self.__ignore_steps and overall_result == Result.PASS:
            return

        print(f"{indent()}{self.label}: {overall_result.result.name}", file=file)

        for child in self.children:
            match child:
                case ResultNode():
                    child.print_results(file, indent_level + 1, indent_width)
                case ResultLeaf(reason=reason) if reason is not None:
                    # The result is already printed as part of `overall_result` above.
                    print(f"{indent(1)}reason: {reason}", file=file)


class TestRunResult(BaseModel):
    """Class representing the root node of the results tree.

    Root node of the model containing metadata about the DPDK version, ports, compiler and
    DTS execution results.

    Attributes:
        sut_session_info: The SUT node OS session information.
        dpdk_build_info: The DPDK build information.
        ports: The ports that were used in the test run.
        test_suites: The test suites containing the results of DTS execution.
        execution_errors: A list of errors that occur during DTS execution.
    """

    model_config = ConfigDict(arbitrary_types_allowed=True)

    json_filepath: ClassVar[Path] = Path(SETTINGS.output_dir, "results.json")
    summary_filepath: ClassVar[Path] = Path(SETTINGS.output_dir, "results_summary.txt")

    sut_session_info: OSSessionInfo | None = None
    dpdk_build_info: DPDKBuildInfo | None = None
    ports: list[dict[str, str]] | None = None
    test_suites: ResultNode
    execution_errors: list[DTSError] = Field(default_factory=list)

    @field_serializer("execution_errors", when_used="json")
    def serialize_errors(self, execution_errors: list[DTSError]) -> list[str]:
        """Serialize errors as plain text."""
        return [str(err) for err in execution_errors]

    def add_error(self, ex: BaseException) -> None:
        """Add an execution error to the test run result."""
        if isinstance(ex, DTSError):
            self.execution_errors.append(ex)
        else:
            self.execution_errors.append(InternalError(f"Unhandled exception raised: {ex}"))

    @computed_field  # type: ignore[prop-decorator]
    @property
    def summary(self) -> dict[str, int]:
        """The test cases result summary."""
        summary = self.test_suites.make_summary()
        total_without_skip = (
            sum(total for result, total in summary.items() if result != Result.SKIP) or 1
        )

        final_summary = OrderedDict((result.name, summary[result]) for result in Result)
        final_summary["PASS_RATE"] = int(final_summary["PASS"] / total_without_skip * 100)
        return final_summary

    @property
    def return_code(self) -> int:
        """Gather all the errors and return a code by highest severity."""
        codes = [err.severity for err in self.execution_errors]
        if err := self.test_suites.get_overall_result().reason:
            codes.append(err.severity)
        return max(codes, default=ErrorSeverity.NO_ERR).value

    def print_summary(self, file: TextIO = sys.stdout) -> None:
        """Print out the textual summary."""
        print("Results", file=file)
        print("=======", file=file)
        self.test_suites.print_results(file)
        print(file=file)

        print("Test Cases Summary", file=file)
        print("==================", file=file)
        summary = self.summary
        padding = max(len(result_label) for result_label in self.summary.keys())
        for result_label, total in summary.items():
            if result_label == "PASS_RATE":
                print(f"{'PASS RATE': <{padding}} = {total}%", file=file)
            else:
                print(f"{result_label: <{padding}} = {total}", file=file)

    def dump_json(self, file: TextIO = sys.stdout, /, indent: int = 4) -> None:
        """Dump the results as JSON."""
        file.write(self.model_dump_json(indent=indent))

    def process(self) -> int:
        """Process and store all the results, and return the resulting exit code."""
        with open(self.json_filepath, "w") as json_file:
            self.dump_json(json_file)

        summary = StringIO()
        self.print_summary(summary)
        with open(self.summary_filepath, "w") as summary_file:
            summary_file.write(summary.getvalue())

        print()
        print(summary.getvalue())

        return self.return_code
