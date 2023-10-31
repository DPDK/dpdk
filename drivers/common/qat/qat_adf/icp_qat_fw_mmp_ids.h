/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

/**
 * @file icp_qat_fw_mmp_ids.h
 * @ingroup icp_qat_fw_mmp
 * @brief
 *      This file documents the external interfaces that the QAT FW running
 *      on the QAT Acceleration Engine provides to clients wanting to
 *      accelerate crypto asymmetric applications
 */

#ifndef _ICP_QAT_FW_MMP_IDS_
#define _ICP_QAT_FW_MMP_IDS_

#define PKE_INIT 0x09061a09
/**< Functionality ID for Initialisation sequence
 * @li 1 input parameters : @link icp_qat_fw_mmp_init_input::z z @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_init_output::zz zz @endlink
 */
#define PKE_DH_G2_768 0x1c0b1a10
/**< Functionality ID for Diffie-Hellman Modular exponentiation base 2 for
 *768-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_dh_g2_768_input::e e
 * @endlink @link icp_qat_fw_mmp_dh_g2_768_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_g2_768_output::r r
 * @endlink
 */
#define PKE_DH_768 0x210c1a1b
/**< Functionality ID for Diffie-Hellman Modular exponentiation for 768-bit
 *numbers
 * @li 3 input parameters : @link icp_qat_fw_mmp_dh_768_input::g g @endlink
 * @link icp_qat_fw_mmp_dh_768_input::e e @endlink @link
 * icp_qat_fw_mmp_dh_768_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_768_output::r r @endlink
 */
#define PKE_DH_G2_1024 0x220b1a27
/**< Functionality ID for Diffie-Hellman Modular exponentiation base 2 for
 * 1024-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_dh_g2_1024_input::e e
 * @endlink @link icp_qat_fw_mmp_dh_g2_1024_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_g2_1024_output::r r
 * @endlink
 */
#define PKE_DH_1024 0x290c1a32
/**< Functionality ID for Diffie-Hellman Modular exponentiation for 1024-bit
 * numbers
 * @li 3 input parameters : @link icp_qat_fw_mmp_dh_1024_input::g g @endlink
 * @link icp_qat_fw_mmp_dh_1024_input::e e @endlink @link
 * icp_qat_fw_mmp_dh_1024_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_1024_output::r r @endlink
 */
#define PKE_DH_G2_1536 0x2e0b1a3e
/**< Functionality ID for Diffie-Hellman Modular exponentiation base 2 for
 * 1536-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_dh_g2_1536_input::e e
 * @endlink @link icp_qat_fw_mmp_dh_g2_1536_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_g2_1536_output::r r
 * @endlink
 */
#define PKE_DH_1536 0x390c1a49
/**< Functionality ID for Diffie-Hellman Modular exponentiation for 1536-bit
 * numbers
 * @li 3 input parameters : @link icp_qat_fw_mmp_dh_1536_input::g g @endlink
 * @link icp_qat_fw_mmp_dh_1536_input::e e @endlink @link
 * icp_qat_fw_mmp_dh_1536_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_1536_output::r r @endlink
 */
#define PKE_DH_G2_2048 0x3e0b1a55
/**< Functionality ID for Diffie-Hellman Modular exponentiation base 2 for
 * 2048-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_dh_g2_2048_input::e e
 * @endlink @link icp_qat_fw_mmp_dh_g2_2048_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_g2_2048_output::r r
 * @endlink
 */
#define PKE_DH_2048 0x4d0c1a60
/**< Functionality ID for Diffie-Hellman Modular exponentiation for 2048-bit
 * numbers
 * @li 3 input parameters : @link icp_qat_fw_mmp_dh_2048_input::g g @endlink
 * @link icp_qat_fw_mmp_dh_2048_input::e e @endlink @link
 * icp_qat_fw_mmp_dh_2048_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_2048_output::r r @endlink
 */
#define PKE_DH_G2_3072 0x3a0b1a6c
/**< Functionality ID for Diffie-Hellman Modular exponentiation base 2 for
 * 3072-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_dh_g2_3072_input::e e
 * @endlink @link icp_qat_fw_mmp_dh_g2_3072_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_g2_3072_output::r r
 * @endlink
 */
#define PKE_DH_3072 0x510c1a77
/**< Functionality ID for Diffie-Hellman Modular exponentiation for 3072-bit
 * numbers
 * @li 3 input parameters : @link icp_qat_fw_mmp_dh_3072_input::g g @endlink
 * @link icp_qat_fw_mmp_dh_3072_input::e e @endlink @link
 * icp_qat_fw_mmp_dh_3072_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_3072_output::r r @endlink
 */
#define PKE_DH_G2_4096 0x4a0b1a83
/**< Functionality ID for Diffie-Hellman Modular exponentiation base 2 for
 * 4096-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_dh_g2_4096_input::e e
 * @endlink @link icp_qat_fw_mmp_dh_g2_4096_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_g2_4096_output::r r
 * @endlink
 */
#define PKE_DH_4096 0x690c1a8e
/**< Functionality ID for Diffie-Hellman Modular exponentiation for 4096-bit
 * numbers
 * @li 3 input parameters : @link icp_qat_fw_mmp_dh_4096_input::g g @endlink
 * @link icp_qat_fw_mmp_dh_4096_input::e e @endlink @link
 * icp_qat_fw_mmp_dh_4096_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dh_4096_output::r r @endlink
 */
#define PKE_RSA_KP1_512 0x191d1a9a
/**< Functionality ID for RSA 512 key generation first form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp1_512_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_512_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp1_512_input::e e @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_rsa_kp1_512_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_512_output::d d @endlink
 */
#define PKE_RSA_KP2_512 0x19401acc
/**< Functionality ID for RSA 512 key generation second form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp2_512_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_512_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_512_input::e e @endlink
 * @li 5 output parameters : @link icp_qat_fw_mmp_rsa_kp2_512_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_512_output::d d @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_512_output::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_512_output::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_512_output::qinv qinv @endlink
 */
#define PKE_RSA_EP_512 0x1c161b21
/**< Functionality ID for RSA 512 Encryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_ep_512_input::m m
 * @endlink @link icp_qat_fw_mmp_rsa_ep_512_input::e e @endlink @link
 * icp_qat_fw_mmp_rsa_ep_512_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_ep_512_output::c c
 * @endlink
 */
