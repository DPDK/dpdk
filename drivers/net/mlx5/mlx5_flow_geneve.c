/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_flow.h>

#include <mlx5_malloc.h>
#include <stdint.h>

#include "generic/rte_byteorder.h"
#include "mlx5.h"
#include "mlx5_flow.h"
#include "rte_pmd_mlx5.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)

#define MAX_GENEVE_OPTION_DATA_SIZE 32
#define MAX_GENEVE_OPTION_TOTAL_DATA_SIZE \
		(MAX_GENEVE_OPTION_DATA_SIZE * MAX_GENEVE_OPTIONS_RESOURCES)

#define INVALID_SAMPLE_ID (UINT8_MAX)

/**
 * Single DW inside GENEVE TLV option.
 */
struct mlx5_geneve_tlv_resource {
	struct mlx5_devx_obj *obj; /* FW object returned in parser creation. */
	uint32_t modify_field; /* Modify field ID for this DW. */
	uint8_t offset; /* Offset used in obj creation, from option start. */
};

/**
 * Single GENEVE TLV option context.
 * May include some FW objects for different DWs in same option.
 */
struct mlx5_geneve_tlv_option {
	uint8_t type;
	uint16_t class;
	uint8_t class_mode;
	struct mlx5_hl_data match_data[MAX_GENEVE_OPTION_DATA_SIZE];
	uint32_t match_data_size;
	struct mlx5_hl_data hl_ok_bit;
	struct mlx5_geneve_tlv_resource resources[MAX_GENEVE_OPTIONS_RESOURCES];
	RTE_ATOMIC(uint32_t) refcnt;
};

/**
 * List of GENEVE TLV options.
 */
struct mlx5_geneve_tlv_options {
	/* List of configured GENEVE TLV options. */
	struct mlx5_geneve_tlv_option options[MAX_GENEVE_OPTIONS_RESOURCES];
	/*
	 * Copy of list given in parser creation, use to compare with new
	 * configuration.
	 */
	struct rte_pmd_mlx5_geneve_tlv spec[MAX_GENEVE_OPTIONS_RESOURCES];
	rte_be32_t buffer[MAX_GENEVE_OPTION_TOTAL_DATA_SIZE];
	uint8_t nb_options; /* Number entries in above lists. */
	RTE_ATOMIC(uint32_t) refcnt;
};

/**
 * Check if type and class is matching to given GENEVE TLV option.
 *
 * @param type
 *   GENEVE option type.
 * @param class
 *   GENEVE option class.
 * @param option
 *   Pointer to GENEVE TLV option structure.
 *
 * @return
 *   True if this type and class match to this option, false otherwise.
 */
static inline bool
option_match_type_and_class(uint8_t type, uint16_t class,
			    struct mlx5_geneve_tlv_option *option)
{
	if (type != option->type)
		return false;
	if (option->class_mode == 1 && option->class != class)
		return false;
	return true;
}

/**
 * Get GENEVE TLV option matching to given type and class.
 *
 * @param priv
 *   Pointer to port's private data.
 * @param type
 *   GENEVE option type.
 * @param class
 *   GENEVE option class.
 *
 * @return
 *   Pointer to option structure if exist, NULL otherwise and rte_errno is set.
 */
struct mlx5_geneve_tlv_option *
mlx5_geneve_tlv_option_get(const struct mlx5_priv *priv, uint8_t type,
			   uint16_t class)
{
	struct mlx5_geneve_tlv_options *options;
	uint8_t i;

	if (priv->tlv_options == NULL) {
		DRV_LOG(ERR,
			"Port %u doesn't have configured GENEVE TLV options.",
			priv->dev_data->port_id);
		rte_errno = EINVAL;
		return NULL;
	}
	options = priv->tlv_options;
	MLX5_ASSERT(options != NULL);
	for (i = 0; i < options->nb_options; ++i) {
		struct mlx5_geneve_tlv_option *option = &options->options[i];

		if (option_match_type_and_class(type, class, option))
			return option;
	}
	DRV_LOG(ERR, "TLV option type %u class %u doesn't exist.", type, class);
	rte_errno = ENOENT;
	return NULL;
}

