# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 University of New Hampshire
"""Module containing types and parameter classes for cryptodev-pmd application."""

from dataclasses import dataclass, field
from enum import auto
from typing import Literal

from framework.params import Params, Switch
from framework.params.eal import EalParams
from framework.utils import StrEnum

Silent = Literal[""]


class DeviceType(StrEnum):
    """Enum for cryptodev device types.

    Attributes:
        crypto_aesni_gcm: AES-NI GCM device type.
        crypto_aesni_mb: AES-NI MB device type.
        crypto_armv8: ARMv8 device type.
        crypto_cn10k: CN10K device type.
        crypto_cn9k: CN9K device type.
        crypto_dpaa_sec: DPAA SEC device type.
        crypto_dpaa2_sec: DPAA2 SEC device type.
        crypto_kasumi: KASUMI device type.
        crypto_mvsam: MVSAM device type.
        crypto_null: NULL device type.
        crypto_octeontx: OCTEONTX device type.
        crypto_openssl: OpenSSL device type.
        crypto_qat: QAT device type.
        crypto_scheduler: Scheduler device type.
        crypto_snow3g: SNOW3G device type.
        crypto_zuc: ZUC device type.
    """

    crypto_aesni_gcm = auto()
    crypto_aesni_mb = auto()
    crypto_armv8 = auto()
    crypto_cn10k = auto()
    crypto_cn9k = auto()
    crypto_dpaa_sec = auto()
    crypto_dpaa2_sec = auto()
    crypto_kasumi = auto()
    crypto_mvsam = auto()
    crypto_null = auto()
    crypto_octeontx = auto()
    crypto_openssl = auto()
    crypto_qat = auto()
    crypto_scheduler = auto()
    crypto_snow3g = auto()
    crypto_zuc = auto()


def get_device_from_str(device: str) -> DeviceType | None:
    """Get a device type from the given string.

    Args:
        device: the string representation of the desired device.

    Returns:
        the device as a DeviceType object.
    """
    for item in DeviceType:
        if device in item.value:
            return item
    return None


class OperationType(StrEnum):
    """Enum for cryptodev operation types.

    Attributes:
        aead: AEAD operation type.
        auth_only: Authentication only operation type.
        auth_then_cipher: Authentication then cipher operation type.
        cipher_only: Cipher only operation type.
        cipher_then_auth: Cipher then authentication operation type.
        docsis: DOCSIS operation type.
        ecdsa_p192r1 = ECDSA P-192R1 operation type.
        ecdsa_p224r1 = ECDSA P-224R1 operation type.
        ecdsa_p256r1: ECDSA P-256R1 operation type.
        ecdsa_p384r1 = ECDSA P-384R1 operation type.
        ecdsa_p521r1 = ECDSA P-521R1 operation type.
        eddsa_25519: EdDSA 25519 operation type.
        modex: Modex operation type.
        ipsec: IPsec operation type.
        pdcp: PDCP operation type.
        rsa: RSA operation type.
        sm2: SM2 operation type.
        tls_record: TLS record operation type.
    """

    aead = auto()
    auth_only = "auth-only"
    auth_then_cipher = "auth-then-cipher"
    cipher_only = "cipher-only"
    cipher_then_auth = "cipher-then-auth"
    docsis = auto()
    ecdsa_p192r1 = auto()
    ecdsa_p224r1 = auto()
    ecdsa_p256r1 = auto()
    ecdsa_p384r1 = auto()
    ecdsa_p521r1 = auto()
    eddsa_25519 = auto()
    modex = auto()
    ipsec = auto()
    pdcp = auto()
    rsa = auto()
    sm2 = auto()
    tls_record = "tls-record"

    def __str__(self) -> str:
        """Override str to return the value."""
        return self.value