#define PKE_RSA_DP1_512 0x1c161b3c
/**< Functionality ID for RSA 512 Decryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_dp1_512_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp1_512_input::d d @endlink @link
 * icp_qat_fw_mmp_rsa_dp1_512_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp1_512_output::m m
 * @endlink
 */
#define PKE_RSA_DP2_512 0x1c131b57
/**< Functionality ID for RSA 1024 Decryption with CRT
 * @li 6 input parameters : @link icp_qat_fw_mmp_rsa_dp2_512_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp2_512_input::p p @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_512_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_512_input::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_512_input::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_512_input::qinv qinv @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp2_512_output::m m
 * @endlink
 */
#define PKE_RSA_KP1_1024 0x36181b71
/**< Functionality ID for RSA 1024 key generation first form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp1_1024_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_1024_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp1_1024_input::e e @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_rsa_kp1_1024_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_1024_output::d d @endlink
 */
#define PKE_RSA_KP2_1024 0x40451b9e
/**< Functionality ID for RSA 1024 key generation second form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp2_1024_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_1024_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1024_input::e e @endlink
 * @li 5 output parameters : @link icp_qat_fw_mmp_rsa_kp2_1024_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_1024_output::d d @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1024_output::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1024_output::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1024_output::qinv qinv @endlink
 */
#define PKE_RSA_EP_1024 0x35111bf7
/**< Functionality ID for RSA 1024 Encryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_ep_1024_input::m m
 * @endlink @link icp_qat_fw_mmp_rsa_ep_1024_input::e e @endlink @link
 * icp_qat_fw_mmp_rsa_ep_1024_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_ep_1024_output::c c
 * @endlink
 */
#define PKE_RSA_DP1_1024 0x35111c12
/**< Functionality ID for RSA 1024 Decryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_dp1_1024_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp1_1024_input::d d @endlink @link
 * icp_qat_fw_mmp_rsa_dp1_1024_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp1_1024_output::m m
 * @endlink
 */
#define PKE_RSA_DP2_1024 0x26131c2d
/**< Functionality ID for RSA 1024 Decryption with CRT
 * @li 6 input parameters : @link icp_qat_fw_mmp_rsa_dp2_1024_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp2_1024_input::p p @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1024_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1024_input::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1024_input::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1024_input::qinv qinv @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp2_1024_output::m m
 * @endlink
 */
#define PKE_RSA_KP1_1536 0x531d1c46
/**< Functionality ID for RSA 1536 key generation first form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp1_1536_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_1536_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp1_1536_input::e e @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_rsa_kp1_1536_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_1536_output::d d @endlink
 */
#define PKE_RSA_KP2_1536 0x32391c78
/**< Functionality ID for RSA 1536 key generation second form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp2_1536_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_1536_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1536_input::e e @endlink
 * @li 5 output parameters : @link icp_qat_fw_mmp_rsa_kp2_1536_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_1536_output::d d @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1536_output::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1536_output::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_1536_output::qinv qinv @endlink
 */
#define PKE_RSA_EP_1536 0x4d111cdc
/**< Functionality ID for RSA 1536 Encryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_ep_1536_input::m m
 * @endlink @link icp_qat_fw_mmp_rsa_ep_1536_input::e e @endlink @link
 * icp_qat_fw_mmp_rsa_ep_1536_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_ep_1536_output::c c
 * @endlink
 */
#define PKE_RSA_DP1_1536 0x4d111cf7
/**< Functionality ID for RSA 1536 Decryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_dp1_1536_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp1_1536_input::d d @endlink @link
 * icp_qat_fw_mmp_rsa_dp1_1536_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp1_1536_output::m m
 *@endlink
 */
#define PKE_RSA_DP2_1536 0x45111d12
/**< Functionality ID for RSA 1536 Decryption with CRT
 * @li 6 input parameters : @link icp_qat_fw_mmp_rsa_dp2_1536_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp2_1536_input::p p @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1536_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1536_input::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1536_input::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_1536_input::qinv qinv @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp2_1536_output::m m
 * @endlink
 */
#define PKE_RSA_KP1_2048 0x72181d2e
/**< Functionality ID for RSA 2048 key generation first form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp1_2048_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_2048_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp1_2048_input::e e @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_rsa_kp1_2048_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_2048_output::d d @endlink
 */
#define PKE_RSA_KP2_2048 0x42341d5b
/**< Functionality ID for RSA 2048 key generation second form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp2_2048_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_2048_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_2048_input::e e @endlink
 * @li 5 output parameters : @link icp_qat_fw_mmp_rsa_kp2_2048_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_2048_output::d d @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_2048_output::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_2048_output::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_2048_output::qinv qinv @endlink
 */
#define PKE_RSA_EP_2048 0x6e111dba
/**< Functionality ID for RSA 2048 Encryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_ep_2048_input::m m
 * @endlink @link icp_qat_fw_mmp_rsa_ep_2048_input::e e @endlink @link
 * icp_qat_fw_mmp_rsa_ep_2048_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_ep_2048_output::c c
 * @endlink
 */
#define PKE_RSA_DP1_2048 0x6e111dda
/**< Functionality ID for RSA 2048 Decryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_dp1_2048_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp1_2048_input::d d @endlink @link
 * icp_qat_fw_mmp_rsa_dp1_2048_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp1_2048_output::m m
 * @endlink
 */
#define PKE_RSA_DP2_2048 0x59121dfa
/**< Functionality ID for RSA 2048 Decryption with CRT
 * @li 6 input parameters : @link icp_qat_fw_mmp_rsa_dp2_2048_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp2_2048_input::p p @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_2048_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_2048_input::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_2048_input::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_2048_input::qinv qinv @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp2_2048_output::m m
 * @endlink
 */
#define PKE_RSA_KP1_3072 0x60191e16
/**< Functionality ID for RSA 3072 key generation first form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp1_3072_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_3072_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp1_3072_input::e e @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_rsa_kp1_3072_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_3072_output::d d @endlink
 */
#define PKE_RSA_KP2_3072 0x68331e45
/**< Functionality ID for RSA 3072 key generation second form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp2_3072_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_3072_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_3072_input::e e @endlink
 * @li 5 output parameters : @link icp_qat_fw_mmp_rsa_kp2_3072_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_3072_output::d d @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_3072_output::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_3072_output::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_3072_output::qinv qinv @endlink
 */
