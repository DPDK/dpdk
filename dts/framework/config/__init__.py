# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Testbed configuration and test suite specification.

This package offers classes that hold real-time information about the testbed, hold test run
configuration describing the tested testbed and a loader function, :func:`load_config`, which loads
the YAML test run configuration file and validates it against the :class:`Configuration` Pydantic
model.

The YAML test run configuration file is parsed into a dictionary, parts of which are used throughout
this package. The allowed keys and types inside this dictionary map directly to the
:class:`Configuration` model, its fields and sub-models.

The test run configuration has two main sections:

    * The :class:`TestRunConfiguration` which defines what tests are going to be run
      and how DPDK will be built. It also references the testbed where these tests and DPDK
      are going to be run,
    * The nodes of the testbed are defined in the other section,
      a :class:`list` of :class:`NodeConfiguration` objects.

The real-time information about testbed is supposed to be gathered at runtime.

The classes defined in this package make heavy use of :mod:`pydantic`.
Nearly all of them are frozen:

    * Frozen makes the object immutable. This enables further optimizations,
      and makes it thread safe should we ever want to move in that direction.
"""

import tarfile
from enum import Enum, auto, unique
from functools import cached_property
from pathlib import Path, PurePath
from typing import TYPE_CHECKING, Annotated, Any, Literal, NamedTuple

import yaml
from pydantic import (
    BaseModel,
    ConfigDict,
    Field,
    ValidationError,
    field_validator,
    model_validator,
)
from typing_extensions import Self

from framework.exception import ConfigurationError
from framework.utils import REGEX_FOR_PCI_ADDRESS, StrEnum

if TYPE_CHECKING:
    from framework.test_suite import TestSuiteSpec


class FrozenModel(BaseModel):
    """A pre-configured :class:`~pydantic.BaseModel`."""

    #: Fields are set as read-only and any extra fields are forbidden.
    model_config = ConfigDict(frozen=True, extra="forbid")


@unique
class Architecture(StrEnum):
    r"""The supported architectures of :class:`~framework.testbed_model.node.Node`\s."""

    #:
    i686 = auto()
    #:
    x86_64 = auto()
    #:
    x86_32 = auto()
    #:
    arm64 = auto()
    #:
    ppc64le = auto()


@unique
class OS(StrEnum):
    r"""The supported operating systems of :class:`~framework.testbed_model.node.Node`\s."""

    #:
    linux = auto()
    #:
    freebsd = auto()
    #:
    windows = auto()


@unique
class CPUType(StrEnum):
    r"""The supported CPUs of :class:`~framework.testbed_model.node.Node`\s."""

    #:
    native = auto()
    #:
    armv8a = auto()
    #:
    dpaa2 = auto()
    #:
    thunderx = auto()
    #:
    xgene1 = auto()


@unique
class Compiler(StrEnum):
    r"""The supported compilers of :class:`~framework.testbed_model.node.Node`\s."""

    #:
    gcc = auto()
    #:
    clang = auto()
    #:
    icc = auto()
    #:
    msvc = auto()


@unique
class TrafficGeneratorType(str, Enum):
    """The supported traffic generators."""

    #:
    SCAPY = "SCAPY"


class HugepageConfiguration(FrozenModel):
    r"""The hugepage configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The number of hugepages to allocate.
    number_of: int
    #: If :data:`True`, the hugepages will be configured on the first NUMA node.
    force_first_numa: bool