class CipherAlgorithm(StrEnum):
    """Enum for cryptodev cipher algorithms.

    Attributes:
        aes_cbc: AES CBC cipher algorithm.
        aes_ctr: AES CTR cipher algorithm.
        aes_docsisbpi: AES DOCSIS BPI cipher algorithm.
        aes_ecb: AES ECB cipher algorithm.
        aes_f8: AES F8 cipher algorithm.
        aes_xts: AES XTS cipher algorithm.
        arc4: ARC4 cipher algorithm.
        null: NULL cipher algorithm.
        three_des_cbc: 3DES CBC cipher algorithm.
        three_des_ctr: 3DES CTR cipher algorithm.
        three_des_ecb: 3DES ECB cipher algorithm.
        kasumi_f8: KASUMI F8 cipher algorithm.
        snow3g_uea2: SNOW3G UEA2 cipher algorithm.
        zuc_eea3: ZUC EEA3 cipher algorithm.
    """

    aes_cbc = "aes-cbc"
    aes_ctr = "aes-ctr"
    aes_docsisbpi = "aes-docsisbpi"
    aes_ecb = "aes-ecb"
    aes_f8 = "aes-f8"
    aes_gcm = "aes-gcm"
    aes_xts = "aes-xts"
    arc4 = auto()
    null = auto()
    three_des_cbc = "3des-cbc"
    three_des_ctr = "3des-ctr"
    three_des_ecb = "3des-ecb"
    kasumi_f8 = "kasumi-f8"
    snow3g_uea2 = "snow3g-uea2"
    zuc_eea3 = "zuc-eea3"

    def __str__(self):
        """Override str to return the value."""
        return self.value


class AuthenticationAlgorithm(StrEnum):
    """Enum for cryptodev authentication algorithms.

    Attributes:
        aes_cbc_mac: AES CBC MAC authentication algorithm.
        aes_cmac: AES CMAC authentication algorithm.
        aes_gmac: AES GMAC authentication algorithm.
        aes_xcbc_mac: AES XCBC MAC authentication algorithm.
        kasumi_f9: KASUMI F9 authentication algorithm.
        md5: MD5 authentication algorithm.
        md5_hmac: MD5 HMAC authentication algorithm.
        sha1: SHA1 authentication algorithm.
        sha1_hmac: SHA1 HMAC authentication algorithm.
        sha2_224: SHA2-224 authentication algorithm.
        sha2_224_hmac: SHA2-224 HMAC authentication algorithm.
        sha2_256: SHA2-256 authentication algorithm.
        sha2_256_hmac: SHA2-256 HMAC authentication algorithm.
        sha2_384: SHA2-384 authentication algorithm.
        sha2_384_hmac: SHA2-384 HMAC authentication algorithm.
        sha2_512: SHA2-512 authentication algorithm.
        sha2_512_hmac: SHA2-512 HMAC authentication algorithm.
        snow3g_uia2: SNOW3G UIA2 authentication algorithm.
        zuc_eia3: ZUC EIA3 authentication algorithm.
    """

    aes_cbc_mac = "aes-cbc-mac"
    aes_cmac = "aes-cmac"
    aes_gmac = "aes-gmac"
    aes_xcbc_mac = "aes-xcbc-mac"
    kasumi_f9 = "kasumi-f9"
    md5 = auto()
    md5_hmac = "md5-hmac"
    sha1 = auto()
    sha1_hmac = "sha1-hmac"
    sha2_224 = "sha2-224"
    sha2_224_hmac = "sha2-224-hmac"
    sha2_256 = "sha2-256"
    sha2_256_hmac = "sha2-256-hmac"
    sha2_384 = "sha2-384"
    sha2_384_hmac = "sha2-384-hmac"
    sha2_512 = "sha2-512"
    sha2_512_hmac = "sha2-512-hmac"
    snow3g_uia2 = "snow3g-uia2"
    zuc_eia3 = "zuc-eia3"

    def __str__(self) -> str:
        """Override str to return the value."""
        return self.value


class TestType(StrEnum):
    """Enum for cryptodev test types.

    Attributes:
        latency: Latency test type.
        pmd_cyclecount: PMD cyclecount test type.
        throughput: Throughput test type.
        verify: Verify test type.
    """

    latency = auto()
    pmd_cyclecount = "pmd-cyclecount"
    throughput = auto()
    verify = auto()

    def __str__(self) -> str:
        """Override str to return the value."""
        return self.value