#define PKE_RSA_EP_3072 0x7d111ea3
/**< Functionality ID for RSA 3072 Encryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_ep_3072_input::m m
 * @endlink @link icp_qat_fw_mmp_rsa_ep_3072_input::e e @endlink @link
 * icp_qat_fw_mmp_rsa_ep_3072_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_ep_3072_output::c c
 * @endlink
 */
#define PKE_RSA_DP1_3072 0x7d111ebe
/**< Functionality ID for RSA 3072 Decryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_dp1_3072_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp1_3072_input::d d @endlink @link
 * icp_qat_fw_mmp_rsa_dp1_3072_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp1_3072_output::m m
 * @endlink
 */
#define PKE_RSA_DP2_3072 0x81121ed9
/**< Functionality ID for RSA 3072 Decryption with CRT
 * @li 6 input parameters : @link icp_qat_fw_mmp_rsa_dp2_3072_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp2_3072_input::p p @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_3072_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_3072_input::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_3072_input::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_3072_input::qinv qinv @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp2_3072_output::m m
 * @endlink
 */
#define PKE_RSA_KP1_4096 0x7d1f1ef6
/**< Functionality ID for RSA 4096 key generation first form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp1_4096_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_4096_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp1_4096_input::e e @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_rsa_kp1_4096_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp1_4096_output::d d @endlink
 */
#define PKE_RSA_KP2_4096 0x91251f27
/**< Functionality ID for RSA 4096 key generation second form
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_kp2_4096_input::p p
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_4096_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_4096_input::e e @endlink
 * @li 5 output parameters : @link icp_qat_fw_mmp_rsa_kp2_4096_output::n n
 * @endlink @link icp_qat_fw_mmp_rsa_kp2_4096_output::d d @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_4096_output::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_4096_output::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_kp2_4096_output::qinv qinv @endlink
 */
#define PKE_RSA_EP_4096 0xa5101f7e
/**< Functionality ID for RSA 4096 Encryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_ep_4096_input::m m
 * @endlink @link icp_qat_fw_mmp_rsa_ep_4096_input::e e @endlink @link
 * icp_qat_fw_mmp_rsa_ep_4096_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_ep_4096_output::c c
 * @endlink
 */
#define PKE_RSA_DP1_4096 0xa5101f98
/**< Functionality ID for RSA 4096 Decryption
 * @li 3 input parameters : @link icp_qat_fw_mmp_rsa_dp1_4096_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp1_4096_input::d d @endlink @link
 * icp_qat_fw_mmp_rsa_dp1_4096_input::n n @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp1_4096_output::m m
 * @endlink
 */
#define PKE_RSA_DP2_4096 0xb1111fb2
/**< Functionality ID for RSA 4096 Decryption with CRT
 * @li 6 input parameters : @link icp_qat_fw_mmp_rsa_dp2_4096_input::c c
 * @endlink @link icp_qat_fw_mmp_rsa_dp2_4096_input::p p @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_4096_input::q q @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_4096_input::dp dp @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_4096_input::dq dq @endlink @link
 * icp_qat_fw_mmp_rsa_dp2_4096_input::qinv qinv @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_rsa_dp2_4096_output::m m
 * @endlink
 */
#define PKE_GCD_PT_192 0x19201fcd
/**< Functionality ID for GCD primality test for 192-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_192_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_256 0x19201ff7
/**< Functionality ID for GCD primality test for 256-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_256_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_384 0x19202021
/**< Functionality ID for GCD primality test for 384-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_384_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_512 0x1b1b204b
/**< Functionality ID for GCD primality test for 512-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_512_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_768 0x170c2070
/**< Functionality ID for GCD primality test for 768-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_768_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_1024 0x130f2085
/**< Functionality ID for GCD primality test for 1024-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_1024_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_1536 0x1d0c2094
/**< Functionality ID for GCD primality test for 1536-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_1536_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_2048 0x210c20a5
/**< Functionality ID for GCD primality test for 2048-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_2048_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_3072 0x290c20b6
/**< Functionality ID for GCD primality test for 3072-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_3072_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_GCD_PT_4096 0x310c20c7
/**< Functionality ID for GCD primality test for 4096-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_gcd_pt_4096_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_160 0x0e1120d8
/**< Functionality ID for Fermat primality test for 160-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_160_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_512 0x121120ee
/**< Functionality ID for Fermat primality test for 512-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_512_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_L512 0x19162104
/**< Functionality ID for Fermat primality test for &lte; 512-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_l512_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_768 0x19112124
/**< Functionality ID for Fermat primality test for 768-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_768_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_1024 0x1f11213a
/**< Functionality ID for Fermat primality test for 1024-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_1024_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_1536 0x2b112150
/**< Functionality ID for Fermat primality test for 1536-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_1536_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_2048 0x3b112166
/**< Functionality ID for Fermat primality test for 2048-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_2048_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_3072 0x3a11217c
/**< Functionality ID for Fermat primality test for 3072-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_3072_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_FERMAT_PT_4096 0x4a112192
/**< Functionality ID for Fermat primality test for 4096-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_fermat_pt_4096_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_160 0x0e1221a8
/**< Functionality ID for Miller-Rabin primality test for 160-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_160_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_160_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_512 0x111221bf
/**< Functionality ID for Miller-Rabin primality test for 512-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_512_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_512_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_768 0x1d0d21d6
/**< Functionality ID for Miller-Rabin primality test for 768-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_768_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_768_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_1024 0x250d21ed
/**< Functionality ID for Miller-Rabin primality test for 1024-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_1024_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_1024_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_1536 0x350d2204
/**< Functionality ID for Miller-Rabin primality test for 1536-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_1536_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_1536_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_2048 0x490d221b
/**< Functionality ID for Miller-Rabin primality test for 2048-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_2048_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_2048_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_3072 0x4d0d2232
/**< Functionality ID for Miller-Rabin primality test for 3072-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_3072_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_3072_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_4096 0x650d2249
/**< Functionality ID for Miller-Rabin primality test for 4096-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_4096_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_4096_input::m m @endlink
 * @li no output parameters
 */
