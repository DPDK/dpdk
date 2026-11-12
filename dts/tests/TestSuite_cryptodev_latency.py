# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 University of New Hampshire

"""DPDK cryptodev performance test suite.

The main goal of this test suite is to utilize the dpdk-test-cryptodev application to gather
performance metrics for various cryptographic operations supported by DPDK cryptodev-pmd.
It will then compare the results against a predefined baseline given in the test_config file to
ensure performance standards are met.
"""

from api.capabilities import (
    LinkTopology,
    requires_link_topology,
)
from api.context import get_ctx
from api.cryptodev import Cryptodev
from api.cryptodev.config import (
    AEAD_ALGORITHM_PARAMS,
    AUTHENTICATION_ALGORITHM_PARAMS,
    CIPHER_ALGORITHM_PARAMS,
    AeadAlgName,
    AuthenticationAlgorithm,
    AuthenticationOpMode,
    CipherAlgorithm,
    DeviceType,
    EncryptDecryptSwitch,
    ListWrapper,
    OperationType,
    TestType,
    get_device_from_str,
)
from api.cryptodev.types import (
    CryptodevResults,
    LatencyCryptoConfig,
    LatencyParameters,
)
from api.exception import ConfigurationError, SkippedTestException, TestCaseVerifyError
from api.test import fail, skip, verify
from api.test_suite import BaseConfig, TestSuite, perf_suite, perf_test
from api.testbed_model.virtual_device import VirtualDevice
from api.utils import default_if_none

TOTAL_OPS = 10_000_000

config_list: list[LatencyParameters] = [
    LatencyParameters(buff_size=64, avg_cycles=999_999_999.0, avg_time_us=999_999_999.0),
    LatencyParameters(buff_size=512, avg_cycles=999_999_999.0, avg_time_us=999_999_999.0),
    LatencyParameters(buff_size=2048, avg_cycles=999_999_999.0, avg_time_us=999_999_999.0),
]


class Config(BaseConfig):
    """Performance test metrics.

    Attributes:
        delta_tolerance: The allowed tolerance below a given baseline.
        test_combinations: List of test combinations with cipher/auth algorithms and parameters.
    """

    delta_tolerance: float = 0.05
    test_combinations: list[LatencyCryptoConfig] = [
        #: cipher only tests
        LatencyCryptoConfig(
            name="aes-cbc (cipher only)",
            cipher_algorithm=CipherAlgorithm.aes_cbc,
            burst_size=32,
        ),
        LatencyCryptoConfig(
            name="aead-docsisbpi (cipher only)",
            cipher_algorithm=CipherAlgorithm.aes_docsisbpi,
        ),
        #: cipher then auth tests
        LatencyCryptoConfig(
            name="aes-cbc/sha1-hmac (cipher then auth)",
            cipher_algorithm=CipherAlgorithm.aes_cbc,
            auth_algorithm=AuthenticationAlgorithm.sha1_hmac,
            burst_size=32,
            virtual_device="crypto_openssl",
        ),
        LatencyCryptoConfig(
            name="aes-cbc/sha2-256-hmac (cipher then auth)",
            cipher_algorithm=CipherAlgorithm.aes_cbc,
            auth_algorithm=AuthenticationAlgorithm.sha2_256_hmac,
            burst_size=32,
            virtual_device="crypto_openssl",
        ),
        LatencyCryptoConfig(
            name="kasumi-f8/kasumi-f9 (cipher then auth)",
            cipher_algorithm=CipherAlgorithm.kasumi_f8,
            auth_algorithm=AuthenticationAlgorithm.kasumi_f9,
            burst_size=32,
        ),
        LatencyCryptoConfig(
            name="snow3g-uea2/snow3g-uia2 (cipher then auth)",
            cipher_algorithm=CipherAlgorithm.snow3g_uea2,
            auth_algorithm=AuthenticationAlgorithm.snow3g_uia2,
        ),
        LatencyCryptoConfig(
            name="zuc-eea3/zuc-eia3 (cipher then auth)",
            cipher_algorithm=CipherAlgorithm.zuc_eea3,
            auth_algorithm=AuthenticationAlgorithm.zuc_eia3,
        ),
        #: aead tests
        LatencyCryptoConfig(
            name="aead-aes-gcm (aead)",
            aead_algorithm=AeadAlgName.aes_gcm,
        ),
    ]