class EncryptDecryptSwitch(StrEnum):
    """Enum for cryptodev encrypt/decrypt operations.

    Attributes:
        decrypt: Decrypt operation.
        encrypt: Encrypt operation.
    """

    decrypt = auto()
    encrypt = auto()


class AuthenticationOpMode(StrEnum):
    """Enum for cryptodev authentication operation modes.

    Attributes:
        generate: Generate operation mode.
        verify: Verify operation mode.
    """

    generate = auto()
    verify = auto()


class AeadAlgName(StrEnum):
    """Enum for cryptodev AEAD algorithms.

    Attributes:
        aes_ccm: AES CCM algorithm.
        aes_gcm: AES GCM algorithm.
    """

    aes_ccm = "aes-ccm"
    aes_gcm = "aes-gcm"

    def __str__(self):
        """Override str to return the value."""
        return self.value


class AsymOpMode(StrEnum):
    """Enum for cryptodev asymmetric operation modes.

    Attributes:
        decrypt: Decrypt operation mode.
        encrypt: Encrypt operation mode.
        sign: Sign operation mode.
        verify: Verify operation mode.
    """

    decrypt = auto()
    encrypt = auto()
    sign = auto()
    verify = auto()


class PDCPDomain(StrEnum):
    """Enum for cryptodev PDCP domains.

    Attributes:
        control: Control domain.
        user: User domain.
    """

    control = auto()
    user = auto()


class RSAPrivKeyType(StrEnum):
    """Enum for cryptodev RSA private key types.

    Attributes:
        exp: Exponent key type.
        qt: QT key type.
    """

    exp = auto()
    qt = auto()


class TLSVersion(StrEnum):
    """Enum for cryptodev TLS versions.

    Attributes:
        DTLS1_2: DTLS 1.2 version.
        TLS1_2: TLS 1.2 version.
        TLS1_3: TLS 1.3 version.
    """

    DTLS1_2 = "DTLS1.2"
    TLS1_2 = "TLS1.2"
    TLS1_3 = "TLS1.3"

    def __str__(self):
        """Override str to return the value."""
        return self.value


class RSAPrivKeytype(StrEnum):
    """Enum for cryptodev RSA private key types.

    Attributes:
        exp: Exponent key type.
        qt: QT key type.
    """

    exp = auto()
    qt = auto()


class BurstSizeRange:
    """Class for burst size parameter.

    Attributes:
        burst_size: The burst size range, this list must be less than 32 elements.
    """

    burst_size: int | list[int]

    def __init__(self, min: int, inc: int, max: int) -> None:
        """Initialize the burst size range.

        Args:
            min: minimum value in range (inclusive)
            inc: space in between each element in the range
            max: maximum value in range (inclusive)

        Raises:
            ValueError: If the burst size range is more than 32 elements.
        """
        if (max - min) % inc > 32:
            raise ValueError("Burst size range must be less than 32 elements.")
        self.burst_size = list(range(min, max + 1, inc))


class ListWrapper:
    """Class for wrapping a list of integers.

    One of the arguments for the cryptodev application is a list of integers. However, when
    passing a list directly, it causes a syntax error in the command line. This class wraps
    a list of integers and converts it to a comma-separated string for a proper command.
    """

    def __init__(self, values: list[int]) -> None:
        """Initialize the list wrapper.

        Args:
            values: list of values to wrap
        """
        self.values = values

    def __str__(self) -> str:
        """Convert the list to a comma-separated string."""
        return ",".join(str(v) for v in self.values)