#define PKE_MR_PT_L512 0x18182260
/**< Functionality ID for Miller-Rabin primality test for 512-bit numbers
 * @li 2 input parameters : @link icp_qat_fw_mmp_mr_pt_l512_input::x x
 * @endlink @link icp_qat_fw_mmp_mr_pt_l512_input::m m @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_160 0x0e0c227e
/**< Functionality ID for Lucas primality test for 160-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_160_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_512 0x110c228f
/**< Functionality ID for Lucas primality test for 512-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_512_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_768 0x130c22a0
/**< Functionality ID for Lucas primality test for 768-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_768_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_1024 0x150c22b1
/**< Functionality ID for Lucas primality test for 1024-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_1024_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_1536 0x190c22c2
/**< Functionality ID for Lucas primality test for 1536-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_1536_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_2048 0x1d0c22d3
/**< Functionality ID for Lucas primality test for 2048-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_2048_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_3072 0x250c22e4
/**< Functionality ID for Lucas primality test for 3072-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_3072_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_4096 0x661522f5
/**< Functionality ID for Lucas primality test for 4096-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_4096_input::m m
 * @endlink
 * @li no output parameters
 */
#define PKE_LUCAS_PT_L512 0x1617230a
/**< Functionality ID for Lucas primality test for L512-bit numbers
 * @li 1 input parameters : @link icp_qat_fw_mmp_lucas_pt_l512_input::m m
 * @endlink
 * @li no output parameters
 */
#define MATHS_MODEXP_L512 0x150c2327
/**< Functionality ID for Modular exponentiation for numbers less than 512-bits
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l512_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l512_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l512_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l512_output::r r
@endlink
 */
#define MATHS_MODEXP_L1024 0x2d0c233e
/**< Functionality ID for Modular exponentiation for numbers less than 1024-bit
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l1024_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l1024_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l1024_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l1024_output::r r
 * @endlink
 */
#define MATHS_MODEXP_L1536 0x410c2355
/**< Functionality ID for Modular exponentiation for numbers less than 1536-bits
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l1536_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l1536_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l1536_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l1536_output::r r
 * @endlink
 */
#define MATHS_MODEXP_L2048 0x5e12236c
/**< Functionality ID for Modular exponentiation for numbers less than 2048-bit
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l2048_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l2048_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l2048_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l2048_output::r r
 * @endlink
 */
#define MATHS_MODEXP_L2560 0x60162388
/**< Functionality ID for Modular exponentiation for numbers less than 2560-bits
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l2560_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l2560_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l2560_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l2560_output::r r
 * @endlink
 */
#define MATHS_MODEXP_L3072 0x650c23a9
/**< Functionality ID for Modular exponentiation for numbers less than 3072-bits
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l3072_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l3072_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l3072_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l3072_output::r r
 * @endlink
 */
#define MATHS_MODEXP_L3584 0x801623c0
/**< Functionality ID for Modular exponentiation for numbers less than 3584-bits
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l3584_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l3584_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l3584_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l3584_output::r r
 * @endlink
 */
#define MATHS_MODEXP_L4096 0x850c23e1
/**< Functionality ID for Modular exponentiation for numbers less than 4096-bit
 * @li 3 input parameters : @link icp_qat_fw_maths_modexp_l4096_input::g g
 * @endlink @link icp_qat_fw_maths_modexp_l4096_input::e e @endlink @link
 * icp_qat_fw_maths_modexp_l4096_input::m m @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modexp_l4096_output::r r
 * @endlink
 */
#define MATHS_MODINV_ODD_L128 0x090623f8
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 128 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l128_input::a a
 * @endlink @link icp_qat_fw_maths_modinv_odd_l128_input::b b @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modinv_odd_l128_output::c
 * c @endlink
 */
#define MATHS_MODINV_ODD_L192 0x0a0623fe
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 192 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l192_input::a a
 * @endlink @link icp_qat_fw_maths_modinv_odd_l192_input::b b @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modinv_odd_l192_output::c
 * c @endlink
 */
#define MATHS_MODINV_ODD_L256 0x0a062404
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 256 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l256_input::a a
 * @endlink @link icp_qat_fw_maths_modinv_odd_l256_input::b b @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modinv_odd_l256_output::c
 * c @endlink
 */
#define MATHS_MODINV_ODD_L384 0x0b06240a
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 384 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l384_input::a a
 * @endlink @link icp_qat_fw_maths_modinv_odd_l384_input::b b @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modinv_odd_l384_output::c
 * c @endlink
 */
#define MATHS_MODINV_ODD_L512 0x0c062410
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 512 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l512_input::a a
 * @endlink @link icp_qat_fw_maths_modinv_odd_l512_input::b b @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modinv_odd_l512_output::c
 * c @endlink
 */
#define MATHS_MODINV_ODD_L768 0x0e062416
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 768 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l768_input::a a
 * @endlink @link icp_qat_fw_maths_modinv_odd_l768_input::b b @endlink
 * @li 1 output parameters : @link icp_qat_fw_maths_modinv_odd_l768_output::c
 * c @endlink
 */
#define MATHS_MODINV_ODD_L1024 0x1006241c
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 1024 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l1024_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_odd_l1024_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_odd_l1024_output::c c @endlink
 */
#define MATHS_MODINV_ODD_L1536 0x18062422
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 1536 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l1536_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_odd_l1536_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_odd_l1536_output::c c @endlink
 */
#define MATHS_MODINV_ODD_L2048 0x20062428
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 2048 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l2048_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_odd_l2048_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_odd_l2048_output::c c @endlink
 */
#define MATHS_MODINV_ODD_L3072 0x3006242e
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 3072 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l3072_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_odd_l3072_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_odd_l3072_output::c c @endlink
 */
#define MATHS_MODINV_ODD_L4096 0x40062434
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 4096 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_odd_l4096_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_odd_l4096_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_odd_l4096_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L128 0x0906243a
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 128 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l128_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l128_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l128_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L192 0x0a062440
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 192 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l192_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l192_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l192_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L256 0x0a062446
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 256 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l256_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l256_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l256_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L384 0x0e0b244c
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 384 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l384_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l384_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l384_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L512 0x110b2457
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 512 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l512_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l512_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l512_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L768 0x170b2462
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 768 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l768_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l768_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l768_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L1024 0x1d0b246d
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 1024 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l1024_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l1024_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l1024_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L1536 0x290b2478
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 1536 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l1536_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l1536_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l1536_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L2048 0x350b2483
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 2048 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l2048_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l2048_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l2048_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L3072 0x4d0b248e
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 3072 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l3072_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l3072_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l3072_output::c c @endlink
 */
#define MATHS_MODINV_EVEN_L4096 0x650b2499
/**< Functionality ID for Modular multiplicative inverse for numbers less than
 * 4096 bits
 * @li 2 input parameters : @link icp_qat_fw_maths_modinv_even_l4096_input::a
 * a @endlink @link icp_qat_fw_maths_modinv_even_l4096_input::b b @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_maths_modinv_even_l4096_output::c c @endlink
 */