int
mlx5_get_geneve_hl_data(const void *dr_ctx, uint8_t type, uint16_t class,
			struct mlx5_hl_data ** const hl_ok_bit,
			uint8_t *num_of_dws,
			struct mlx5_hl_data ** const hl_dws,
			bool *ok_bit_on_class)
{
	uint16_t port_id;

	MLX5_ETH_FOREACH_DEV(port_id, NULL) {
		struct mlx5_priv *priv;
		struct mlx5_geneve_tlv_option *option;

		priv = rte_eth_devices[port_id].data->dev_private;
		if (priv->dr_ctx != dr_ctx)
			continue;
		/* Find specific option inside list. */
		option = mlx5_geneve_tlv_option_get(priv, type, class);
		if (option == NULL)
			return -rte_errno;
		*hl_ok_bit = &option->hl_ok_bit;
		*hl_dws = option->match_data;
		*num_of_dws = option->match_data_size;
		*ok_bit_on_class = !!(option->class_mode == 1);
		return 0;
	}
	DRV_LOG(ERR, "DR CTX %p doesn't belong to any DPDK port.", dr_ctx);
	return -EINVAL;
}

/**
 * Calculate total data size.
 *
 * @param[in] priv
 *   Pointer to port's private data.
 * @param[in] geneve_opt
 *   Pointer to GENEVE option item structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_geneve_tlv_option_validate(struct mlx5_priv *priv,
				     const struct rte_flow_item *geneve_opt,
				     struct rte_flow_error *error)
{
	const struct rte_flow_item_geneve_opt *spec = geneve_opt->spec;
	const struct rte_flow_item_geneve_opt *mask = geneve_opt->mask;
	struct mlx5_geneve_tlv_option *option;

	option = mlx5_geneve_tlv_option_get(priv, spec->option_type, spec->option_class);
	if (option == NULL)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "Unregistered GENEVE option");
	if (mask->option_type != UINT8_MAX)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "GENEVE option type must be fully masked");
	if (option->class_mode == 1 && mask->option_class != UINT16_MAX)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "GENEVE option class must be fully masked");
	return 0;
}

/**
 * Register single GENEVE TLV option as used by pattern template.
 *
 * @param[in] priv
 *   Pointer to port's private data.
 * @param[in] spec
 *   Pointer to GENEVE option item structure.
 * @param[out] mng
 *   Pointer to GENEVE option manager.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_geneve_tlv_option_register(struct mlx5_priv *priv,
				const struct rte_flow_item_geneve_opt *spec,
				struct mlx5_geneve_tlv_options_mng *mng)
{
	struct mlx5_geneve_tlv_option *option;

	option = mlx5_geneve_tlv_option_get(priv, spec->option_type, spec->option_class);
	if (option == NULL)
		return -rte_errno;
	/* Increase the option reference counter. */
	rte_atomic_fetch_add_explicit(&option->refcnt, 1,
				      rte_memory_order_relaxed);
	/* Update the manager with option information. */
	mng->options[mng->nb_options].opt_type = spec->option_type;
	mng->options[mng->nb_options].opt_class = spec->option_class;
	mng->nb_options++;
	return 0;
}

/**
 * Unregister all GENEVE TLV options used by pattern template.
 *
 * @param[in] priv
 *   Pointer to port's private data.
 * @param[in] mng
 *   Pointer to GENEVE option manager.
 */
void
mlx5_geneve_tlv_options_unregister(struct mlx5_priv *priv,
				   struct mlx5_geneve_tlv_options_mng *mng)
{
	struct mlx5_geneve_tlv_option *option;
	uint8_t i;

