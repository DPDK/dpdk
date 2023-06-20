/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5_CRYPTO_H_
#define MLX5_CRYPTO_H_

#include <stdbool.h>

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>

#include <mlx5_common_utils.h>
#include <mlx5_common_devx.h>
#include <mlx5_common_mr.h>

#define MLX5_CRYPTO_DEK_HTABLE_SZ (1 << 11)
#define MLX5_CRYPTO_KEY_LENGTH 80
#define MLX5_CRYPTO_UMR_WQE_STATIC_SIZE (sizeof(struct mlx5_wqe_cseg) +\
					sizeof(struct mlx5_wqe_umr_cseg) +\
					sizeof(struct mlx5_wqe_mkey_cseg) +\
					sizeof(struct mlx5_wqe_umr_bsf_seg))
#define MLX5_CRYPTO_KLM_SEGS_NUM(umr_wqe_sz) ((umr_wqe_sz -\
					MLX5_CRYPTO_UMR_WQE_STATIC_SIZE) /\
					MLX5_WSEG_SIZE)
#define MLX5_CRYPTO_GCM_MAX_AAD 64
#define MLX5_CRYPTO_GCM_MAX_DIGEST 16

struct mlx5_crypto_priv {
	TAILQ_ENTRY(mlx5_crypto_priv) next;
	struct mlx5_common_device *cdev; /* Backend mlx5 device. */
	struct rte_cryptodev *crypto_dev;
	mlx5_reg_mr_t reg_mr_cb; /* Callback to reg_mr func */
	mlx5_dereg_mr_t dereg_mr_cb; /* Callback to dereg_mr func */
	struct mlx5_uar uar; /* User Access Region. */
	uint32_t max_segs_num; /* Maximum supported data segs. */
	uint32_t max_klm_num; /* Maximum supported klm. */
	struct mlx5_hlist *dek_hlist; /* Dek hash list. */
	const struct rte_cryptodev_capabilities *caps;
	struct rte_cryptodev_config dev_config;
	struct mlx5_devx_obj *login_obj;
	uint64_t keytag;
	uint16_t wqe_set_size;
	uint16_t umr_wqe_size;
	uint16_t umr_wqe_stride;
	uint16_t max_rdmar_ds;
	uint32_t is_wrapped_mode:1;
};

struct mlx5_crypto_qp {
	struct mlx5_crypto_priv *priv;
	struct mlx5_devx_cq cq_obj;
	struct mlx5_devx_qp qp_obj;
	struct mlx5_devx_qp umr_qp_obj;
	struct rte_cryptodev_stats stats;
	struct rte_crypto_op **ops;
	struct mlx5_devx_obj **mkey; /* WQE's indirect mekys. */
	struct mlx5_klm *klm_array;
	union mlx5_gga_crypto_opaque *opaque_addr;
	struct mlx5_mr_ctrl mr_ctrl;
	struct mlx5_pmd_mr mr;
	/* Crypto QP. */
	uint8_t *wqe;
	uint16_t entries_n;
	uint16_t cq_entries_n;
	uint16_t reported_ci;
	uint16_t qp_ci;
	uint16_t cq_ci;
	uint16_t pi;
	uint16_t ci;
	uint16_t db_pi;
	/* UMR QP. */
	uint8_t *umr_wqe;
	uint16_t umr_wqbbs;
	uint16_t umr_pi;
	uint16_t umr_ci;
	uint32_t umr_errors;
	uint16_t last_gga_pi;
	bool has_umr;
	uint16_t cpy_tag_op;
};

struct mlx5_crypto_dek {
	struct mlx5_list_entry entry; /* Pointer to DEK hash list entry. */
	struct mlx5_devx_obj *obj; /* Pointer to DEK DevX object. */
	uint8_t data[MLX5_CRYPTO_KEY_LENGTH]; /* DEK key data. */
	uint32_t size; /* key+keytag size. */
} __rte_cache_aligned;

struct mlx5_crypto_devarg_params {
	bool login_devarg;
	struct mlx5_devx_crypto_login_attr login_attr;
	uint64_t keytag;
	uint32_t max_segs_num;
	uint32_t is_aes_gcm:1;
};

struct mlx5_crypto_session {
	union {
		/**< AES-XTS configuration. */
		struct {
			uint32_t bs_bpt_eo_es;
			/**< bsf_size, bsf_p_type, encryption_order and encryption standard,
			 * saved in big endian format.
			 */
			uint32_t bsp_res;
			/**< crypto_block_size_pointer and reserved 24 bits saved in big
			 * endian format.
			 */
		};
		/**< AES-GCM configuration. */
		struct {
			uint32_t mmo_ctrl;
			/**< Crypto control fields with algo type and op type in big
			 * endian format.
			 */
			uint32_t wqe_aad_len;
			/**< Crypto AAD length field in big endian format. */
			uint32_t wqe_tag_len;
			/**< Crypto tag length field in big endian format. */
			uint16_t tag_len;
			/**< AES-GCM crypto digest size in bytes. */
			uint16_t aad_len;
			/**< The length of the additional authenticated data (AAD) in bytes. */
			uint32_t op_type;
			/**< Operation type. */
		};
	};
	uint32_t iv_offset:16;
	/**< Starting point for Initialisation Vector. */
	uint32_t iv_len;
	/**< Initialisation Vector length. */
	struct mlx5_crypto_dek *dek; /**< Pointer to dek struct. */
	uint32_t dek_id; /**< DEK ID */
} __rte_packed;

struct mlx5_crypto_dek_ctx {
	struct rte_crypto_sym_xform *xform;
	struct mlx5_crypto_priv *priv;
};

typedef void *(*mlx5_crypto_mkey_update_t)(struct mlx5_crypto_priv *priv,
					   struct mlx5_crypto_qp *qp,
					   uint32_t idx);

void
mlx5_crypto_indirect_mkeys_release(struct mlx5_crypto_qp *qp,
				   uint16_t n);

int
mlx5_crypto_indirect_mkeys_prepare(struct mlx5_crypto_priv *priv,
				   struct mlx5_crypto_qp *qp,
				   struct mlx5_devx_mkey_attr *attr,
				   mlx5_crypto_mkey_update_t update_cb);

int
mlx5_crypto_dek_destroy(struct mlx5_crypto_priv *priv,
			struct mlx5_crypto_dek *dek);

struct mlx5_crypto_dek *
mlx5_crypto_dek_prepare(struct mlx5_crypto_priv *priv,
			struct rte_crypto_sym_xform *xform);

int
mlx5_crypto_dek_setup(struct mlx5_crypto_priv *priv);

void
mlx5_crypto_dek_unset(struct mlx5_crypto_priv *priv);

int
mlx5_crypto_xts_init(struct mlx5_crypto_priv *priv);

int
mlx5_crypto_gcm_init(struct mlx5_crypto_priv *priv);

int
mlx5_crypto_dek_fill_xts_attr(struct mlx5_crypto_dek *dek,
			      struct mlx5_devx_dek_attr *dek_attr,
			      void *cb_ctx);

int
mlx5_crypto_dek_fill_gcm_attr(struct mlx5_crypto_dek *dek,
			      struct mlx5_devx_dek_attr *dek_attr,
			      void *cb_ctx);

#endif /* MLX5_CRYPTO_H_ */