#define PKE_DSA_GEN_P_1024_160 0x381824a4
/**< Functionality ID for DSA parameter generation P
 * @li 2 input parameters : @link icp_qat_fw_mmp_dsa_gen_p_1024_160_input::x
 * x @endlink @link icp_qat_fw_mmp_dsa_gen_p_1024_160_input::q q @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_gen_p_1024_160_output::p p @endlink
 */
#define PKE_DSA_GEN_G_1024 0x261424d4
/**< Functionality ID for DSA key generation G
 * @li 3 input parameters : @link icp_qat_fw_mmp_dsa_gen_g_1024_input::p p
 * @endlink @link icp_qat_fw_mmp_dsa_gen_g_1024_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_gen_g_1024_input::h h @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_gen_g_1024_output::g g
 * @endlink
 */
#define PKE_DSA_GEN_Y_1024 0x291224ed
/**< Functionality ID for DSA key generation Y
 * @li 3 input parameters : @link icp_qat_fw_mmp_dsa_gen_y_1024_input::p p
 * @endlink @link icp_qat_fw_mmp_dsa_gen_y_1024_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_gen_y_1024_input::x x @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_gen_y_1024_output::y y
 * @endlink
 */
#define PKE_DSA_SIGN_R_1024_160 0x2c1c2504
/**< Functionality ID for DSA Sign R
 * @li 4 input parameters : @link icp_qat_fw_mmp_dsa_sign_r_1024_160_input::k
 * k @endlink @link icp_qat_fw_mmp_dsa_sign_r_1024_160_input::p p @endlink
 * @link icp_qat_fw_mmp_dsa_sign_r_1024_160_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_1024_160_input::g g @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_1024_160_output::r r @endlink
 */
#define PKE_DSA_SIGN_S_160 0x12142526
/**< Functionality ID for DSA Sign S
 * @li 5 input parameters : @link icp_qat_fw_mmp_dsa_sign_s_160_input::m m
 * @endlink @link icp_qat_fw_mmp_dsa_sign_s_160_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_160_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_160_input::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_160_input::x x @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_sign_s_160_output::s s
 * @endlink
 */
#define PKE_DSA_SIGN_R_S_1024_160 0x301e2540
/**< Functionality ID for DSA Sign R S
 * @li 6 input parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_input::x x @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_output::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_1024_160_output::s s @endlink
 */
#define PKE_DSA_VERIFY_1024_160 0x323a2570
/**< Functionality ID for DSA Verify
 * @li 7 input parameters : @link icp_qat_fw_mmp_dsa_verify_1024_160_input::r
 * r @endlink @link icp_qat_fw_mmp_dsa_verify_1024_160_input::s s @endlink
 * @link icp_qat_fw_mmp_dsa_verify_1024_160_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_verify_1024_160_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_verify_1024_160_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_verify_1024_160_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_verify_1024_160_input::y y @endlink
 * @li no output parameters
 */
#define PKE_DSA_GEN_P_2048_224 0x341d25be
/**< Functionality ID for DSA parameter generation P
 * @li 2 input parameters : @link icp_qat_fw_mmp_dsa_gen_p_2048_224_input::x
 * x @endlink @link icp_qat_fw_mmp_dsa_gen_p_2048_224_input::q q @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_gen_p_2048_224_output::p p @endlink
 */
#define PKE_DSA_GEN_Y_2048 0x4d1225ea
/**< Functionality ID for DSA key generation Y
 * @li 3 input parameters : @link icp_qat_fw_mmp_dsa_gen_y_2048_input::p p
 * @endlink @link icp_qat_fw_mmp_dsa_gen_y_2048_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_gen_y_2048_input::x x @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_gen_y_2048_output::y y
 * @endlink
 */
#define PKE_DSA_SIGN_R_2048_224 0x511c2601
/**< Functionality ID for DSA Sign R
 * @li 4 input parameters : @link icp_qat_fw_mmp_dsa_sign_r_2048_224_input::k
 * k @endlink @link icp_qat_fw_mmp_dsa_sign_r_2048_224_input::p p @endlink
 * @link icp_qat_fw_mmp_dsa_sign_r_2048_224_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_2048_224_input::g g @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_2048_224_output::r r @endlink
 */
#define PKE_DSA_SIGN_S_224 0x15142623
/**< Functionality ID for DSA Sign S
 * @li 5 input parameters : @link icp_qat_fw_mmp_dsa_sign_s_224_input::m m
 * @endlink @link icp_qat_fw_mmp_dsa_sign_s_224_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_224_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_224_input::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_224_input::x x @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_sign_s_224_output::s s
 * @endlink
 */
#define PKE_DSA_SIGN_R_S_2048_224 0x571e263d
/**< Functionality ID for DSA Sign R S
 * @li 6 input parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_input::x x @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_output::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_224_output::s s @endlink
 */
#define PKE_DSA_VERIFY_2048_224 0x6930266d
/**< Functionality ID for DSA Verify
 * @li 7 input parameters : @link icp_qat_fw_mmp_dsa_verify_2048_224_input::r
 * r @endlink @link icp_qat_fw_mmp_dsa_verify_2048_224_input::s s @endlink
 * @link icp_qat_fw_mmp_dsa_verify_2048_224_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_224_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_224_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_224_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_224_input::y y @endlink
 * @li no output parameters
 */
#define PKE_DSA_GEN_P_2048_256 0x431126b7
/**< Functionality ID for DSA parameter generation P
 * @li 2 input parameters : @link icp_qat_fw_mmp_dsa_gen_p_2048_256_input::x
 * x @endlink @link icp_qat_fw_mmp_dsa_gen_p_2048_256_input::q q @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_gen_p_2048_256_output::p p @endlink
 */
#define PKE_DSA_GEN_G_2048 0x4b1426ed
/**< Functionality ID for DSA key generation G
 * @li 3 input parameters : @link icp_qat_fw_mmp_dsa_gen_g_2048_input::p p
 * @endlink @link icp_qat_fw_mmp_dsa_gen_g_2048_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_gen_g_2048_input::h h @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_gen_g_2048_output::g g
 * @endlink
 */