	for (i = 0; i < mng->nb_options; ++i) {
		option = mlx5_geneve_tlv_option_get(priv,
						    mng->options[i].opt_type,
						    mng->options[i].opt_class);
		MLX5_ASSERT(option != NULL);
		/* Decrease the option reference counter. */
		rte_atomic_fetch_sub_explicit(&option->refcnt, 1,
					      rte_memory_order_relaxed);
		mng->options[i].opt_type = 0;
		mng->options[i].opt_class = 0;
	}
	mng->nb_options = 0;
}

/**
 * Get single DW resource from given option.
 *
 * @param option
 *   Pointer to single GENEVE TLV option.
 * @param offset
 *   Offset of DW related to option start.
 *
 * @return
 *   DW resource on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_geneve_tlv_resource *
mlx5_geneve_tlv_option_get_resource_by_offset(struct mlx5_geneve_tlv_option *option,
					      uint8_t offset)
{
	uint8_t i;

	for (i = 0; option->resources[i].obj != NULL; ++i) {
		if (option->resources[i].offset < offset)
			continue;
		if (option->resources[i].offset == offset)
			return &option->resources[i];
		break;
	}
	DRV_LOG(ERR, "The DW in offset %u wasn't configured.", offset);
	rte_errno = EINVAL;
	return NULL;
}

int
mlx5_get_geneve_option_modify_field_id(const void *dr_ctx, uint8_t type,
				       uint16_t class, uint8_t dw_offset)
{
	uint16_t port_id;

	MLX5_ETH_FOREACH_DEV(port_id, NULL) {
		struct mlx5_priv *priv;
		struct mlx5_geneve_tlv_option *option;
		struct mlx5_geneve_tlv_resource *resource;

		priv = rte_eth_devices[port_id].data->dev_private;
		if (priv->dr_ctx != dr_ctx)
			continue;
		/* Find specific option inside list. */
		option = mlx5_geneve_tlv_option_get(priv, type, class);
		if (option == NULL)
			return -rte_errno;
		/* Find specific FW object inside option resources. */
		resource = mlx5_geneve_tlv_option_get_resource_by_offset(option,
									 dw_offset);
		if (resource == NULL)
			return -rte_errno;
		return resource->modify_field;
	}
	DRV_LOG(ERR, "DR CTX %p doesn't belong to any DPDK port.", dr_ctx);
	rte_errno = EINVAL;
	return -rte_errno;
}

/**
 * Get modify field ID for single DW inside configured GENEVE TLV option.
 *
 * @param[in] priv
 *   Pointer to port's private data.
 * @param[in] data
 *   Pointer to modify field data structure.
 *
 * @return
 *   Modify field ID on success, negative errno otherwise and rte_errno is set.
 */
