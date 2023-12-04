# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.

"""Testbed configuration and test suite specification.

This package offers classes that hold real-time information about the testbed, hold test run
configuration describing the tested testbed and a loader function, :func:`load_config`, which loads
the YAML test run configuration file
and validates it according to :download:`the schema <conf_yaml_schema.json>`.

The YAML test run configuration file is parsed into a dictionary, parts of which are used throughout
this package. The allowed keys and types inside this dictionary are defined in
the :doc:`types <framework.config.types>` module.

The test run configuration has two main sections:

    * The :class:`ExecutionConfiguration` which defines what tests are going to be run
      and how DPDK will be built. It also references the testbed where these tests and DPDK
      are going to be run,
    * The nodes of the testbed are defined in the other section,
      a :class:`list` of :class:`NodeConfiguration` objects.

The real-time information about testbed is supposed to be gathered at runtime.

The classes defined in this package make heavy use of :mod:`dataclasses`.
All of them use slots and are frozen:

    * Slots enables some optimizations, by pre-allocating space for the defined
      attributes in the underlying data structure,
    * Frozen makes the object immutable. This enables further optimizations,
      and makes it thread safe should we ever want to move in that direction.
"""

import json
import os.path
import pathlib
from dataclasses import dataclass
from enum import auto, unique
from typing import Union

import warlock  # type: ignore[import]
import yaml

from framework.config.types import (
    BuildTargetConfigDict,
    ConfigurationDict,
    ExecutionConfigDict,
    NodeConfigDict,
    PortConfigDict,
    TestSuiteConfigDict,
    TrafficGeneratorConfigDict,
)
from framework.exception import ConfigurationError
from framework.settings import SETTINGS
from framework.utils import StrEnum


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
class TrafficGeneratorType(StrEnum):
    """The supported traffic generators."""

    #:
    SCAPY = auto()


@dataclass(slots=True, frozen=True)
class HugepageConfiguration:
    r"""The hugepage configuration of :class:`~framework.testbed_model.node.Node`\s.

    Attributes:
        amount: The number of hugepages.
        force_first_numa: If :data:`True`, the hugepages will be configured on the first NUMA node.
    """

    amount: int
    force_first_numa: bool


@dataclass(slots=True, frozen=True)
class PortConfig:
    r"""The port configuration of :class:`~framework.testbed_model.node.Node`\s.

    Attributes:
        node: The :class:`~framework.testbed_model.node.Node` where this port exists.
        pci: The PCI address of the port.
        os_driver_for_dpdk: The operating system driver name for use with DPDK.
        os_driver: The operating system driver name when the operating system controls the port.
        peer_node: The :class:`~framework.testbed_model.node.Node` of the port
            connected to this port.
        peer_pci: The PCI address of the port connected to this port.
    """

    node: str
    pci: str
    os_driver_for_dpdk: str
    os_driver: str
    peer_node: str
    peer_pci: str

    @staticmethod
    def from_dict(node: str, d: PortConfigDict) -> "PortConfig":
        """A convenience method that creates the object from fewer inputs.

        Args:
            node: The node where this port exists.
            d: The configuration dictionary.

        Returns:
            The port configuration instance.
        """
        return PortConfig(node=node, **d)


@dataclass(slots=True, frozen=True)
class TrafficGeneratorConfig:
    """The configuration of traffic generators.

    The class will be expanded when more configuration is needed.

    Attributes:
        traffic_generator_type: The type of the traffic generator.
    """

    traffic_generator_type: TrafficGeneratorType

    @staticmethod
    def from_dict(d: TrafficGeneratorConfigDict) -> "ScapyTrafficGeneratorConfig":
        """A convenience method that produces traffic generator config of the proper type.

        Args:
            d: The configuration dictionary.

        Returns:
            The traffic generator configuration instance.

        Raises:
            ConfigurationError: An unknown traffic generator type was encountered.
        """
        match TrafficGeneratorType(d["type"]):
            case TrafficGeneratorType.SCAPY:
                return ScapyTrafficGeneratorConfig(
                    traffic_generator_type=TrafficGeneratorType.SCAPY
                )
            case _:
                raise ConfigurationError(f'Unknown traffic generator type "{d["type"]}".')