#define PKE_DSA_SIGN_R_2048_256 0x5b182706
/**< Functionality ID for DSA Sign R
 * @li 4 input parameters : @link icp_qat_fw_mmp_dsa_sign_r_2048_256_input::k
 * k @endlink @link icp_qat_fw_mmp_dsa_sign_r_2048_256_input::p p @endlink
 * @link icp_qat_fw_mmp_dsa_sign_r_2048_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_2048_256_input::g g @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_2048_256_output::r r @endlink
 */
#define PKE_DSA_SIGN_S_256 0x15142733
/**< Functionality ID for DSA Sign S
 * @li 5 input parameters : @link icp_qat_fw_mmp_dsa_sign_s_256_input::m m
 * @endlink @link icp_qat_fw_mmp_dsa_sign_s_256_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_256_input::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_s_256_input::x x @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_sign_s_256_output::s s
 * @endlink
 */
#define PKE_DSA_SIGN_R_S_2048_256 0x5a2a274d
/**< Functionality ID for DSA Sign R S
 * @li 6 input parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_input::x x @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_output::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_2048_256_output::s s @endlink
 */
#define PKE_DSA_VERIFY_2048_256 0x723a2789
/**< Functionality ID for DSA Verify
 * @li 7 input parameters : @link icp_qat_fw_mmp_dsa_verify_2048_256_input::r
 * r @endlink @link icp_qat_fw_mmp_dsa_verify_2048_256_input::s s @endlink
 * @link icp_qat_fw_mmp_dsa_verify_2048_256_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_256_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_256_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_verify_2048_256_input::y y @endlink
 * @li no output parameters
 */
#define PKE_DSA_GEN_P_3072_256 0x4b1127e0
/**< Functionality ID for DSA parameter generation P
 * @li 2 input parameters : @link icp_qat_fw_mmp_dsa_gen_p_3072_256_input::x
 * x @endlink @link icp_qat_fw_mmp_dsa_gen_p_3072_256_input::q q @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_gen_p_3072_256_output::p p @endlink
 */
#define PKE_DSA_GEN_G_3072 0x4f142816
/**< Functionality ID for DSA key generation G
 * @li 3 input parameters : @link icp_qat_fw_mmp_dsa_gen_g_3072_input::p p
 * @endlink @link icp_qat_fw_mmp_dsa_gen_g_3072_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_gen_g_3072_input::h h @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_gen_g_3072_output::g g
 * @endlink
 */
#define PKE_DSA_GEN_Y_3072 0x5112282f
/**< Functionality ID for DSA key generation Y
 * @li 3 input parameters : @link icp_qat_fw_mmp_dsa_gen_y_3072_input::p p
 * @endlink @link icp_qat_fw_mmp_dsa_gen_y_3072_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_gen_y_3072_input::x x @endlink
 * @li 1 output parameters : @link icp_qat_fw_mmp_dsa_gen_y_3072_output::y y
 * @endlink
 */
#define PKE_DSA_SIGN_R_3072_256 0x59282846
/**< Functionality ID for DSA Sign R
 * @li 4 input parameters : @link icp_qat_fw_mmp_dsa_sign_r_3072_256_input::k
 * k @endlink @link icp_qat_fw_mmp_dsa_sign_r_3072_256_input::p p @endlink
 * @link icp_qat_fw_mmp_dsa_sign_r_3072_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_3072_256_input::g g @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_3072_256_output::r r @endlink
 */
#define PKE_DSA_SIGN_R_S_3072_256 0x61292874
/**< Functionality ID for DSA Sign R S
 * @li 6 input parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input::k k @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_input::x x @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_output::r r @endlink @link
 * icp_qat_fw_mmp_dsa_sign_r_s_3072_256_output::s s @endlink
 */
#define PKE_DSA_VERIFY_3072_256 0x7f4328ae
/**< Functionality ID for DSA Verify
 * @li 7 input parameters : @link icp_qat_fw_mmp_dsa_verify_3072_256_input::r
 * r @endlink @link icp_qat_fw_mmp_dsa_verify_3072_256_input::s s @endlink
 * @link  icp_qat_fw_mmp_dsa_verify_3072_256_input::m m @endlink @link
 * icp_qat_fw_mmp_dsa_verify_3072_256_input::p p @endlink @link
 * icp_qat_fw_mmp_dsa_verify_3072_256_input::q q @endlink @link
 * icp_qat_fw_mmp_dsa_verify_3072_256_input::g g @endlink @link
 * icp_qat_fw_mmp_dsa_verify_3072_256_input::y y @endlink
 * @li no output parameters
 */
#define PKE_ECDSA_SIGN_RS_GF2_L256 0x46512907
/**< Functionality ID for ECDSA Sign RS for curves B/K-163 and B/K-233
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_input::in in @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_output::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l256_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_R_GF2_L256 0x323a298f
/**< Functionality ID for ECDSA Sign R for curves B/K-163 and B/K-233
 * @li 7 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::xg xg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::yg yg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::n n @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::q q @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::a a @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::b b @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_input::k k @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l256_output::r r @endlink
 */
#define PKE_ECDSA_SIGN_S_GF2_L256 0x2b2229e6
/**< Functionality ID for ECDSA Sign S for curves with n &lt; 2^256
 * @li 5 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input::e e @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input::d d @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input::k k @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_input::n n @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l256_output::s s @endlink
 */
#define PKE_ECDSA_VERIFY_GF2_L256 0x337e2a27
/**< Functionality ID for ECDSA Verify for curves B/K-163 and B/K-233
 * @li 1 input parameters : @link
 *icp_qat_fw_mmp_ecdsa_verify_gf2_l256_input::in in @endlink
 * @li no output parameters
 */
#define PKE_ECDSA_SIGN_RS_GF2_L512 0x5e5f2ad7
/**< Functionality ID for ECDSA Sign RS
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_input::in in @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_output::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_l512_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_R_GF2_L512 0x84312b6a
/**< Functionality ID for ECDSA GF2 Sign R
 * @li 7 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::xg xg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::yg yg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::n n @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::q q @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::a a @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::b b @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_input::k k @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_l512_output::r r @endlink
 */
#define PKE_ECDSA_SIGN_S_GF2_L512 0x26182bbe
/**< Functionality ID for ECDSA GF2 Sign S
 * @li 5 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input::e e @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input::d d @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input::k k @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_input::n n @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_l512_output::s s @endlink
 */
#define PKE_ECDSA_VERIFY_GF2_L512 0x58892bea
/**< Functionality ID for ECDSA GF2 Verify
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_verify_gf2_l512_input::in in @endlink
 * @li no output parameters
 */