int
mlx5_geneve_opt_modi_field_get(struct mlx5_priv *priv,
			       const struct rte_flow_field_data *data)
{
	uint16_t class = data->class_id;
	uint8_t type = data->type;
	struct mlx5_geneve_tlv_option *option;
	struct mlx5_geneve_tlv_resource *resource;
	uint8_t offset;

	option = mlx5_geneve_tlv_option_get(priv, type, class);
	if (option == NULL)
		return -rte_errno;
	switch (data->field) {
	case RTE_FLOW_FIELD_GENEVE_OPT_TYPE:
	case RTE_FLOW_FIELD_GENEVE_OPT_CLASS:
		if (!option->match_data[0].dw_mask) {
			DRV_LOG(ERR, "DW0 isn't configured");
			rte_errno = EINVAL;
			return -rte_errno;
		}
		resource = &option->resources[0];
		MLX5_ASSERT(resource->offset == 0);
		break;
	case RTE_FLOW_FIELD_GENEVE_OPT_DATA:
		/*
		 * Convert offset twice:
		 *  - First conversion from bit offset to DW offset.
		 *  - Second conversion is to be related to data start instead
		 *    of option start.
		 */
		offset = (data->offset >> 5) + 1;
		resource = mlx5_geneve_tlv_option_get_resource_by_offset(option,
									 offset);
		break;
	default:
		DRV_LOG(ERR,
			"Field ID %u doesn't describe GENEVE option header.",
			data->field);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (resource == NULL)
		return -rte_errno;
	return resource->modify_field;
}

/**
 * Create single GENEVE TLV option sample.
 *
 * @param ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param attr
 *   Pointer to GENEVE TLV option attributes structure.
 * @param query_attr
 *   Pointer to match sample info attributes structure.
 * @param match_data
 *   Pointer to header layout structure to update.
 * @param resource
 *   Pointer to single sample context to fill.
 * @param sample_id
 *   The flex parser id for single DW or UINT8_MAX for multiple DWs.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
static int
mlx5_geneve_tlv_option_create_sample(void *ctx,
		      struct mlx5_devx_geneve_tlv_option_attr *attr,
		      struct mlx5_devx_match_sample_info_query_attr *query_attr,
		      struct mlx5_hl_data *match_data,
		      struct mlx5_geneve_tlv_resource *resource, uint8_t sample_id)
{
	struct mlx5_devx_obj *obj;
	int ret;

	obj = mlx5_devx_cmd_create_geneve_tlv_option(ctx, attr);
	if (obj == NULL)
		return -rte_errno;
	if (sample_id == INVALID_SAMPLE_ID)
		ret = mlx5_devx_cmd_query_geneve_tlv_option(ctx, obj, query_attr);
	else
		ret = mlx5_devx_cmd_match_sample_info_query(ctx, sample_id, query_attr);
	if (ret) {
		claim_zero(mlx5_devx_cmd_destroy(obj));
		return ret;
	}
	resource->obj = obj;
	resource->offset = attr->sample_offset;
	resource->modify_field = query_attr->modify_field_id;
	match_data->dw_offset = query_attr->sample_dw_data;
	match_data->dw_mask = 0xffffffff;
	return 0;
}

/**
 * Destroy single GENEVE TLV option sample.
 *
 * @param resource
 *   Pointer to single sample context to clean.
 */
static void
mlx5_geneve_tlv_option_destroy_sample(struct mlx5_geneve_tlv_resource *resource)
{
	claim_zero(mlx5_devx_cmd_destroy(resource->obj));
	resource->obj = NULL;
}

/*
 * Sample for DW0 are created when one of two conditions is met:
 * 1. Header is matchable.
 * 2. This option doesn't configure any data DW.
 */
static bool
should_configure_sample_for_dw0(const struct rte_pmd_mlx5_geneve_tlv *spec)
{
	uint8_t i;

	if (spec->match_on_class_mode == 2)
		return true;
	for (i = 0; i < spec->sample_len; ++i)
		if (spec->match_data_mask[i] != 0)
			return false;
	return true;
}

/**
 * Create single GENEVE TLV option.
 *
 * @param ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param spec
 *   Pointer to user configuration.
 * @param option
 *   Pointer to single GENEVE TLV option to fill.
 * @param sample_id
 *   The flex parser id for single DW or UINT8_MAX for multiple DWs.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
static int
mlx5_geneve_tlv_option_create(void *ctx, const struct rte_pmd_mlx5_geneve_tlv *spec,
			      struct mlx5_geneve_tlv_option *option, uint8_t sample_id)
{
	struct mlx5_devx_geneve_tlv_option_attr attr = {
		.option_class = spec->option_class,
		.option_type = spec->option_type,
		.option_data_len = spec->option_len,
		.option_class_ignore = spec->match_on_class_mode == 1 ? 0 : 1,
		.offset_valid = sample_id == INVALID_SAMPLE_ID ? 1 : 0,
	};
	struct mlx5_devx_match_sample_info_query_attr query_attr = {0};
	struct mlx5_geneve_tlv_resource *resource;
	uint8_t i, resource_id = 0;
	int ret;

	if (should_configure_sample_for_dw0(spec)) {
		MLX5_ASSERT(sample_id == INVALID_SAMPLE_ID);
		attr.sample_offset = 0;
		resource = &option->resources[resource_id];
		ret = mlx5_geneve_tlv_option_create_sample(ctx, &attr,
							   &query_attr,
							   &option->match_data[0],
							   resource,
							   INVALID_SAMPLE_ID);
		if (ret)
			return ret;
		resource_id++;
	}
	/*
	 * Create FW object for each DW request by user.
	 * Starting from 1 since FW offset starts from header.
	 */
	for (i = 1; i <= spec->sample_len; ++i) {
		if (spec->match_data_mask[i - 1] == 0)
			continue;
		/* offset of data + offset inside data = specific DW offset. */
		attr.sample_offset = spec->offset + i;
		resource = &option->resources[resource_id];
		ret = mlx5_geneve_tlv_option_create_sample(ctx, &attr,
							   &query_attr,
							   &option->match_data[i],
							   resource,
							   sample_id);
		if (ret)
			goto error;
		resource_id++;
	}
	/*
	 * Update the OK bit information according to last query.
	 * It should be same for each query under same option.
	 */
	option->hl_ok_bit.dw_offset = query_attr.sample_dw_ok_bit;
	option->hl_ok_bit.dw_mask = 1 << query_attr.sample_dw_ok_bit_offset;
	option->match_data_size = spec->sample_len + 1;
	option->type = spec->option_type;
	option->class = spec->option_class;
	option->class_mode = spec->match_on_class_mode;
	rte_atomic_store_explicit(&option->refcnt, 0, rte_memory_order_relaxed);
	return 0;
error:
	for (i = 0; i < resource_id; ++i) {
		resource = &option->resources[i];
		mlx5_geneve_tlv_option_destroy_sample(resource);
	}
	return ret;
}

