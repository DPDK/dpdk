# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 University of New Hampshire

"""DPDK cryptodev verify test suite.

The main goal of this test suite is to utilize the verify mode of dpdk-test-crypto application
to ensure functional correctness for various cryptographic operations supported by DPDK
cryptodev-pmd.
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
    OperationType,
    TestType,
    get_device_from_str,
)
from api.cryptodev.types import (
    CryptodevResults,
)
from api.test import verify
from api.test_suite import TestSuite, perf_test
from api.testbed_model.virtual_device import VirtualDevice
from framework.context import get_ctx

TOTAL_OPS = 10_000_000
AES_CBC_DATA = "test_aes_cbc.data"
AES_GCM_DATA = "test_aes_gcm.data"


@requires_link_topology(LinkTopology.NO_LINK)
class TestCryptodevVerify(TestSuite):
    """DPDK Crypto Device Testing Suite."""

    def set_up_suite(self) -> None:
        """Set up the test suite."""
        self.device_type: DeviceType | None = get_device_from_str(
            str(get_ctx().sut_node.crypto_device_type)
        )

    def _verify_output(
        self,
        results: list[CryptodevResults],
    ) -> bool:
        for result in results:
            if (
                getattr(result, "failed_enqueued") > 0
                or getattr(result, "failed_dequeued") > 0
                or getattr(result, "failed_ops") > 0
            ):
                return False
        return True

    @perf_test
    def aesni_mb_vdev(self) -> None:
        """aesni_mb_vdev test.

        Steps:
            * Create a cryptodev instance with aesni_mb virtual device and provided buffer sizes.
        Verify:
            * The aes_cbc cipher and sha1_hmac authentication algorithms are working as expected
                with the dpdk-test-crypto application.
        """
        app = Cryptodev(
            vdevs=[VirtualDevice("crypto_aesni_mb0")],
            ptest=TestType.verify,
            test_file=AES_CBC_DATA,
            test_name="sha1_hmac_buff_32",
            devtype=DeviceType.crypto_aesni_mb,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=32,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            digest_sz=12,
            burst_sz=32,
            buffer_sz=32,
            total_ops=TOTAL_OPS,
        )

        verify(
            self._verify_output(app.run_app(num_vfs=0)), "Failed to verify test sha1_hmac_buff_32"
        )

    @perf_test
    def openssl_vdev(self) -> None:
        """Openssl vdev test.

        Steps:
            * Create a cryptodev instance with openssl virtual device and provided buffer sizes.
        Verify:
            * The aes_cbc cipher and sha1_hmac authentication algorithms are working as expected
                with the dpdk-test-crypto application.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        app = Cryptodev(
            vdevs=[VirtualDevice("crypto_openssl0")],
            ptest=TestType.verify,
            test_file=AES_GCM_DATA,
            test_name="aes_gcm_buff_32",
            devtype=DeviceType.crypto_openssl,
            optype=OperationType.aead,
            aead_algo=AeadAlgName.aes_gcm,
            aead_op=EncryptDecryptSwitch.encrypt,
            aead_key_sz=16,
            aead_aad_sz=16,
            aead_iv_sz=12,
            digest_sz=16,
            burst_sz=32,
            buffer_sz=32,
            total_ops=TOTAL_OPS,
        )

        verify(self._verify_output(app.run_app(num_vfs=0)), "Failed to verify test aes_gcm_buff_32")

    @perf_test
    def sha1_hmac_buff_32(self) -> None:
        """aes_cbc test.

        Steps:
            * Create a cryptodev instance with provided device type and buffer sizes.
        Verify:
            * The aes_cbc cipher and sha1_hmac authentication algorithms are working as expected
                with the dpdk-test-crypto application.

        Raises:
            SkippedTestException: When configuration is not provided.
        """
        app = Cryptodev(
            ptest=TestType.verify,
            test_file=AES_CBC_DATA,
            test_name="sha1_hmac_buff_32",
            devtype=self.device_type,
            optype=OperationType.cipher_then_auth,
            cipher_algo=CipherAlgorithm.aes_cbc,
            cipher_op=EncryptDecryptSwitch.encrypt,
            cipher_key_sz=32,
            cipher_iv_sz=16,
            auth_algo=AuthenticationAlgorithm.sha1_hmac,
            auth_op=AuthenticationOpMode.generate,
            auth_key_sz=64,
            digest_sz=20,
            burst_sz=32,
            buffer_sz=32,
            total_ops=TOTAL_OPS,
        )

        verify(self._verify_output(app.run_app()), "Failed to verify test sha1_hmac_buff_32")