#define PKE_ECDSA_SIGN_RS_GF2_571 0x554a2c93
/**< Functionality ID for ECDSA GF2 Sign RS for curves B-571/K-571
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_input::in in @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_output::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gf2_571_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_S_GF2_571 0x52332d09
/**< Functionality ID for ECDSA GF2 Sign S for curves with deg(q) &lt; 576
 * @li 5 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input::e e @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input::d d @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input::k k @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_input::n n @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gf2_571_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_R_GF2_571 0x731a2d51
/**< Functionality ID for ECDSA GF2 Sign R for degree 571
 * @li 7 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::xg xg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::yg yg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::n n @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::q q @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::a a @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::b b @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_input::k k @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gf2_571_output::r r @endlink
 */
#define PKE_ECDSA_VERIFY_GF2_571 0x4f6c2d91
/**< Functionality ID for ECDSA GF2 Verify for degree 571
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_verify_gf2_571_input::in in @endlink
 * @li no output parameters
 */
#define MATHS_POINT_MULTIPLICATION_GF2_L256 0x3b242e38
/**< Functionality ID for MATHS GF2 Point Multiplication
 * @li 7 input parameters : @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::k k @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::xg xg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::yg yg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::a a @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::b b @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::q q @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_input::h h @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_output::xk xk @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l256_output::yk yk @endlink
 */
#define MATHS_POINT_VERIFY_GF2_L256 0x231a2e7c
/**< Functionality ID for MATHS GF2 Point Verification
 * @li 5 input parameters : @link
 * icp_qat_fw_maths_point_verify_gf2_l256_input::xq xq @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l256_input::yq yq @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l256_input::q q @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l256_input::a a @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l256_input::b b @endlink
 * @li no output parameters
 */
#define MATHS_POINT_MULTIPLICATION_GF2_L512 0x722c2e96
/**< Functionality ID for MATHS GF2 Point Multiplication
 * @li 7 input parameters : @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::k k @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::xg xg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::yg yg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::a a @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::b b @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::q q @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_input::h h @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_output::xk xk @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_l512_output::yk yk @endlink
 */
#define MATHS_POINT_VERIFY_GF2_L512 0x25132ee2
/**< Functionality ID for MATHS GF2 Point Verification
 * @li 5 input parameters : @link
 * icp_qat_fw_maths_point_verify_gf2_l512_input::xq xq @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l512_input::yq yq @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l512_input::q q @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l512_input::a a @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_l512_input::b b @endlink
 * @li no output parameters
 */
#define MATHS_POINT_MULTIPLICATION_GF2_571 0x44152ef5
/**< Functionality ID for ECC GF2 Point Multiplication for curves B-571/K-571
 * @li 7 input parameters : @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::k k @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::xg xg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::yg yg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::a a @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::b b @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::q q @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_input::h h @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_output::xk xk @endlink @link
 * icp_qat_fw_maths_point_multiplication_gf2_571_output::yk yk @endlink
 */
#define MATHS_POINT_VERIFY_GF2_571 0x12072f1b
/**< Functionality ID for ECC GF2 Point Verification for degree 571
 * @li 5 input parameters : @link
 * icp_qat_fw_maths_point_verify_gf2_571_input::xq xq @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_571_input::yq yq @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_571_input::q q @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_571_input::a a @endlink @link
 * icp_qat_fw_maths_point_verify_gf2_571_input::b b @endlink
 * @li no output parameters
 */
#define PKE_KPT_ECDSA_SIGN_RS_GF2_L256 0x515217d9
/**< Functionality ID for KPT ECDSA Sign RS for curves B/K-163 and B/K-233
 * @li 3 input parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l256_input::in in @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l256_input::d d @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l256_input::c c @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l256_output::r r @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l256_output::s s @endlink
 */
#define PKE_KPT_ECDSA_SIGN_RS_GF2_L512 0x4d811987
/**< Functionality ID for KPT ECDSA Sign RS
 * @li 3 input parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l512_input::in in @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l512_input::d d @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l512_input::c c @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l512_output::r r @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_l512_output::s s @endlink
 */
#define PKE_KPT_ECDSA_SIGN_RS_GF2_571 0x45731898
/**< Functionality ID for KPT ECDSA GF2 Sign RS for curves B-571/K-571
 * @li 3 input parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_571_input::in in @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_571_input::d d @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_571_input::c c @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_571_output::r r @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gf2_571_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_R_GFP_L256 0x431b2f22
/**< Functionality ID for ECDSA GFP Sign R
 * @li 7 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::xg xg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::yg yg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::n n @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::q q @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::a a @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::b b @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_input::k k @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l256_output::r r @endlink
 */
#define PKE_ECDSA_SIGN_S_GFP_L256 0x2b252f6d
/**< Functionality ID for ECDSA GFP Sign S
 * @li 5 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input::e e @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input::d d @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input::k k @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_input::n n @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l256_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_RS_GFP_L256 0x6a3c2fa6
/**< Functionality ID for ECDSA GFP Sign RS
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_input::in in @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_output::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l256_output::s s @endlink
 */
#define PKE_ECDSA_VERIFY_GFP_L256 0x325b3023
/**< Functionality ID for ECDSA GFP Verify
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_verify_gfp_l256_input::in in @endlink
 * @li no output parameters
 */
#define PKE_ECDSA_SIGN_R_GFP_L512 0x4e2530b3
/**< Functionality ID for ECDSA GFP Sign R
 * @li 7 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::xg xg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::yg yg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::n n @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::q q @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::a a @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::b b @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_input::k k @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_l512_output::r r @endlink
 */
#define PKE_ECDSA_SIGN_S_GFP_L512 0x251830fa
/**< Functionality ID for ECDSA GFP Sign S
 * @li 5 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input::e e @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input::d d @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input::k k @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_input::n n @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_l512_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_RS_GFP_L512 0x5a2b3127
/**< Functionality ID for ECDSA GFP Sign RS
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_input::in in @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_output::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_l512_output::s s @endlink
 */
#define PKE_ECDSA_VERIFY_GFP_L512 0x3553318a
/**< Functionality ID for ECDSA GFP Verify
 * @li 1 input parameters : @link
icp_qat_fw_mmp_ecdsa_verify_gfp_l512_input::in in @endlink
 * @li no output parameters
 */