@requires_link_topology(LinkTopology.NO_LINK)
@perf_suite
class TestCryptodevLatency(TestSuite):
    """DPDK Crypto Device Latency Testing Suite."""

    config: Config

    def set_up_suite(self) -> None:
        """Set up the test suite.

        Raises:
            ConfigurationError: If no tests can be derived from the provided config.
        """
        self.test_combinations: list[LatencyCryptoConfig] = self.config.test_combinations
        self.delta_tolerance: float = self.config.delta_tolerance
        self.ops: int = 10_000_000
        self.device_type: DeviceType | None = get_device_from_str(
            str(get_ctx().sut_node.crypto_device_type)
        )
        self.buffer_sizes: dict[str, ListWrapper] = {}

        # categorize configured tests
        self.cipher_tests: list[LatencyCryptoConfig] = []
        self.auth_tests: list[LatencyCryptoConfig] = []
        self.cipher_then_auth_tests: list[LatencyCryptoConfig] = []
        self.aead_tests: list[LatencyCryptoConfig] = []

        for combination in self.test_combinations:
            self.buffer_sizes[combination.name] = ListWrapper(
                [int(run.buff_size) for run in combination.test_parameters or config_list]
            )
            is_cipher = combination.cipher_algorithm is not None
            is_auth = combination.auth_algorithm is not None
            aead = combination.aead_algorithm is not None

            if is_cipher and is_auth:
                self.cipher_then_auth_tests.append(combination)
            elif is_cipher:
                self.cipher_tests.append(combination)
            elif is_auth:
                self.auth_tests.append(combination)
            elif aead:
                self.aead_tests.append(combination)
            else:
                raise ConfigurationError(
                    "Each test instance must contain either "
                    "'cipher_algorithm', 'auth_algorithm', or 'aead_algorithm'"
                )

    def _print_stats(self, test_vals: list[dict[str, int | float | str]]) -> None:
        """Print the stats of the given test values in a clean format.

        Args:
            test_vals: the testing values to print
        """
        assert len(test_vals) > 0, "test_vals must contain at least one element"

        element_len = len("Avg Time us Target")
        border_len = (element_len + 1) * (len(test_vals[0]) - 1)
        test_name = test_vals[0]["Test Name"]

        print(f"{f'{test_name} Latency Results'.center(border_len)}\n{'=' * border_len}")
        for k, v in test_vals[0].items():
            if k != "Test Name":
                print(f"|{k.title():<{element_len}}", end="")
        print(f"|\n{'=' * border_len}")

        for test_val in test_vals:
            for k, v in test_val.items():
                if k != "Test Name":
                    print(f"|{v:<{element_len}}", end="")
            print(f"|\n{'=' * border_len}")

    def _print_and_verify(self, all_results: list[list[dict[str, int | float | str]]]) -> None:
        """Print the given results and then verify all results have passed.

        Args:
            all_results: a list of results to print and verify.
        """
        for results in all_results:
            if len(results) > 0:
                self._print_stats(test_vals=results)

        for results in all_results:
            for result in results:
                verify(
                    result["passed"] == "PASS",
                    f"Gbps fell below delta tolerance for test {result['Test Name']}",
                )

    def _create_summary(
        self,
        results: list[CryptodevResults],
        params: list[LatencyParameters],
        test_name: str,
    ) -> list[dict[str, int | float | str]]:
        """Verify the throughput of the given results against the supplied parameters.

        Args:
            results: The results of the dpdk crypto perf app ran in latency mode.
            params: The baselines to compare the given results to.
            test_name: The test name to pin to the result summary.

        Returns:
            A summary of latency statistics for a test and whether the test passed.

        Raises:
            RuntimeError: When there are no baselines for a given result.
        """
        result_list: list[dict[str, int | float | str]] = []
        filtered_params: list[LatencyParameters] = [
            p for p in params if isinstance(p, LatencyParameters)
        ]

        for result in results:
            # get the corresponding baseline for the current buffer size
            parameters: LatencyParameters | None = next(
                (p for p in filtered_params if p.buff_size == result.buffer_size), None
            )
            errmsg = (
                f"No test parameters found for {test_name} with buffer size {result.buffer_size}"
            )
            if parameters is None:
                raise RuntimeError(errmsg)
            test_result = True

            expected_cycles = parameters.avg_cycles
            assert expected_cycles > 0, "Expected average cycles must not be zero"
            measured_delta_cycles = abs(
                round((getattr(result, "avg_cycles") - expected_cycles) / expected_cycles, 5)
            )
            expected_time = parameters.avg_time_us
            assert expected_time > 0, "Expected average time must not be zero"
            measured_delta_time = abs(
                round((getattr(result, "avg_time_us") - expected_time) / expected_time, 5)
            )

            # result did not meet the matching parameter, check if within delta.
            if getattr(result, "avg_cycles") > expected_cycles:
                if self.delta_tolerance < measured_delta_cycles:
                    test_result = False
            if getattr(result, "avg_time_us") > expected_time:
                if self.delta_tolerance < measured_delta_time:
                    test_result = False

            result_list.append(
                {
                    "Test Name": test_name,
                    "Buffer Size": parameters.buff_size,
                    "delta tolerance": self.delta_tolerance,
                    "avg_cycles": getattr(result, "avg_cycles"),
                    "avg_cycles target": expected_cycles,
                    "avg_cycles delta": measured_delta_cycles,
                    "avg_time_us": getattr(result, "avg_time_us"),
                    "avg_time_us target": expected_time,
                    "avg_time_us delta": measured_delta_time,
                    "passed": "PASS" if test_result else "FAIL",
                }
            )
        return result_list

    @perf_test
    def cipher_only_tests(self) -> None:
        """Cipher only cryptography device tests.

        Steps:
            * Run dpdk-test-crypto-perf application with configured arguments in cipher only mode.
        Verify:
            * The reported statistics are within is within a defined tolerance of a given baseline.
        """

        def test() -> list[dict[str, int | float | str]]:
            """Run a configured throughput test with the given encryption.

            Args:
                encrypt: The cipher encryption mode to run throughput testing on.

            Returns:
                The generated summary or an empty list if a test is skipped.
            """
            assert combination.cipher_algorithm, "Cipher algorithm required for cipher test."
            cipher_params = CIPHER_ALGORITHM_PARAMS[combination.cipher_algorithm]
            app = Cryptodev(
                ptest=TestType.latency,
                devtype=self.device_type,
                optype=OperationType.cipher_only,
                cipher_algo=combination.cipher_algorithm,
                cipher_op=combination.cipher_op or EncryptDecryptSwitch.encrypt,
                cipher_key_sz=default_if_none(
                    combination.cipher_key_size, cipher_params["key_size"]
                ),
                cipher_iv_sz=default_if_none(combination.cipher_iv_size, cipher_params["iv_size"]),
                digest_sz=combination.digest_size,
                total_ops=default_if_none(combination.ops, self.ops),
                buffer_sz=self.buffer_sizes[combination.name],
            )
            if combination.virtual_device:
                app.update_params(
                    vdevs=[VirtualDevice(combination.virtual_device)],
                    devtype=get_device_from_str(combination.virtual_device),
                )

            self._logger.info(f"Running configured test: {test_name}")
            try:
                return self._create_summary(
                    results=app.run_app(num_vfs=0 if combination.virtual_device else 1),
                    params=combination.test_parameters or config_list,
                    test_name=combination.name,
                )
            # A skip on one test will stop the execution of all tests, return empty summary.
            except SkippedTestException as e:
                self._logger.error(f"test {test_name} skipped: {str(e)}")
                return []

        test_cases_skipped: int = 0
        failed: bool = False
        reason: str = ""
        # Execute all cipher only tests and verify
        for combination in self.cipher_tests:
            test_name = combination.name
            combination_results = [test()]
            if all(result == [] for result in combination_results):
                test_cases_skipped += 1
            else:
                try:
                    self._print_and_verify(combination_results)
                except TestCaseVerifyError as e:
                    failed = True
                    reason = f"{reason}\n{str(e)}" if reason else str(e)
        if failed:
            fail(reason)
        if test_cases_skipped == len(self.cipher_tests):
            skip("All configured test cases skipped.")

    @perf_test
    def auth_only_tests(self) -> None:
        """Authentication cryptography device tests.

        Steps:
            * Run dpdk-test-crypto-perf application with configured parameters in authentication
                only moode.
        Verify:
            * The resulting statistics are within a delta tolerance of a provided baseline.
        """

        def test() -> list[dict[str, int | float | str]]:
            """Run a configured throughput test with the given authentication mode.

            Args:
                op_mode: The authentication op mode to run testing in.

            Returns:
                The generated summary or an empty list if a test is skipped.
            """
            assert combination.auth_algorithm, "Authentication algorithm required for auth test."
            auth_params = AUTHENTICATION_ALGORITHM_PARAMS[combination.auth_algorithm]
            app = Cryptodev(
                ptest=TestType.latency,
                devtype=self.device_type,
                optype=OperationType.auth_only,
                auth_algo=combination.auth_algorithm,
                auth_op=combination.auth_op or AuthenticationOpMode.generate,
                auth_key_sz=default_if_none(combination.auth_key_size, auth_params["key_size"]),
                auth_iv_sz=default_if_none(combination.auth_iv_size, auth_params["iv_size"]),
                total_ops=default_if_none(combination.ops, self.ops),
                digest_sz=combination.digest_size,
                buffer_sz=self.buffer_sizes[combination.name],
            )
            if combination.virtual_device:
                app.update_params(
                    vdevs=[VirtualDevice(combination.virtual_device)],
                    devtype=get_device_from_str(combination.virtual_device),
                )

            self._logger.info(f"Running configured test: {test_name}")
            try:
                return self._create_summary(
                    results=app.run_app(num_vfs=0 if combination.virtual_device else 1),
                    params=combination.test_parameters or config_list,
                    test_name=combination.name,
                )
            # A skip on one test will stop the execution of all tests, return empty summary.
            except SkippedTestException as e:
                self._logger.error(f"failed to run test {test_name}: {str(e)}")
                return []

        test_cases_skipped: int = 0
        failed: bool = False
        reason: str = ""
        # Execute all auth only tests and verify
        for combination in self.auth_tests:
            test_name = combination.name
            combination_results = [(test())]
            if all(result == [] for result in combination_results):
                test_cases_skipped += 1
            else:
                try:
                    self._print_and_verify(combination_results)
                except TestCaseVerifyError as e:
                    failed = True
                    reason = f"{reason}\n{str(e)}" if reason else str(e)
        if failed:
            fail(reason)
        if test_cases_skipped == len(self.auth_tests):
            skip("All configured test cases skipped.")

    @perf_test
    def aead_test(self) -> None:
        """Aead cryptography test.

        Steps:
            * Run dpdk-test-crypto-perf application with configured parameters in aead mode.
        Verify:
            * The resulting statistics are within a delta tolerance of a given baseline.
        """

        def test() -> list[dict[str, int | float | str]]:
            """Execute the dpdk-test-crypto application and verify.

            Args:
                encrypt: The authentication operation mode to run the cryptodev application in.

            Returns:
                The generated summary or an empty list if a test is skipped.
            """
            assert combination.aead_algorithm, "Aead algorithm required for aead test."
            aead_params = AEAD_ALGORITHM_PARAMS[combination.aead_algorithm]
            app = Cryptodev(
                ptest=TestType.latency,
                devtype=self.device_type,
                optype=OperationType.aead,
                aead_algo=combination.aead_algorithm,
                aead_op=combination.aead_op or EncryptDecryptSwitch.encrypt,
                aead_key_sz=default_if_none(combination.aead_key_size, aead_params["key_size"]),
                aead_iv_sz=default_if_none(combination.aead_iv_size, aead_params["iv_size"]),
                aead_aad_sz=default_if_none(combination.aead_aad_size, aead_params["aad_size"]),
                digest_sz=default_if_none(combination.digest_size, aead_params["digest_size"]),
                total_ops=default_if_none(combination.ops, self.ops),
                buffer_sz=self.buffer_sizes[combination.name],
            )
            if combination.virtual_device:
                app.update_params(
                    vdevs=[VirtualDevice(combination.virtual_device)],
                    devtype=get_device_from_str(combination.virtual_device),
                )
            if combination.out_of_place:
                app.update_params(
                    out_of_place=combination.out_of_place,
                )

            self._logger.info(f"Running configured test: {test_name}")
            try:
                return self._create_summary(
                    results=app.run_app(num_vfs=0 if combination.virtual_device else 1),
                    params=combination.test_parameters or config_list,
                    test_name=combination.name,
                )
            # Exceptions will stop the execution of all tests, return an empty result and continue.
            except SkippedTestException as e:
                self._logger.error(f"failed to run test {test_name}: {str(e)}")
                return []

        test_cases_skipped: int = 0
        failed: bool = False
        reason: str = ""
        # Execute all aead tests and verify
        for combination in self.aead_tests:
            test_name = combination.name
            combination_results = [test()]
            if all(result == [] for result in combination_results):
                test_cases_skipped += 1
            else:
                try:
                    self._print_and_verify(combination_results)
                except TestCaseVerifyError as e:
                    failed = True
                    reason = f"{reason}\n{str(e)}" if reason else str(e)
        if failed:
            fail(reason)
        if test_cases_skipped == len(self.aead_tests):
            skip("All configured test cases skipped.")

    @perf_test
    def cipher_and_auth_tests(self) -> None:
        """Cipher and Authentication tests.

        Steps:
            * Run dpdk-test-crypto-perf application in cipher then authentication mode
        Verify:
            * The resulting statistics are within a delta tolerance of a given baseline.
        """

        def test() -> list[dict[str, int | float | str]]:
            """Execute the dpdk-test-crypto application and verify.

            Args:
                encrypt: The cipher encryption mode to run latency testing in.
                op_mode: The authentication operation mode to run the cryptodev application in.

            Returns:
                The generated summary or an empty list if a test is skipped.
            """
            assert combination.cipher_algorithm and combination.auth_algorithm, (
                "Cipher and auth algorithms required for cipher then auth test."
            )
            cipher_params = CIPHER_ALGORITHM_PARAMS[combination.cipher_algorithm]
            auth_params = AUTHENTICATION_ALGORITHM_PARAMS[combination.auth_algorithm]
            app = Cryptodev(
                ptest=TestType.latency,
                devtype=self.device_type,
                optype=OperationType.cipher_then_auth,
                cipher_algo=combination.cipher_algorithm,
                cipher_op=combination.cipher_op or EncryptDecryptSwitch.encrypt,
                cipher_key_sz=default_if_none(
                    combination.cipher_key_size, cipher_params["key_size"]
                ),
                cipher_iv_sz=default_if_none(combination.cipher_iv_size, cipher_params["iv_size"]),
                auth_algo=combination.auth_algorithm,
                auth_op=AuthenticationOpMode.generate,
                auth_key_sz=default_if_none(combination.auth_key_size, auth_params["key_size"]),
                auth_iv_sz=default_if_none(combination.auth_iv_size, auth_params["iv_size"]),
                total_ops=default_if_none(combination.ops, self.ops),
                digest_sz=combination.digest_size,
                buffer_sz=self.buffer_sizes[combination.name],
            )
            if combination.virtual_device:
                app.update_params(
                    vdevs=[VirtualDevice(combination.virtual_device)],
                    devtype=get_device_from_str(combination.virtual_device),
                )

            self._logger.info(f"Running configured test: {test_name}")
            try:
                return self._create_summary(
                    results=app.run_app(num_vfs=0 if combination.virtual_device else 1),
                    params=combination.test_parameters or config_list,
                    test_name=combination.name,
                )
            # Exceptions will stop the execution of all tests, return an empty result and continue.
            except SkippedTestException as e:
                self._logger.error(f"failed to run test {test_name}: {str(e)}")
                return []

        test_cases_skipped: int = 0
        failed: bool = False
        reason: str = ""
        # Execute all cipher_then_auth tests and verify
        for combination in self.cipher_then_auth_tests:
            test_name = combination.name
            combination_results = [test()]
            if all(result == [] for result in combination_results):
                test_cases_skipped += 1
            else:
                try:
                    self._print_and_verify(combination_results)
                except TestCaseVerifyError as e:
                    failed = True
                    reason = f"{reason}\n{str(e)}" if reason else str(e)
        if failed:
            fail(reason)
        if test_cases_skipped == len(self.cipher_then_auth_tests):
            skip("All configured test cases skipped.")
