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
from api.cryptodev import Cryptodev
from api.cryptodev.config import (
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
)
from api.test import skip, verify
from framework.context import get_ctx
from framework.test_suite import BaseConfig, TestSuite, crypto_test
from framework.testbed_model.virtual_device import VirtualDevice

config_list: list[dict[str, int | float | str]] = [
    {"buff_size": 64, "avg_cycles": 99_999.00, "avg_time_us": 9999.0},
    {"buff_size": 512, "avg_cycles": 99_999.00, "avg_time_us": 9999.0},
    {"buff_size": 2048, "avg_cycles": 99_999.00, "avg_time_us": 9999.0},
]

TOTAL_OPS = 10_000_000


class Config(BaseConfig):
    """Performance test metrics.

    Attributes:
        delta_tolerance: The allowed tolerance below a given baseline.
        latency_test_parameters: The test parameters to use in the test suite.
    """

    delta_tolerance: float = 0.05

    latency_test_parameters: dict[str, list[dict[str, int | float | str]]] = {
        "aes_cbc": config_list,
        "aes_cbc_sha1_hmac": config_list,
        "aes_cbc_sha2_hmac": config_list,
        "aes_docsisbpi": config_list,
        "aes_gcm": config_list,
        "kasumi_f8_kasumi_f9": config_list,
        "snow3g_uea2_snow3g_uia2": config_list,
        "zuc_eea3_zuc_eia3": config_list,
        "aesni_gcm_vdev": config_list,
        "aesni_mb_cipher_then_auth_vdev": config_list,
        "aesni_mb_vdev": config_list,
        "kasumi_vdev": config_list,
        "open_ssl_vdev": config_list,
        "snow3g_vdev": config_list,
        "zuc_vdev": config_list,
    }