#define PKE_ECDSA_SIGN_R_GFP_521 0x772c31fe
/**< Functionality ID for ECDSA GFP Sign R
 * @li 7 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::xg xg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::yg yg @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::n n @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::q q @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::a a @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::b b @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_input::k k @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_r_gfp_521_output::r r @endlink
 */
#define PKE_ECDSA_SIGN_S_GFP_521 0x52343251
/**< Functionality ID for ECDSA GFP Sign S
 * @li 5 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input::e e @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input::d d @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input::k k @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_input::n n @endlink
 * @li 1 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_s_gfp_521_output::s s @endlink
 */
#define PKE_ECDSA_SIGN_RS_GFP_521 0x494a329b
/**< Functionality ID for ECDSA GFP Sign RS
 * @li 1 input parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_input::in in @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_output::r r @endlink @link
 * icp_qat_fw_mmp_ecdsa_sign_rs_gfp_521_output::s s @endlink
 */
#define PKE_ECDSA_VERIFY_GFP_521 0x554c331f
/**< Functionality ID for ECDSA GFP Verify
 * @li 1 input parameters : @link
icp_qat_fw_mmp_ecdsa_verify_gfp_521_input::in in @endlink
 * @li no output parameters
 */
#define MATHS_POINT_MULTIPLICATION_GFP_L256 0x432033a6
/**< Functionality ID for ECC GFP Point Multiplication
 * @li 7 input parameters : @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::k k @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::xg xg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::yg yg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::a a @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::b b @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::q q @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_input::h h @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_output::xk xk @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l256_output::yk yk @endlink
 */
#define MATHS_POINT_VERIFY_GFP_L256 0x1f0c33fc
/**< Functionality ID for ECC GFP Partial Point Verification
 * @li 5 input parameters : @link
 * icp_qat_fw_maths_point_verify_gfp_l256_input::xq xq @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l256_input::yq yq @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l256_input::q q @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l256_input::a a @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l256_input::b b @endlink
 * @li no output parameters
 */
#define MATHS_POINT_MULTIPLICATION_GFP_L512 0x41253419
/**< Functionality ID for ECC GFP Point Multiplication
 * @li 7 input parameters : @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::k k @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::xg xg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::yg yg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::a a @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::b b @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::q q @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_input::h h @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_output::xk xk @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_l512_output::yk yk @endlink
 */
#define MATHS_POINT_VERIFY_GFP_L512 0x2612345c
/**< Functionality ID for ECC GFP Partial Point
 * @li 5 input parameters : @link
 * icp_qat_fw_maths_point_verify_gfp_l512_input::xq xq @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l512_input::yq yq @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l512_input::q q @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l512_input::a a @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_l512_input::b b @endlink
 * @li no output parameters
 */
#define MATHS_POINT_MULTIPLICATION_GFP_521 0x5511346e
/**< Functionality ID for ECC GFP Point Multiplication
 * @li 7 input parameters : @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::k k @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::xg xg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::yg yg @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::a a @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::b b @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::q q @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_input::h h @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_output::xk xk @endlink @link
 * icp_qat_fw_maths_point_multiplication_gfp_521_output::yk yk @endlink
 */
#define MATHS_POINT_VERIFY_GFP_521 0x0e0734be
/**< Functionality ID for ECC GFP Partial Point Verification
 * @li 5 input parameters : @link
 * icp_qat_fw_maths_point_verify_gfp_521_input::xq xq @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_521_input::yq yq @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_521_input::q q @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_521_input::a a @endlink @link
 * icp_qat_fw_maths_point_verify_gfp_521_input::b b @endlink
 * @li no output parameters
 */
#define PKE_KPT_ECDSA_SIGN_RS_GFP_L256 0x1b6b182c
/**< Functionality ID for KPT ECDSA GFP Sign RS
 * @li 3 input parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l256_input::in in @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l256_input::d d @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l256_input::c c @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l256_output::r r @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l256_output::s s @endlink
 */
#define PKE_KPT_ECDSA_SIGN_RS_GFP_L512 0x7439179f
/**< Functionality ID for KPT ECDSA GFP Sign RS
 * @li 3 input parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l512_input::in in @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l512_input::d d @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l512_input::c c @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l512_output::r r @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_l512_output::s s @endlink
 */
#define PKE_KPT_ECDSA_SIGN_RS_GFP_521 0x3b7a190c
/**< Functionality ID for KPT ECDSA GFP Sign RS
 * @li 3 input parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_521_input::in in @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_521_input::d d @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_521_input::c c @endlink
 * @li 2 output parameters : @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_521_output::r r @endlink @link
 * icp_qat_fw_mmp_kpt_ecdsa_sign_rs_gfp_521_output::s s @endlink
 */

#define PKE_ECSM2_SIGN_RS 0x222116fe
/**< Functionality ID for ECC SM2 Sign RS
 * @li 3 input parameters : @link icp_qat_fw_mmp_ecsm2_sign_rs_input_s::k k
 * @endlink @link icp_qat_fw_mmp_ecsm2_sign_rs_input_s::e e @endlink @link
 * icp_qat_fw_mmp_ecsm2_sign_rs_input_s::d d @endlink
 * @li 2 output parameters : @link icp_qat_fw_mmp_ecsm2_sign_rs_output_s::r r
 * @endlink @link icp_qat_fw_mmp_ecsm2_sign_rs_output_s::s s @endlink
 */
#define PKE_ECSM2_VERIFY 0x29241743
/**< Functionality ID for ECC SM2 Signature Verify
 * @li 5 input parameters : @link icp_qat_fw_mmp_ecsm2_verify_input_s::e e
 * @endlink @link icp_qat_fw_mmp_ecsm2_verify_input_s::r r @endlink @link
 * icp_qat_fw_mmp_ecsm2_verify_input_s::s s @endlink @link
 * icp_qat_fw_mmp_ecsm2_verify_input_s::xp xp @endlink @link
 * icp_qat_fw_mmp_ecsm2_verify_input_s::yp yp @endlink
 * @li no output parameters
 */

#define PKE_LIVENESS 0x00000001
/**< Functionality ID for PKE_LIVENESS
 * @li 0 input parameter(s)
 * @li 1 output parameter(s) (8 qwords)
 */
#define PKE_INTERFACE_SIGNATURE 0x972ded54
/**< Encoded signature of the interface specifications
 */

#define PKE_INVALID_FUNC_ID 0xffffffff

#endif /* __ICP_QAT_FW_MMP_IDS__ */