@dataclass(slots=True, frozen=True)
class ScapyTrafficGeneratorConfig(TrafficGeneratorConfig):
    """Scapy traffic generator specific configuration."""

    pass


@dataclass(slots=True, frozen=True)
class NodeConfiguration:
    r"""The configuration of :class:`~framework.testbed_model.node.Node`\s.

    Attributes:
        name: The name of the :class:`~framework.testbed_model.node.Node`.
        hostname: The hostname of the :class:`~framework.testbed_model.node.Node`.
            Can be an IP or a domain name.
        user: The name of the user used to connect to
            the :class:`~framework.testbed_model.node.Node`.
        password: The password of the user. The use of passwords is heavily discouraged.
            Please use keys instead.
        arch: The architecture of the :class:`~framework.testbed_model.node.Node`.
        os: The operating system of the :class:`~framework.testbed_model.node.Node`.
        lcores: A comma delimited list of logical cores to use when running DPDK.
        use_first_core: If :data:`True`, the first logical core won't be used.
        hugepages: An optional hugepage configuration.
        ports: The ports that can be used in testing.
    """

    name: str
    hostname: str
    user: str
    password: str | None
    arch: Architecture
    os: OS
    lcores: str
    use_first_core: bool
    hugepages: HugepageConfiguration | None
    ports: list[PortConfig]

    @staticmethod
    def from_dict(
        d: NodeConfigDict,
    ) -> Union["SutNodeConfiguration", "TGNodeConfiguration"]:
        """A convenience method that processes the inputs before creating a specialized instance.

        Args:
            d: The configuration dictionary.

        Returns:
            Either an SUT or TG configuration instance.
        """
        hugepage_config = None
        if "hugepages" in d:
            hugepage_config_dict = d["hugepages"]
            if "force_first_numa" not in hugepage_config_dict:
                hugepage_config_dict["force_first_numa"] = False
            hugepage_config = HugepageConfiguration(**hugepage_config_dict)

        # The calls here contain duplicated code which is here because Mypy doesn't
        # properly support dictionary unpacking with TypedDicts
        if "traffic_generator" in d:
            return TGNodeConfiguration(
                name=d["name"],
                hostname=d["hostname"],
                user=d["user"],
                password=d.get("password"),
                arch=Architecture(d["arch"]),
                os=OS(d["os"]),
                lcores=d.get("lcores", "1"),
                use_first_core=d.get("use_first_core", False),
                hugepages=hugepage_config,
                ports=[PortConfig.from_dict(d["name"], port) for port in d["ports"]],
                traffic_generator=TrafficGeneratorConfig.from_dict(d["traffic_generator"]),
            )
        else:
            return SutNodeConfiguration(
                name=d["name"],
                hostname=d["hostname"],
                user=d["user"],
                password=d.get("password"),
                arch=Architecture(d["arch"]),
                os=OS(d["os"]),
                lcores=d.get("lcores", "1"),
                use_first_core=d.get("use_first_core", False),
                hugepages=hugepage_config,
                ports=[PortConfig.from_dict(d["name"], port) for port in d["ports"]],
                memory_channels=d.get("memory_channels", 1),
            )


@dataclass(slots=True, frozen=True)
class SutNodeConfiguration(NodeConfiguration):
    """:class:`~framework.testbed_model.sut_node.SutNode` specific configuration.

    Attributes:
        memory_channels: The number of memory channels to use when running DPDK.
    """

    memory_channels: int


@dataclass(slots=True, frozen=True)
class TGNodeConfiguration(NodeConfiguration):
    """:class:`~framework.testbed_model.tg_node.TGNode` specific configuration.

    Attributes:
        traffic_generator: The configuration of the traffic generator present on the TG node.
    """

    traffic_generator: ScapyTrafficGeneratorConfig