@dataclass(slots=True, kw_only=True)
class CryptoPmdParams(EalParams):
    """Parameters for cryptodev-pmd application.

    Attributes:
        aead_aad_sz: set the size of AEAD AAD.
        aead_algo: set AEAD algorithm name from class `AeadAlgName`.
        aead_iv_sz: set the size of AEAD iv.
        aead_key_sz: set the size of AEAD key.
        aead_op: set AEAD operation mode from class `EncryptDecryptSwitch`.
        asym_op: set asymmetric operation mode from class `AsymOpMode`.
        auth_algo: set authentication algorithm name.
        auth_aad_sz: set the size of authentication AAD.
        auth_iv_sz: set the size of authentication iv.
        auth_key_sz: set the size of authentication key.
        auth_op: set authentication operation mode from class `AuthenticationOpMode`.
        buffer_sz: Set the size of a single packet (plaintext or ciphertext in it).
            burst_sz: Set the number of packets per burst. This can be set as a single value or
            range of values defined by class `BurstSizeRange`. Default is 16.
        burst_sz: Set the number of packets per burst. This can be set as a single value or
            range of values defined by class `BurstSizeRange`. Default is 16.
        cipher_algo: Set cipher algorithm name from class `CipherAlgorithm`.
        cipher_iv_sz: set the size of cipher iv.
        cipher_key_sz: set the size of cipher key.
        cipher_op: set cipher operation mode from class `EncryptDecryptSwitch`.
        csv_friendly: Enable test result output CSV friendly rather than human friendly.
        desc_nb: set the number of descriptors for each crypto device.
        devtype: Set the device name from class `DeviceType`.
        digest_sz: set the size of digest.
        docsis_hdr_sz: set DOCSIS header size(n) in bytes.
        enable_sdap: enable service data adaptation protocol.
        imix: Set the distribution of packet sizes. A list of weights must be passed, containing the
            same number of items than buffer-sz, so each item in this list will be the weight of the
            packet size on the same position in the buffer-sz parameter (a list has to be passed in
            that parameter).
        low_prio_qp_mask: set low priority queue pairs set in the hexadecimal mask. This is an
            optional parameter, if not set all queue pairs will be on the same high priority.
        modex_len: set modex length for asymmetric crypto perf test. Supported lengths are 60,
            128, 255, 448.  Default length is 128.
        optype: Set operation type from class `OpType`.
        out_of_place: Enable out-of-place crypto operations mode.
        pdcp_sn_sz:  set PDCP sequebce number size(n) in bits. Valid values of n are 5/7/12/15/18.
        pdcp_domain: Set PDCP domain to specify short_mac/control/user plane from class
            `PDCPDomain`.
        pdcp_ses_hfn_en: enable fixed session based HFN instead of per packet HFN.
        pmd_cyclecount_delay_pmd: Add a delay (in milliseconds) between enqueue and dequeue in
            pmd-cyclecount benchmarking mode (useful when benchmarking hardware acceleration).
        pool_sz: Set the number if mbufs to be allocated in the mbuf pool.
        ptest: Set performance throughput test type from class `TestType`.
        rsa_modlen: Set RSA modulus length (in bits) for asymmetric crypto perf test.
            To be used with RSA asymmetric crypto ops.Supported lengths are 1024, 2048, 4096, 8192.
            Default length is 1024.
        rsa_priv_keytype: set RSA private key type from class `RSAPrivKeytype`. To be used with RSA
            asymmetric crypto ops.
        segment_sz: Set the size of the segment to use, for Scatter Gather List testing. Use list of
            values in buffer-sz in descending order if segment-sz is used. By default, it is set to
            the size of the maximum buffer size, including the digest size, so a single segment is
            created.
        sessionless: Enable session-less crypto operations mode.
        shared_session: Enable sharing sessions between all queue pairs on a single crypto PMD. This
            can be useful for benchmarking this setup, or finding and debugging concurrency errors
            that can occur while using sessions on multiple lcores simultaneously.
        silent: Disable options dump.
        test_file: Set test vector file path. See the Test Vector File chapter.
        test_name: Set specific test name section in the test vector file.
        tls_version: Set TLS/DTLS protocol version for perf test from class `TLSVersion`.
            Default is TLS1.2.
        total_ops: Set the number of total operations performed.
    """

    aead_aad_sz: int | None = field(default=None, metadata=Params.long("aead-aad-sz"))
    aead_algo: AeadAlgName | None = field(default=None, metadata=Params.long("aead-algo"))
    aead_iv_sz: int | None = field(default=None, metadata=Params.long("aead-iv-sz"))
    aead_key_sz: int | None = field(default=None, metadata=Params.long("aead-key-sz"))
    aead_op: EncryptDecryptSwitch | None = field(default=None, metadata=Params.long("aead-op"))
    asym_op: AsymOpMode | None = field(default=None, metadata=Params.long("asym-op"))
    auth_algo: AuthenticationAlgorithm | None = field(
        default=None, metadata=Params.long("auth-algo")
    )
    auth_iv_sz: int | None = field(default=None, metadata=Params.long("auth-iv-sz"))
    auth_key_sz: int | None = field(default=None, metadata=Params.long("auth-key-sz"))
    auth_op: AuthenticationOpMode | None = field(default=None, metadata=Params.long("auth-op"))
    buffer_sz: BurstSizeRange | ListWrapper | int | None = field(
        default=None, metadata=Params.long("buffer-sz")
    )
    burst_sz: BurstSizeRange | ListWrapper | int | None = field(
        default=None, metadata=Params.long("burst-sz")
    )
    cipher_algo: CipherAlgorithm | None = field(default=None, metadata=Params.long("cipher-algo"))
    cipher_iv_sz: int | None = field(default=None, metadata=Params.long("cipher-iv-sz"))
    cipher_key_sz: int | None = field(default=None, metadata=Params.long("cipher-key-sz"))
    cipher_op: EncryptDecryptSwitch | None = field(default=None, metadata=Params.long("cipher-op"))
    csv_friendly: Switch = field(default=None, metadata=Params.long("csv-friendly"))
    desc_nb: int | None = field(default=None, metadata=Params.long("desc-nb"))
    devtype: DeviceType = field(metadata=Params.long("devtype"))
    digest_sz: int | None = field(default=None, metadata=Params.long("digest-sz"))
    docsis_hdr_sz: int | None = field(default=None, metadata=Params.long("docsis-hdr-sz"))
    enable_sdap: Switch = None
    imix: int | None = field(default=None, metadata=Params.long("imix"))
    low_prio_qp_mask: int | None = field(default=None, metadata=Params.convert_value(hex))
    modex_len: int | None = field(default=None, metadata=Params.long("modex-len"))
    optype: OperationType | None = field(default=None, metadata=Params.long("optype"))
    out_of_place: Switch = None
    pdcp_sn_sz: int | None = None
    pdcp_domain: PDCPDomain | None = field(default=None, metadata=Params.long("pdcp-domain"))
    pdcp_ses_hfn_en: Switch | None = field(default=None, metadata=Params.long("pdcp-ses-hfn-en"))
    pmd_cyclecount_delay_pmd: int | None = field(
        default=None, metadata=Params.long("pmd-cyclecount-delay-pmd")
    )
    pool_sz: int | None = field(default=None, metadata=Params.long("pool-sz"))
    ptest: TestType = field(default=TestType.throughput, metadata=Params.long("ptest"))
    rsa_modlen: int | None = field(default=None, metadata=Params.long("rsa-modlen"))
    rsa_priv_keytype: RSAPrivKeytype | None = field(
        default=None, metadata=Params.long("rsa-priv-keytype")
    )
    segment_sz: int | None = field(default=None, metadata=Params.long("segment-sz"))
    sessionless: Switch = None
    shared_session: Switch = None
    silent: Silent | None = field(default="", metadata=Params.long("silent"))
    test_file: str | None = field(default=None, metadata=Params.long("test-file"))
    test_name: str | None = field(default=None, metadata=Params.long("test-name"))
    tls_version: TLSVersion | None = field(default=None, metadata=Params.long("tls-version"))
    total_ops: int | None = field(default=100000, metadata=Params.long("total-ops"))