/**
 * Destroy single GENEVE TLV option.
 *
 * @param option
 *   Pointer to single GENEVE TLV option to destroy.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
static int
mlx5_geneve_tlv_option_destroy(struct mlx5_geneve_tlv_option *option)
{
	uint8_t i;

	if (rte_atomic_load_explicit(&option->refcnt, rte_memory_order_relaxed)) {
		DRV_LOG(ERR,
			"Option type %u class %u is still in used by %u tables.",
			option->type, option->class, option->refcnt);
		rte_errno = EBUSY;
		return -rte_errno;
	}
	for (i = 0; option->resources[i].obj != NULL; ++i)
		mlx5_geneve_tlv_option_destroy_sample(&option->resources[i]);
	return 0;
}

/**
 * Copy the GENEVE TLV option user configuration for future comparing.
 *
 * @param dst
 *   Pointer to internal user configuration copy.
 * @param src
 *   Pointer to user configuration.
 * @param match_data_mask
 *   Pointer to allocated data array.
 */
static void
mlx5_geneve_tlv_option_copy(struct rte_pmd_mlx5_geneve_tlv *dst,
			    const struct rte_pmd_mlx5_geneve_tlv *src,
			    rte_be32_t *match_data_mask)
{
	uint8_t i;

	dst->option_type = src->option_type;
	dst->option_class = src->option_class;
	dst->option_len = src->option_len;
	dst->offset = src->offset;
	dst->match_on_class_mode = src->match_on_class_mode;
	dst->sample_len = src->sample_len;
	for (i = 0; i < dst->sample_len; ++i)
		match_data_mask[i] = src->match_data_mask[i];
	dst->match_data_mask = match_data_mask;
}