@dataclass(slots=True, frozen=True)
class NodeInfo:
    """Supplemental node information.

    Attributes:
        os_name: The name of the running operating system of
            the :class:`~framework.testbed_model.node.Node`.
        os_version: The version of the running operating system of
            the :class:`~framework.testbed_model.node.Node`.
        kernel_version: The kernel version of the running operating system of
            the :class:`~framework.testbed_model.node.Node`.
    """

    os_name: str
    os_version: str
    kernel_version: str


@dataclass(slots=True, frozen=True)
class BuildTargetConfiguration:
    """DPDK build configuration.

    The configuration used for building DPDK.

    Attributes:
        arch: The target architecture to build for.
        os: The target os to build for.
        cpu: The target CPU to build for.
        compiler: The compiler executable to use.
        compiler_wrapper: This string will be put in front of the compiler when
            executing the build. Useful for adding wrapper commands, such as ``ccache``.
        name: The name of the compiler.
    """

    arch: Architecture
    os: OS
    cpu: CPUType
    compiler: Compiler
    compiler_wrapper: str
    name: str

    @staticmethod
    def from_dict(d: BuildTargetConfigDict) -> "BuildTargetConfiguration":
        r"""A convenience method that processes the inputs before creating an instance.

        `arch`, `os`, `cpu` and `compiler` are converted to :class:`Enum`\s and
        `name` is constructed from `arch`, `os`, `cpu` and `compiler`.

        Args:
            d: The configuration dictionary.

        Returns:
            The build target configuration instance.
        """
        return BuildTargetConfiguration(
            arch=Architecture(d["arch"]),
            os=OS(d["os"]),
            cpu=CPUType(d["cpu"]),
            compiler=Compiler(d["compiler"]),
            compiler_wrapper=d.get("compiler_wrapper", ""),
            name=f"{d['arch']}-{d['os']}-{d['cpu']}-{d['compiler']}",
        )


@dataclass(slots=True, frozen=True)
class BuildTargetInfo:
    """Various versions and other information about a build target.

    Attributes:
        dpdk_version: The DPDK version that was built.
        compiler_version: The version of the compiler used to build DPDK.
    """

    dpdk_version: str
    compiler_version: str


@dataclass(slots=True, frozen=True)
class TestSuiteConfig:
    """Test suite configuration.

    Information about a single test suite to be executed.

    Attributes:
        test_suite: The name of the test suite module without the starting ``TestSuite_``.
        test_cases: The names of test cases from this test suite to execute.
            If empty, all test cases will be executed.
    """

    test_suite: str
    test_cases: list[str]

    @staticmethod
    def from_dict(
        entry: str | TestSuiteConfigDict,
    ) -> "TestSuiteConfig":
        """Create an instance from two different types.

        Args:
            entry: Either a suite name or a dictionary containing the config.

        Returns:
            The test suite configuration instance.
        """
        if isinstance(entry, str):
            return TestSuiteConfig(test_suite=entry, test_cases=[])
        elif isinstance(entry, dict):
            return TestSuiteConfig(test_suite=entry["suite"], test_cases=entry["cases"])
        else:
            raise TypeError(f"{type(entry)} is not valid for a test suite config.")