@requires_link_topology(LinkTopology.NO_LINK)
class TestCryptodevLatency(TestSuite):
    """DPDK Crypto Device Testing Suite."""

    config: Config

    def set_up_suite(self) -> None:
        """Set up the test suite."""
        self.latency_test_parameters: dict[str, list[dict[str, int | float | str]]] = (
            self.config.latency_test_parameters
        )
        self.delta_tolerance: float = self.config.delta_tolerance
        self.device_type: DeviceType | None = get_device_from_str(
            str(get_ctx().sut_node.crypto_device_type)
        )
        self.buffer_sizes = {}

        for k, v in self.latency_test_parameters.items():
            self.buffer_sizes[k] = ListWrapper([int(run["buff_size"]) for run in v])

    def _print_stats(self, test_vals: list[dict[str, int | float | str]]) -> None:
        element_len = len("Avg Time us Target")
        border_len = (element_len + 1) * (len(test_vals[0]))

        print(f"{'Latency Results'.center(border_len)}\n{'=' * border_len}")
        for k, v in test_vals[0].items():
            print(f"|{k.title():<{element_len}}", end="")
        print(f"|\n{'='*border_len}")

        for test_val in test_vals:
            for k, v in test_val.items():
                print(f"|{v:<{element_len}}", end="")
            print(f"|\n{'='*border_len}")

    def _verify_latency(
        self,
        results: list[CryptodevResults],
        key: str,
    ) -> list[dict[str, int | float | str]]:
        result_list: list[dict[str, int | float | str]] = []

        for result in results:
            # get the corresponding baseline for the current buffer size
            parameters: dict[str, int | float | str] = next(
                filter(
                    lambda x: x["buff_size"] == result.buffer_size,
                    self.latency_test_parameters[key],
                ),
                {},
            )
            if parameters == {}:
                raise RuntimeError(
                    f"No test parameters found for {key} with buffer size {result.buffer_size}"
                )
            test_result = True
            expected_cycles = parameters["avg_cycles"]
            expected_time_us = parameters["avg_time_us"]
            measured_time_delta = abs(
                (getattr(result, "avg_time_us") - expected_time_us) / expected_time_us
            )
            measured_cycles_delta = abs(
                (getattr(result, "avg_cycles") - expected_cycles) / expected_cycles
            )

            # result did not meet the given cycles parameter, check if within delta.
            if getattr(result, "avg_cycles") > expected_cycles:
                if self.delta_tolerance < measured_cycles_delta:
                    test_result = False
            # result did not meet the given time parameter, check if within delta.
            if getattr(result, "avg_time_us") > expected_time_us:
                if self.delta_tolerance < measured_time_delta:
                    test_result = False
            result_list.append(
                {
                    "Buffer Size": parameters["buff_size"],
                    "delta tolerance": self.delta_tolerance,
                    "cycles delta": round(measured_cycles_delta, 5),
                    "Avg cycles": round(getattr(result, "avg_cycles"), 5),
                    "Avg cycles target": expected_cycles,
                    "time delta": round(measured_time_delta, 5),
                    "Avg time us": round(getattr(result, "avg_time_us"), 5),
                    "Avg time us target": expected_time_us,
                    "passed": "PASS" if test_result else "FAIL",
                }
            )
        return result_list

    @crypto_test
    def aes_cbc(self) -> None:
        """aes_cbc latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_only,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc"],
        )
        results = self._verify_latency(app.run_app(), "aes_cbc")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def aes_cbc_sha1_hmac(self) -> None:
        """aes_cbc_sha1_hmac latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc_sha1_hmac" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            digest_sz=12,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc_sha1_hmac"],
        )
        results = self._verify_latency(app.run_app(), "aes_cbc_sha1_hmac")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def aes_cbc_sha2_hmac(self) -> None:
        """aes_cbc_sha2_hmac latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc_sha2_hmac" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.sha2_256_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            digest_sz=32,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc_sha2_hmac"],
        )
        results = self._verify_latency(app.run_app(), "aes_cbc_sha2_hmac")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def aes_docsisbpi(self) -> None:
        """aes_docsisbpi latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_docsisbpi" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_only,
            cipher_algo=CipherAlgorithm.aes_docsisbpi,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=32,
            cipher_iv_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_docsisbpi"],
        )
        results = self._verify_latency(app.run_app(), "aes_docsisbpi")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def aes_gcm(self) -> None:
        """aes_gcm latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_gcm" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.aead,
            aead_algo=AeadAlgName.aes_gcm,
            aead_op=EncryptDecryptSwitch.encrypt,
            aead_key_sz=16,
            aead_iv_sz=12,
            aead_aad_sz=16,
            digest_sz=16,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_gcm"],
        )
        results = self._verify_latency(app.run_app(), "aes_gcm")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def kasumi_f8_kasumi_f9(self) -> None:
        """kasumi_f8_kasumi_f9 latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "kasumi_f8_kasumi_f9" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.kasumi_f8,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=8,
            auth_algo=AuthenticationAlgorithm.kasumi_f9,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            digest_sz=4,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["kasumi_f8_kasumi_f9"],
        )
        results = self._verify_latency(app.run_app(), "kasumi_f8_kasumi_f9")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def snow3g_uea2_snow3g_uia2(self) -> None:
        """snow3g_uea2_snow3g_uia2 latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "snow3g_uea2_snow3g_uia2" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.snow3g_uea2,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.snow3g_uia2,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            auth_iv_sz=16,
            digest_sz=4,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["snow3g_uea2_snow3g_uia2"],
        )
        results = self._verify_latency(app.run_app(), "snow3g_uea2_snow3g_uia2")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def zuc_eea3_zuc_eia3(self) -> None:
        """zuc_eea3_zuc_eia3 cipher and auth latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "zuc_eea3_zuc_eia3" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.zuc_eea3,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.zuc_eia3,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            auth_iv_sz=16,
            digest_sz=4,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["zuc_eea3_zuc_eia3"],
        )
        results = self._verify_latency(app.run_app(), "zuc_eea3_zuc_eia3")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    # BEGIN VDEV TESTS

    @crypto_test
    def aesni_gcm_vdev(self) -> None:
        """aesni_gcm virtual device latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aesni_gcm_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_aesni_gcm0")],
            devtype=DeviceType.crypto_aesni_gcm,
            optype=OperationType.aead,
            aead_op=EncryptDecryptSwitch.encrypt,
            aead_key_sz=16,
            aead_iv_sz=12,
            aead_aad_sz=16,
            digest_sz=16,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aesni_gcm_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "aesni_gcm_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def aesni_mb_cipher_then_auth_vdev(self) -> None:
        """aesni_mb vdev cipher and auth latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aesni_mb_cipher_then_auth_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_aesni_mb0")],
            devtype=DeviceType.crypto_aesni_mb,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            digest_sz=12,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aesni_mb_cipher_then_auth_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "aesni_mb_cipher_then_auth_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def aesni_mb_vdev(self) -> None:
        """aesni_mb vdev latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aesni_mb_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_aesni_mb0")],
            devtype=DeviceType.crypto_aesni_mb,
            optype=OperationType.cipher_only,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aesni_mb_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "aesni_mb_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def kasumi_vdev(self) -> None:
        """Kasumi vdev latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "kasumi_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_kasumi0")],
            devtype=DeviceType.crypto_kasumi,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.kasumi_f8,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=8,
            auth_algo=AuthenticationAlgorithm.kasumi_f9,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            digest_sz=4,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["kasumi_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "kasumi_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def open_ssl_vdev(self) -> None:
        """open_ssl vdev latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "open_ssl_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_openssl0")],
            devtype=DeviceType.crypto_openssl,
            optype=OperationType.aead,
            aead_algo=AeadAlgName.aes_gcm,
            aead_op=EncryptDecryptSwitch.encrypt,
            aead_key_sz=16,
            aead_iv_sz=16,
            aead_aad_sz=16,
            digest_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["open_ssl_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "open_ssl_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def snow3g_vdev(self) -> None:
        """snow3g vdev latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "snow3g_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_snow3g0")],
            devtype=DeviceType.crypto_snow3g,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.snow3g_uea2,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.snow3g_uia2,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            auth_iv_sz=16,
            digest_sz=16,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["snow3g_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "snow3g_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )

    @crypto_test
    def zuc_vdev(self) -> None:
        """Zuc vdev latency test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The latency is below or within delta of provided baseline.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "zuc_vdev" not in self.latency_test_parameters:
            skip("test not configured")
        app = Cryptodev(
            ptest=TestType.latency,
            vdevs=[VirtualDevice("crypto_zuc0")],
            devtype=DeviceType.crypto_zuc,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.zuc_eea3,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.zuc_eia3,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            auth_iv_sz=16,
            digest_sz=4,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["zuc_vdev"],
        )
        results = self._verify_latency(app.run_app(num_vfs=0), "zuc_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "latency was greater than the delta tolerance above baseline",
            )