/**
 * Create list of GENEVE TLV options according to user configuration list.
 *
 * @param ctx
 *   Context returned from mlx5 open_device() glue function.
 * @param tlv_list
 *   A list of GENEVE TLV options to create parser for them.
 * @param nb_options
 *   The number of options in TLV list.
 * @param sample_id
 *   The flex parser id for single DW or UINT8_MAX for multiple DWs.
 *
 * @return
 *   A pointer to GENEVE TLV options parser structure on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_geneve_tlv_options *
mlx5_geneve_tlv_options_create(void *ctx,
			       const struct rte_pmd_mlx5_geneve_tlv tlv_list[],
			       uint8_t nb_options, uint8_t sample_id)
{
	struct mlx5_geneve_tlv_options *options;
	const struct rte_pmd_mlx5_geneve_tlv *spec;
	rte_be32_t *data_mask;
	uint8_t i, j;
	int ret;

	options = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_RTE,
			      sizeof(struct mlx5_geneve_tlv_options),
			      RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (options == NULL) {
		DRV_LOG(ERR,
			"Failed to allocate memory for GENEVE TLV options.");
		rte_errno = ENOMEM;
		return NULL;
	}
	for (i = 0; i < nb_options; ++i) {
		spec = &tlv_list[i];
		ret = mlx5_geneve_tlv_option_create(ctx, spec,
						    &options->options[i], sample_id);
		if (ret < 0)
			goto error;
		/* Copy the user list for comparing future configuration. */
		data_mask = options->buffer + i * MAX_GENEVE_OPTION_DATA_SIZE;
		mlx5_geneve_tlv_option_copy(&options->spec[i], spec, data_mask);
	}
	options->nb_options = nb_options;
	options->refcnt = 1;
	return options;
error:
	for (j = 0; j < i; ++j)
		mlx5_geneve_tlv_option_destroy(&options->options[j]);
	mlx5_free(options);
	return NULL;
}

/**
 * Destroy GENEVE TLV options structure.
 *
 * @param options
 *   Pointer to GENEVE TLV options structure to destroy.
 * @param phdev
 *   Pointer physical device options were created on.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
int
mlx5_geneve_tlv_options_destroy(struct mlx5_geneve_tlv_options *options,
				struct mlx5_physical_device *phdev)
{
	uint8_t i;
	int ret;

	if (--options->refcnt)
		return 0;
	for (i = 0; i < options->nb_options; ++i) {
		ret = mlx5_geneve_tlv_option_destroy(&options->options[i]);
		if (ret < 0) {
			DRV_LOG(ERR,
				"Failed to destroy option %u, %u/%u is already destroyed.",
				i, i, options->nb_options);
			return ret;
		}
	}
	mlx5_free(options);
	phdev->tlv_options = NULL;
	return 0;
}

/**
 * Validate GENEVE TLV option user request structure.
 *
 * @param attr
 *   Pointer to HCA attribute structure.
 * @param option
 *   Pointer to user configuration.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
static int
mlx5_geneve_tlv_option_validate(struct mlx5_hca_attr *attr,
				const struct rte_pmd_mlx5_geneve_tlv *option)
{
	if (option->option_len > attr->max_geneve_tlv_option_data_len) {
		DRV_LOG(ERR,
			"GENEVE TLV option length (%u) exceeds the limit (%u).",
			option->option_len,
			attr->max_geneve_tlv_option_data_len);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (option->option_len < option->offset + option->sample_len) {
		DRV_LOG(ERR,
			"GENEVE TLV option length is smaller than (offset + sample_len).");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (option->match_on_class_mode > 2) {
		DRV_LOG(ERR,
			"GENEVE TLV option match_on_class_mode is invalid.");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

/**
 * Get the number of requested DWs in given GENEVE TLV option.
 *
 * @param option
 *   Pointer to user configuration.
 *
 * @return
 *   Number of requested DWs for given GENEVE TLV option.
 */
static uint8_t
mlx5_geneve_tlv_option_get_nb_dws(const struct rte_pmd_mlx5_geneve_tlv *option)
{
	uint8_t nb_dws = 0;
	uint8_t i;

	if (option->match_on_class_mode == 2)
		nb_dws++;
	for (i = 0; i < option->sample_len; ++i) {
		if (option->match_data_mask[i] == 0xffffffff)
			nb_dws++;
	}
	return nb_dws;
}

/**
 * Compare GENEVE TLV option user request structure.
 *
 * @param option1
 *   Pointer to first user configuration.
 * @param option2
 *   Pointer to second user configuration.
 *
 * @return
 *   True if the options are equal, false otherwise.
 */