@dataclass(slots=True, frozen=True)
class ExecutionConfiguration:
    """The configuration of an execution.

    The configuration contains testbed information, what tests to execute
    and with what DPDK build.

    Attributes:
        build_targets: A list of DPDK builds to test.
        perf: Whether to run performance tests.
        func: Whether to run functional tests.
        skip_smoke_tests: Whether to skip smoke tests.
        test_suites: The names of test suites and/or test cases to execute.
        system_under_test_node: The SUT node to use in this execution.
        traffic_generator_node: The TG node to use in this execution.
        vdevs: The names of virtual devices to test.
    """

    build_targets: list[BuildTargetConfiguration]
    perf: bool
    func: bool
    skip_smoke_tests: bool
    test_suites: list[TestSuiteConfig]
    system_under_test_node: SutNodeConfiguration
    traffic_generator_node: TGNodeConfiguration
    vdevs: list[str]

    @staticmethod
    def from_dict(
        d: ExecutionConfigDict,
        node_map: dict[str, Union[SutNodeConfiguration | TGNodeConfiguration]],
    ) -> "ExecutionConfiguration":
        """A convenience method that processes the inputs before creating an instance.

        The build target and the test suite config are transformed into their respective objects.
        SUT and TG configurations are taken from `node_map`. The other (:class:`bool`) attributes
        are just stored.

        Args:
            d: The configuration dictionary.
            node_map: A dictionary mapping node names to their config objects.

        Returns:
            The execution configuration instance.
        """
        build_targets: list[BuildTargetConfiguration] = list(
            map(BuildTargetConfiguration.from_dict, d["build_targets"])
        )
        test_suites: list[TestSuiteConfig] = list(map(TestSuiteConfig.from_dict, d["test_suites"]))
        sut_name = d["system_under_test_node"]["node_name"]
        skip_smoke_tests = d.get("skip_smoke_tests", False)
        assert sut_name in node_map, f"Unknown SUT {sut_name} in execution {d}"
        system_under_test_node = node_map[sut_name]
        assert isinstance(
            system_under_test_node, SutNodeConfiguration
        ), f"Invalid SUT configuration {system_under_test_node}"

        tg_name = d["traffic_generator_node"]
        assert tg_name in node_map, f"Unknown TG {tg_name} in execution {d}"
        traffic_generator_node = node_map[tg_name]
        assert isinstance(
            traffic_generator_node, TGNodeConfiguration
        ), f"Invalid TG configuration {traffic_generator_node}"

        vdevs = (
            d["system_under_test_node"]["vdevs"] if "vdevs" in d["system_under_test_node"] else []
        )
        return ExecutionConfiguration(
            build_targets=build_targets,
            perf=d["perf"],
            func=d["func"],
            skip_smoke_tests=skip_smoke_tests,
            test_suites=test_suites,
            system_under_test_node=system_under_test_node,
            traffic_generator_node=traffic_generator_node,
            vdevs=vdevs,
        )


@dataclass(slots=True, frozen=True)
class Configuration:
    """DTS testbed and test configuration.

    The node configuration is not stored in this object. Rather, all used node configurations
    are stored inside the execution configuration where the nodes are actually used.

    Attributes:
        executions: Execution configurations.
    """

    executions: list[ExecutionConfiguration]

    @staticmethod
    def from_dict(d: ConfigurationDict) -> "Configuration":
        """A convenience method that processes the inputs before creating an instance.

        Build target and test suite config are transformed into their respective objects.
        SUT and TG configurations are taken from `node_map`. The other (:class:`bool`) attributes
        are just stored.

        Args:
            d: The configuration dictionary.

        Returns:
            The whole configuration instance.
        """
        nodes: list[Union[SutNodeConfiguration | TGNodeConfiguration]] = list(
            map(NodeConfiguration.from_dict, d["nodes"])
        )
        assert len(nodes) > 0, "There must be a node to test"

        node_map = {node.name: node for node in nodes}
        assert len(nodes) == len(node_map), "Duplicate node names are not allowed"

        executions: list[ExecutionConfiguration] = list(
            map(ExecutionConfiguration.from_dict, d["executions"], [node_map for _ in d])
        )

        return Configuration(executions=executions)


def load_config() -> Configuration:
    """Load DTS test run configuration from a file.

    Load the YAML test run configuration file
    and :download:`the configuration file schema <conf_yaml_schema.json>`,
    validate the test run configuration file, and create a test run configuration object.

    The YAML test run configuration file is specified in the :option:`--config-file` command line
    argument or the :envvar:`DTS_CFG_FILE` environment variable.

    Returns:
        The parsed test run configuration.
    """
    with open(SETTINGS.config_file_path, "r") as f:
        config_data = yaml.safe_load(f)

    schema_path = os.path.join(pathlib.Path(__file__).parent.resolve(), "conf_yaml_schema.json")

    with open(schema_path, "r") as f:
        schema = json.load(f)
    config = warlock.model_factory(schema, name="_Config")(config_data)
    config_obj: Configuration = Configuration.from_dict(dict(config))  # type: ignore[arg-type]
    return config_obj