class PortConfig(FrozenModel):
    r"""The port configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The PCI address of the port.
    pci: str = Field(pattern=REGEX_FOR_PCI_ADDRESS)
    #: The driver that the kernel should bind this device to for DPDK to use it.
    os_driver_for_dpdk: str = Field(examples=["vfio-pci", "mlx5_core"])
    #: The operating system driver name when the operating system controls the port.
    os_driver: str = Field(examples=["i40e", "ice", "mlx5_core"])
    #: The name of the peer node this port is connected to.
    peer_node: str
    #: The PCI address of the peer port connected to this port.
    peer_pci: str = Field(pattern=REGEX_FOR_PCI_ADDRESS)


class TrafficGeneratorConfig(FrozenModel):
    """A protocol required to define traffic generator types."""

    #: The traffic generator type the child class is required to define to be distinguished among
    #: others.
    type: TrafficGeneratorType


class ScapyTrafficGeneratorConfig(TrafficGeneratorConfig):
    """Scapy traffic generator specific configuration."""

    type: Literal[TrafficGeneratorType.SCAPY]


#: A union type discriminating traffic generators by the `type` field.
TrafficGeneratorConfigTypes = Annotated[ScapyTrafficGeneratorConfig, Field(discriminator="type")]

#: Comma-separated list of logical cores to use. An empty string means use all lcores.
LogicalCores = Annotated[
    str,
    Field(
        examples=["1,2,3,4,5,18-22", "10-15"],
        pattern=r"^(([0-9]+|([0-9]+-[0-9]+))(,([0-9]+|([0-9]+-[0-9]+)))*)?$",
    ),
]


class NodeConfiguration(FrozenModel):
    r"""The configuration of :class:`~framework.testbed_model.node.Node`\s."""

    #: The name of the :class:`~framework.testbed_model.node.Node`.
    name: str
    #: The hostname of the :class:`~framework.testbed_model.node.Node`. Can also be an IP address.
    hostname: str
    #: The name of the user used to connect to the :class:`~framework.testbed_model.node.Node`.
    user: str
    #: The password of the user. The use of passwords is heavily discouraged, please use SSH keys.
    password: str | None = None
    #: The architecture of the :class:`~framework.testbed_model.node.Node`.
    arch: Architecture
    #: The operating system of the :class:`~framework.testbed_model.node.Node`.
    os: OS
    #: A comma delimited list of logical cores to use when running DPDK.
    lcores: LogicalCores = "1"
    #: If :data:`True`, the first logical core won't be used.
    use_first_core: bool = False
    #: An optional hugepage configuration.
    hugepages: HugepageConfiguration | None = Field(None, alias="hugepages_2mb")
    #: The ports that can be used in testing.
    ports: list[PortConfig] = Field(min_length=1)


class SutNodeConfiguration(NodeConfiguration):
    """:class:`~framework.testbed_model.sut_node.SutNode` specific configuration."""

    #: The number of memory channels to use when running DPDK.
    memory_channels: int = 1


class TGNodeConfiguration(NodeConfiguration):
    """:class:`~framework.testbed_model.tg_node.TGNode` specific configuration."""

    #: The configuration of the traffic generator present on the TG node.
    traffic_generator: TrafficGeneratorConfigTypes


#: Union type for all the node configuration types.
NodeConfigurationTypes = TGNodeConfiguration | SutNodeConfiguration


def resolve_path(path: Path) -> Path:
    """Resolve a path into a real path."""
    return path.resolve()


class BaseDPDKLocation(FrozenModel):
    """DPDK location base class.

    The path to the DPDK sources and type of location.
    """

    #: Specifies whether to find DPDK on the SUT node or on the local host. Which are respectively
    #: represented by :class:`RemoteDPDKLocation` and :class:`LocalDPDKTreeLocation`.
    remote: bool = False


class LocalDPDKLocation(BaseDPDKLocation):
    """Local DPDK location base class.

    This class is meant to represent any location that is present only locally.
    """

    remote: Literal[False] = False


class LocalDPDKTreeLocation(LocalDPDKLocation):
    """Local DPDK tree location.

    This class makes a distinction from :class:`RemoteDPDKTreeLocation` by enforcing on the fly
    validation.
    """

    #: The path to the DPDK source tree directory on the local host passed as string.
    dpdk_tree: Path

    #: Resolve the local DPDK tree path.
    resolve_dpdk_tree_path = field_validator("dpdk_tree")(resolve_path)

    @model_validator(mode="after")
    def validate_dpdk_tree_path(self) -> Self:
        """Validate the provided DPDK tree path."""
        assert self.dpdk_tree.exists(), "DPDK tree not found in local filesystem."
        assert self.dpdk_tree.is_dir(), "The DPDK tree path must be a directory."
        return self


class LocalDPDKTarballLocation(LocalDPDKLocation):
    """Local DPDK tarball location.

    This class makes a distinction from :class:`RemoteDPDKTarballLocation` by enforcing on the fly
    validation.
    """

    #: The path to the DPDK tarball on the local host passed as string.
    tarball: Path

    #: Resolve the local tarball path.
    resolve_tarball_path = field_validator("tarball")(resolve_path)

    @model_validator(mode="after")
    def validate_tarball_path(self) -> Self:
        """Validate the provided tarball."""
        assert self.tarball.exists(), "DPDK tarball not found in local filesystem."
        assert tarfile.is_tarfile(self.tarball), "The DPDK tarball must be a valid tar archive."
        return self


class RemoteDPDKLocation(BaseDPDKLocation):
    """Remote DPDK location base class.

    This class is meant to represent any location that is present only remotely.
    """

    remote: Literal[True] = True


class RemoteDPDKTreeLocation(RemoteDPDKLocation):
    """Remote DPDK tree location.

    This class is distinct from :class:`LocalDPDKTreeLocation` which enforces on the fly validation.
    """

    #: The path to the DPDK source tree directory on the remote node passed as string.
    dpdk_tree: PurePath


class RemoteDPDKTarballLocation(RemoteDPDKLocation):
    """Remote DPDK tarball location.

    This class is distinct from :class:`LocalDPDKTarballLocation` which enforces on the fly
    validation.
    """

    #: The path to the DPDK tarball on the remote node passed as string.
    tarball: PurePath


#: Union type for different DPDK locations.
DPDKLocation = (
    LocalDPDKTreeLocation
    | LocalDPDKTarballLocation
    | RemoteDPDKTreeLocation
    | RemoteDPDKTarballLocation
)


class BaseDPDKBuildConfiguration(FrozenModel):
    """The base configuration for different types of build.

    The configuration contain the location of the DPDK and configuration used for building it.
    """

    #: The location of the DPDK tree.
    dpdk_location: DPDKLocation


class DPDKPrecompiledBuildConfiguration(BaseDPDKBuildConfiguration):
    """DPDK precompiled build configuration."""

    #: If it's defined, DPDK has been pre-compiled and the build directory is located in a
    #: subdirectory of `~dpdk_location.dpdk_tree` or `~dpdk_location.tarball` root directory.
    precompiled_build_dir: str = Field(min_length=1)


class DPDKBuildOptionsConfiguration(FrozenModel):
    """DPDK build options configuration.

    The build options used for building DPDK.
    """

    #: The target architecture to build for.
    arch: Architecture
    #: The target OS to build for.
    os: OS
    #: The target CPU to build for.
    cpu: CPUType
    #: The compiler executable to use.
    compiler: Compiler
    #: This string will be put in front of the compiler when executing the build. Useful for adding
    #: wrapper commands, such as ``ccache``.
    compiler_wrapper: str = ""

    @cached_property
    def name(self) -> str:
        """The name of the compiler."""
        return f"{self.arch}-{self.os}-{self.cpu}-{self.compiler}"


class DPDKUncompiledBuildConfiguration(BaseDPDKBuildConfiguration):
    """DPDK uncompiled build configuration."""

    #: The build options to compiled DPDK with.
    build_options: DPDKBuildOptionsConfiguration


#: Union type for different build configurations.
DPDKBuildConfiguration = DPDKPrecompiledBuildConfiguration | DPDKUncompiledBuildConfiguration


class TestSuiteConfig(FrozenModel):
    """Test suite configuration.

    Information about a single test suite to be executed. This can also be represented as a string
    instead of a mapping, example:

    .. code:: yaml

        test_runs:
        - test_suites:
            # As string representation:
            - hello_world # test all of `hello_world`, or
            - hello_world hello_world_single_core # test only `hello_world_single_core`
            # or as model fields:
            - test_suite: hello_world
              test_cases: [hello_world_single_core] # without this field all test cases are run
    """

    #: The name of the test suite module without the starting ``TestSuite_``.
    test_suite_name: str = Field(alias="test_suite")
    #: The names of test cases from this test suite to execute. If empty, all test cases will be
    #: executed.
    test_cases_names: list[str] = Field(default_factory=list, alias="test_cases")

    @cached_property
    def test_suite_spec(self) -> "TestSuiteSpec":
        """The specification of the requested test suite."""
        from framework.test_suite import find_by_name

        test_suite_spec = find_by_name(self.test_suite_name)
        assert (
            test_suite_spec is not None
        ), f"{self.test_suite_name} is not a valid test suite module name."
        return test_suite_spec

    @model_validator(mode="before")
    @classmethod
    def convert_from_string(cls, data: Any) -> Any:
        """Convert the string representation of the model into a valid mapping."""
        if isinstance(data, str):
            [test_suite, *test_cases] = data.split()
            return dict(test_suite=test_suite, test_cases=test_cases)
        return data

    @model_validator(mode="after")
    def validate_names(self) -> Self:
        """Validate the supplied test suite and test cases names.

        This validator relies on the cached property `test_suite_spec` to run for the first
        time in this call, therefore triggering the assertions if needed.
        """
        available_test_cases = map(
            lambda t: t.name, self.test_suite_spec.class_obj.get_test_cases()
        )
        for requested_test_case in self.test_cases_names:
            assert requested_test_case in available_test_cases, (
                f"{requested_test_case} is not a valid test case "
                f"of test suite {self.test_suite_name}."
            )

        return self


class TestRunSUTNodeConfiguration(FrozenModel):
    """The SUT node configuration of a test run."""

    #: The SUT node to use in this test run.
    node_name: str
    #: The names of virtual devices to test.
    vdevs: list[str] = Field(default_factory=list)


class TestRunConfiguration(FrozenModel):
    """The configuration of a test run.

    The configuration contains testbed information, what tests to execute
    and with what DPDK build.
    """

    #: The DPDK configuration used to test.
    dpdk_config: DPDKBuildConfiguration = Field(alias="dpdk_build")
    #: Whether to run performance tests.
    perf: bool
    #: Whether to run functional tests.
    func: bool
    #: Whether to skip smoke tests.
    skip_smoke_tests: bool = False
    #: The names of test suites and/or test cases to execute.
    test_suites: list[TestSuiteConfig] = Field(min_length=1)
    #: The SUT node configuration to use in this test run.
    system_under_test_node: TestRunSUTNodeConfiguration
    #: The TG node name to use in this test run.
    traffic_generator_node: str
    #: The seed to use for pseudo-random generation.
    random_seed: int | None = None


class TestRunWithNodesConfiguration(NamedTuple):
    """Tuple containing the configuration of the test run and its associated nodes."""

    #:
    test_run_config: TestRunConfiguration
    #:
    sut_node_config: SutNodeConfiguration
    #:
    tg_node_config: TGNodeConfiguration


class Configuration(FrozenModel):
    """DTS testbed and test configuration."""

    #: Test run configurations.
    test_runs: list[TestRunConfiguration] = Field(min_length=1)
    #: Node configurations.
    nodes: list[NodeConfigurationTypes] = Field(min_length=1)

    @cached_property
    def test_runs_with_nodes(self) -> list[TestRunWithNodesConfiguration]:
        """List of test runs with the associated nodes."""
        test_runs_with_nodes = []

        for test_run_no, test_run in enumerate(self.test_runs):
            sut_node_name = test_run.system_under_test_node.node_name
            sut_node = next(filter(lambda n: n.name == sut_node_name, self.nodes), None)

            assert sut_node is not None, (
                f"test_runs.{test_run_no}.sut_node_config.node_name "
                f"({test_run.system_under_test_node.node_name}) is not a valid node name"
            )
            assert isinstance(sut_node, SutNodeConfiguration), (
                f"test_runs.{test_run_no}.sut_node_config.node_name is a valid node name, "
                "but it is not a valid SUT node"
            )

            tg_node_name = test_run.traffic_generator_node
            tg_node = next(filter(lambda n: n.name == tg_node_name, self.nodes), None)

            assert tg_node is not None, (
                f"test_runs.{test_run_no}.tg_node_name "
                f"({test_run.traffic_generator_node}) is not a valid node name"
            )
            assert isinstance(tg_node, TGNodeConfiguration), (
                f"test_runs.{test_run_no}.tg_node_name is a valid node name, "
                "but it is not a valid TG node"
            )

            test_runs_with_nodes.append(TestRunWithNodesConfiguration(test_run, sut_node, tg_node))

        return test_runs_with_nodes

    @field_validator("nodes")
    @classmethod
    def validate_node_names(cls, nodes: list[NodeConfiguration]) -> list[NodeConfiguration]:
        """Validate that the node names are unique."""
        nodes_by_name: dict[str, int] = {}
        for node_no, node in enumerate(nodes):
            assert node.name not in nodes_by_name, (
                f"node {node_no} cannot have the same name as node {nodes_by_name[node.name]} "
                f"({node.name})"
            )
            nodes_by_name[node.name] = node_no

        return nodes

    @model_validator(mode="after")
    def validate_ports(self) -> Self:
        """Validate that the ports are all linked to valid ones."""
        port_links: dict[tuple[str, str], Literal[False] | tuple[int, int]] = {
            (node.name, port.pci): False for node in self.nodes for port in node.ports
        }

        for node_no, node in enumerate(self.nodes):
            for port_no, port in enumerate(node.ports):
                peer_port_identifier = (port.peer_node, port.peer_pci)
                peer_port = port_links.get(peer_port_identifier, None)
                assert peer_port is not None, (
                    "invalid peer port specified for " f"nodes.{node_no}.ports.{port_no}"
                )
                assert peer_port is False, (
                    f"the peer port specified for nodes.{node_no}.ports.{port_no} "
                    f"is already linked to nodes.{peer_port[0]}.ports.{peer_port[1]}"
                )
                port_links[peer_port_identifier] = (node_no, port_no)

        return self

    @model_validator(mode="after")
    def validate_test_runs_with_nodes(self) -> Self:
        """Validate the test runs to nodes associations.

        This validator relies on the cached property `test_runs_with_nodes` to run for the first
        time in this call, therefore triggering the assertions if needed.
        """
        if self.test_runs_with_nodes:
            pass
        return self


def load_config(config_file_path: Path) -> Configuration:
    """Load DTS test run configuration from a file.

    Load the YAML test run configuration file, validate it, and create a test run configuration
    object.

    The YAML test run configuration file is specified in the :option:`--config-file` command line
    argument or the :envvar:`DTS_CFG_FILE` environment variable.

    Args:
        config_file_path: The path to the YAML test run configuration file.

    Returns:
        The parsed test run configuration.

    Raises:
        ConfigurationError: If the supplied configuration file is invalid.
    """
    with open(config_file_path, "r") as f:
        config_data = yaml.safe_load(f)

    try:
        return Configuration.model_validate(config_data)
    except ValidationError as e:
        raise ConfigurationError("failed to load the supplied configuration") from e