static bool
mlx5_geneve_tlv_option_compare(const struct rte_pmd_mlx5_geneve_tlv *option1,
			       const struct rte_pmd_mlx5_geneve_tlv *option2)
{
	uint8_t i;

	if (option1->option_type != option2->option_type ||
	    option1->option_class != option2->option_class ||
	    option1->option_len != option2->option_len ||
	    option1->offset != option2->offset ||
	    option1->match_on_class_mode != option2->match_on_class_mode ||
	    option1->sample_len != option2->sample_len)
		return false;
	for (i = 0; i < option1->sample_len; ++i) {
		if (option1->match_data_mask[i] != option2->match_data_mask[i])
			return false;
	}
	return true;
}

/**
 * Check whether the given GENEVE TLV option list is equal to internal list.
 * The lists are equal when they have same size and same options in the same
 * order inside the list.
 *
 * @param options
 *   Pointer to GENEVE TLV options structure.
 * @param tlv_list
 *   A list of GENEVE TLV options to compare.
 * @param nb_options
 *   The number of options in TLV list.
 *
 * @return
 *   True if the lists are equal, false otherwise.
 */
static bool
mlx5_is_same_geneve_tlv_options(const struct mlx5_geneve_tlv_options *options,
				const struct rte_pmd_mlx5_geneve_tlv tlv_list[],
				uint8_t nb_options)
{
	const struct rte_pmd_mlx5_geneve_tlv *spec = options->spec;
	uint8_t i;

	if (options->nb_options != nb_options)
		return false;
	for (i = 0; i < nb_options; ++i) {
		if (!mlx5_geneve_tlv_option_compare(&spec[i], &tlv_list[i]))
			return false;
	}
	return true;
}

static inline bool
multiple_dws_supported(struct mlx5_hca_attr *attr)
{
	return attr->geneve_tlv_option_offset && attr->geneve_tlv_sample;
}

