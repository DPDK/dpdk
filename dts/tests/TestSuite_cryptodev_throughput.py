# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire

"""DPDK cryptodev performance test suite.

The main goal of this test suite is to utilize the dpdk-test-cryptodev application to gather
performance metrics for various cryptographic operations supported by DPDK cryptodev-pmd.
It will then compare the results against predefined baseline given in the test_config file to
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
from api.test import verify
from framework.context import get_ctx
from framework.exception import SkippedTestException
from framework.test_suite import BaseConfig, TestSuite, crypto_test
from framework.testbed_model.virtual_device import VirtualDevice

config_list: list[dict[str, int | float | str]] = [
    {"buff_size": 64, "Gbps": 1.00},
    {"buff_size": 512, "Gbps": 1.00},
    {"buff_size": 2048, "Gbps": 1.00},
]

TOTAL_OPS = 10_000_000


class Config(BaseConfig):
    """Performance test metrics.

    Attributes:
        delta_tolerance: The allowed tolerance below a given baseline.
        throughput_test_parameters: The test parameters to use in the test suite.
    """

    delta_tolerance: float = 0.05

    throughput_test_parameters: dict[str, list[dict[str, int | float | str]]] = {
        "aes_cbc": config_list,
        "aes_cbc_sha1": config_list,
        "aes_cbc_sha2": config_list,
        "aes_cbc_sha2_digest_16": config_list,
        "aead_aes_gcm": config_list,
        "aes_docsisbpi": config_list,
        "sha1_hmac": config_list,
        "snow3g_uea2_snow3g_uia2": config_list,
        "zuc_eea3_zuc_eia3": config_list,
        "kasumi_f8_kasumi_f9": config_list,
        "open_ssl_vdev": config_list,
        "aesni_mb_vdev": config_list,
        "aesni_gcm_vdev": config_list,
        "kasumi_vdev": config_list,
        "zuc_vdev": config_list,
        "snow3g_vdev": config_list,
    }


@requires_link_topology(LinkTopology.NO_LINK)
class TestCryptodevThroughput(TestSuite):
    """DPDK Crypto Device Testing Suite."""

    config: Config

    def set_up_suite(self) -> None:
        """Set up the test suite."""
        self.throughput_test_parameters: dict[str, list[dict[str, int | float | str]]] = (
            self.config.throughput_test_parameters
        )
        self.delta_tolerance: float = self.config.delta_tolerance
        self.device_type: DeviceType | None = get_device_from_str(
            str(get_ctx().sut_node.crypto_device_type)
        )
        self.buffer_sizes = {}

        for k, v in self.throughput_test_parameters.items():
            self.buffer_sizes[k] = ListWrapper([int(run["buff_size"]) for run in v])

    def _print_stats(self, test_vals: list[dict[str, int | float | str]]) -> None:
        element_len = len("Delta Tolerance")
        border_len = (element_len + 1) * (len(test_vals[0]))

        print(f"{'Throughput Results'.center(border_len)}\n{'=' * border_len}")
        for k, v in test_vals[0].items():
            print(f"|{k.title():<{element_len}}", end="")
        print(f"|\n{'='*border_len}")

        for test_val in test_vals:
            for k, v in test_val.items():
                print(f"|{v:<{element_len}}", end="")
            print(f"|\n{'='*border_len}")

    def _verify_throughput(
        self,
        results: list[CryptodevResults],
        key: str,
    ) -> list[dict[str, int | float | str]]:
        result_list: list[dict[str, int | float | str]] = []

        for result in results:
            # get the corresponding baseline for the current buffer size
            parameters: dict[str, int | float | str] = list(
                filter(
                    lambda x: x["buff_size"] == result.buffer_size,
                    self.throughput_test_parameters[key],
                )
            )[0]
            test_result = True
            expected_gbps = parameters["Gbps"]
            measured_delta = abs(
                round((getattr(result, "gbps") - expected_gbps) / expected_gbps, 5)
            )

            # result did not meet the given Gbps parameter, check if within delta.
            if getattr(result, "gbps") < expected_gbps:
                if self.delta_tolerance < measured_delta:
                    test_result = False
            result_list.append(
                {
                    "Buffer Size": parameters["buff_size"],
                    "Gbps delta": measured_delta,
                    "delta tolerance": self.delta_tolerance,
                    "Gbps": getattr(result, "gbps"),
                    "Gbps target": expected_gbps,
                    "passed": "PASS" if test_result else "FAIL",
                }
            )
        return result_list

    @crypto_test
    def aes_cbc(self) -> None:
        """aes_cbc test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            devtype=self.device_type,
            optype=OperationType.cipher_only,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc"],
        )
        results = self._verify_throughput(app.run_app(), "aes_cbc")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def aes_cbc_sha1(self) -> None:
        """aes_cbc_sha1 test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc_sha1" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            auth_iv_sz=20,
            digest_sz=12,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc_sha1"],
        )
        results = self._verify_throughput(app.run_app(), "aes_cbc_sha1")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def aes_cbc_sha2(self) -> None:
        """aes_cbc_sha2 test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc_sha2" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc_sha2"],
        )
        results = self._verify_throughput(app.run_app(), "aes_cbc_sha2")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def aes_cbc_sha2_digest_16(self) -> None:
        """aes_cbc_sha2_digest_16 test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_cbc_sha2_digest_16" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=16,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.sha2_256_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            digest_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_cbc_sha2_digest_16"],
        )
        results = self._verify_throughput(app.run_app(), "aes_cbc_sha2_digest_16")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def aead_aes_gcm(self) -> None:
        """aead_aes_gcm test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aead_aes_gcm" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            devtype=self.device_type,
            optype=OperationType.aead,
            aead_algo=AeadAlgName.aes_gcm,
            aead_op=EncryptDecryptSwitch.encrypt,
            aead_key_sz=16,
            aead_iv_sz=12,
            aead_aad_sz=16,
            digest_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aead_aes_gcm"],
        )
        results = self._verify_throughput(app.run_app(), "aead_aes_gcm")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def aes_docsisbpi(self) -> None:
        """aes_docsisbpi test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aes_docsisbpi" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            devtype=self.device_type,
            optype=OperationType.cipher_only,
            cipher_algo=CipherAlgorithm.aes_docsisbpi,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=32,
            cipher_iv_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aes_docsisbpi"],
        )
        results = self._verify_throughput(app.run_app(), "aes_docsisbpi")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def sha1_hmac(self) -> None:
        """sha1_hmac test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "sha1_hmac" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            devtype=self.device_type,
            optype=OperationType.auth_only,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            auth_iv_sz=16,
            digest_sz=12,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["sha1_hmac"],
        )
        results = self._verify_throughput(app.run_app(), "sha1_hmac")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def snow3g_uea2_snow3g_uia2(self) -> None:
        """snow3g_uea2_snow3g_uia2 test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "snow3g_uea2_snow3g_uia2" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
        results = self._verify_throughput(app.run_app(), "snow3g_uea2_snow3g_uia2")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def zuc_eea3_zuc_eia3(self) -> None:
        """zuc_eea3_zuc_eia3 test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "zuc_eea3_zuc_eia3" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
        results = self._verify_throughput(app.run_app(), "zuc_eea3_zuc_eia3")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def kasumi_f8_kasumi_f9(self) -> None:
        """kasumi_f8 kasumi_f9 test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "kasumi_f8_kasumi_f9" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
        results = self._verify_throughput(app.run_app(), "kasumi_f8_kasumi_f9")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    # BEGIN VDEV TESTS

    @crypto_test
    def aesni_mb_vdev(self) -> None:
        """aesni_mb virtual device test.

        Steps:
            * Create a cryptodev instance with crypto_aesni_mb and supplied buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aesni_mb_vdev" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            vdevs=[VirtualDevice("crypto_aesni_mb0")],
            devtype=DeviceType.crypto_aesni_mb,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=32,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=16,
            auth_iv_sz=16,
            digest_sz=4,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aesni_mb_vdev"],
        )
        results = self._verify_throughput(app.run_app(num_vfs=0), "aesni_mb_vdev")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def aesni_gcm_vdev(self):
        """aesni_gcm virtual device test.

        Steps:
            * Create a cryptodev instance with crypto_aesni_gcm and supplied buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "aesni_gcm_vdev" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
            vdevs=[VirtualDevice("crypto_aesni_gcm0")],
            devtype=DeviceType.crypto_aesni_gcm,
            optype=OperationType.aead,
            aead_algo=AeadAlgName.aes_gcm,
            aead_op=EncryptDecryptSwitch.encrypt,
            aead_key_sz=16,
            aead_iv_sz=12,
            aead_aad_sz=16,
            digest_sz=16,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["aesni_gcm_vdev"],
        )
        results = self._verify_throughput(app.run_app(num_vfs=0), "aesni_gcm_vdev")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def kasumi_vdev(self) -> None:
        """Kasmumi virtual device test.

        Steps:
            * Create a cryptodev instance with crypto_kasumi and supplied buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "kasumi_vdev" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            vdevs=[VirtualDevice("crypto_kasumi0")],
            ptest=TestType.throughput,
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
        results = self._verify_throughput(app.run_app(num_vfs=0), "kasumi_vdev")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def snow3g_vdev(self) -> None:
        """snow3g virtual device test.

        Steps:
            * Create a cryptodev instance with crypto_snow3g and supplied buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "snow3g_vdev" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
            digest_sz=4,
            burst_sz=32,
            total_ops=TOTAL_OPS,
            buffer_sz=self.buffer_sizes["snow3g_vdev"],
        )
        results = self._verify_throughput(app.run_app(num_vfs=0), "snow3g_vdev")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def zuc_vdev(self) -> None:
        """Zuc virtual device test.

        Steps:
            * Create a cryptodev instance with crypto_zuc and supplied buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "zuc_vdev" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
        results = self._verify_throughput(app.run_app(num_vfs=0), "zuc_vdev")
        self._print_stats(results)
        for result in results:
            verify(result["passed"] == "PASS", "Gbps fell below delta tolerance")

    @crypto_test
    def open_ssl_vdev(self) -> None:
        """open_ssl virtual device test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The resulting Gbps is greater than expected_gbps*(1-delta_tolerance).

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        if "open_ssl_vdev" not in self.throughput_test_parameters:
            raise SkippedTestException("test not configured")
        app = Cryptodev(
            ptest=TestType.throughput,
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
        results = self._verify_throughput(app.run_app(num_vfs=0), "open_ssl_vdev")
        self._print_stats(results)
        for result in results:
            verify(
                result["passed"] == "PASS",
                "Gbps fell below delta tolerance",
            )