void *
mlx5_geneve_tlv_parser_create(uint16_t port_id,
			      const struct rte_pmd_mlx5_geneve_tlv tlv_list[],
			      uint8_t nb_options)
{
	struct mlx5_geneve_tlv_options *options = NULL;
	struct mlx5_physical_device *phdev;
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	struct mlx5_hca_attr *attr;
	uint8_t sample_id;

	/*
	 * Validate the input before taking a lock and before any memory
	 * allocation.
	 */
	if (rte_eth_dev_is_valid_port(port_id) < 0) {
		DRV_LOG(ERR, "There is no Ethernet device for port %u.",
			port_id);
		rte_errno = ENODEV;
		return NULL;
	}
	dev = &rte_eth_devices[port_id];
	priv = dev->data->dev_private;
	if (priv->tlv_options) {
		DRV_LOG(ERR, "Port %u already has GENEVE TLV parser.", port_id);
		rte_errno = EEXIST;
		return NULL;
	}
	if (priv->sh->config.dv_flow_en < 2) {
		DRV_LOG(ERR,
			"GENEVE TLV parser is only supported for HW steering.");
		rte_errno = ENOTSUP;
		return NULL;
	}
	attr = &priv->sh->cdev->config.hca_attr;
	if (!attr->query_match_sample_info || !attr->geneve_tlv_opt) {
		DRV_LOG(ERR, "Not enough capabilities to support GENEVE TLV parser, is this device eswitch manager?");
		rte_errno = ENOTSUP;
		return NULL;
	}
	DRV_LOG(DEBUG, "Max DWs supported for GENEVE TLV option is %u",
		attr->max_geneve_tlv_options);
	if (nb_options > attr->max_geneve_tlv_options) {
		DRV_LOG(ERR,
			"GENEVE TLV option number (%u) exceeds the limit (%u).",
			nb_options, attr->max_geneve_tlv_options);
		rte_errno = EINVAL;
		return NULL;
	}
	if (multiple_dws_supported(attr)) {
		uint8_t total_dws = 0;
		uint8_t i;

		MLX5_ASSERT(attr->max_geneve_tlv_options >= MAX_GENEVE_OPTIONS_RESOURCES);
		for (i = 0; i < nb_options; ++i) {
			if (mlx5_geneve_tlv_option_validate(attr, &tlv_list[i]) < 0) {
				DRV_LOG(ERR, "GENEVE TLV option %u is invalid.", i);
				return NULL;
			}
			total_dws += mlx5_geneve_tlv_option_get_nb_dws(&tlv_list[i]);
		}
		if (total_dws > MAX_GENEVE_OPTIONS_RESOURCES) {
			DRV_LOG(ERR,
				"Total requested DWs (%u) exceeds the limit (%u).",
				total_dws, MAX_GENEVE_OPTIONS_RESOURCES);
			rte_errno = EINVAL;
			return NULL;
		}
		/* Multiple DWs is supported, each of the has sample ID given later. */
		sample_id = INVALID_SAMPLE_ID;
		DRV_LOG(DEBUG, "GENEVE TLV parser supports multiple DWs, FLEX_PARSER_PROFILE_ENABLE == 8");
	} else {
		const struct rte_pmd_mlx5_geneve_tlv *option = &tlv_list[0];

		if (option->offset != 0) {
			DRV_LOG(ERR,
				"GENEVE TLV option offset %u is required but not supported.",
				option->offset);
			rte_errno = ENOTSUP;
			return NULL;
		}
		if (option->sample_len != option->option_len) {
			DRV_LOG(ERR,
				"GENEVE TLV option length (%u) should be equal to sample length (%u).",
				option->option_len, option->sample_len);
			rte_errno = ENOTSUP;
			return NULL;
		}
		if (option->match_on_class_mode != 1) {
			DRV_LOG(ERR,
				"GENEVE TLV option match_on_class_mode %u is invalid for flex parser profile 0.",
				option->match_on_class_mode);
			rte_errno = EINVAL;
			return NULL;
		}
		if (mlx5_geneve_tlv_option_validate(attr, option) < 0)
			return NULL;
		/* Single DW is supported, its sample ID is given. */
		sample_id = attr->geneve_tlv_option_sample_id;
		DRV_LOG(DEBUG, "GENEVE TLV parser supports only single DW, FLEX_PARSER_PROFILE_ENABLE == 0");
	}
	/* Take lock for this physical device and manage the options. */
	phdev = mlx5_get_locked_physical_device(priv);
	options = priv->sh->phdev->tlv_options;
	if (options) {
		if (!mlx5_is_same_geneve_tlv_options(options, tlv_list,
						     nb_options)) {
			mlx5_unlock_physical_device();
			DRV_LOG(ERR, "Another port has already prepared different GENEVE TLV parser.");
			rte_errno = EEXIST;
			return NULL;
		}
		/* Use existing options. */
		options->refcnt++;
		goto exit;
	}
	/* Create GENEVE TLV options for this physical device. */
	options = mlx5_geneve_tlv_options_create(phdev->ctx, tlv_list, nb_options, sample_id);
	if (!options) {
		mlx5_unlock_physical_device();
		return NULL;
	}
	phdev->tlv_options = options;
exit:
	mlx5_unlock_physical_device();
	priv->tlv_options = options;
	return priv;
}

int
mlx5_geneve_tlv_parser_destroy(void *handle)
{
	struct mlx5_priv *priv = (struct mlx5_priv *)handle;
	struct mlx5_physical_device *phdev;
	int ret;

	if (priv == NULL) {
		DRV_LOG(ERR, "Handle input is invalid (NULL).");
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (priv->tlv_options == NULL) {
		DRV_LOG(ERR, "This parser has been already released.");
		rte_errno = ENOENT;
		return -rte_errno;
	}
	/* Take lock for this physical device and manage the options. */
	phdev = mlx5_get_locked_physical_device(priv);
	/* Destroy the options */
	ret = mlx5_geneve_tlv_options_destroy(phdev->tlv_options, phdev);
	if (ret < 0) {
		mlx5_unlock_physical_device();
		return ret;
	}
	priv->tlv_options = NULL;
	mlx5_unlock_physical_device();
	return 0;
}

#endif /* defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H) */
