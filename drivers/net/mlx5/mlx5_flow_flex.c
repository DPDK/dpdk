/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */
#include <rte_malloc.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_malloc.h>
#include "mlx5.h"
#include "mlx5_flow.h"

static_assert(sizeof(uint32_t) * CHAR_BIT >= MLX5_PORT_FLEX_ITEM_NUM,
	      "Flex item maximal number exceeds uint32_t bit width");

/**
 *  Routine called once on port initialization to init flex item
 *  related infrastructure initialization
 *
 * @param dev
 *   Ethernet device to perform flex item initialization
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flex_item_port_init(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	rte_spinlock_init(&priv->flex_item_sl);
	MLX5_ASSERT(!priv->flex_item_map);
	return 0;
}

/**
 *  Routine called once on port close to perform flex item
 *  related infrastructure cleanup.
 *
 * @param dev
 *   Ethernet device to perform cleanup
 */
void
mlx5_flex_item_port_cleanup(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < MLX5_PORT_FLEX_ITEM_NUM && priv->flex_item_map ; i++) {
		if (priv->flex_item_map & (1 << i)) {
			struct mlx5_flex_item *flex = &priv->flex_item[i];

			claim_zero(mlx5_list_unregister
					(priv->sh->flex_parsers_dv,
					 &flex->devx_fp->entry));
			flex->devx_fp = NULL;
			flex->refcnt = 0;
			priv->flex_item_map &= ~(1 << i);
		}
	}
}

static int
mlx5_flex_index(struct mlx5_priv *priv, struct mlx5_flex_item *item)
{
	uintptr_t start = (uintptr_t)&priv->flex_item[0];
	uintptr_t entry = (uintptr_t)item;
	uintptr_t idx = (entry - start) / sizeof(struct mlx5_flex_item);

	if (entry < start ||
	    idx >= MLX5_PORT_FLEX_ITEM_NUM ||
	    (entry - start) % sizeof(struct mlx5_flex_item) ||
	    !(priv->flex_item_map & (1u << idx)))
		return -1;
	return (int)idx;
}

static struct mlx5_flex_item *
mlx5_flex_alloc(struct mlx5_priv *priv)
{
	struct mlx5_flex_item *item = NULL;

	rte_spinlock_lock(&priv->flex_item_sl);
	if (~priv->flex_item_map) {
		uint32_t idx = rte_bsf32(~priv->flex_item_map);

		if (idx < MLX5_PORT_FLEX_ITEM_NUM) {
			item = &priv->flex_item[idx];
			MLX5_ASSERT(!item->refcnt);
			MLX5_ASSERT(!item->devx_fp);
			item->devx_fp = NULL;
			__atomic_store_n(&item->refcnt, 0, __ATOMIC_RELEASE);
			priv->flex_item_map |= 1u << idx;
		}
	}
	rte_spinlock_unlock(&priv->flex_item_sl);
	return item;
}

static void
mlx5_flex_free(struct mlx5_priv *priv, struct mlx5_flex_item *item)
{
	int idx = mlx5_flex_index(priv, item);

	MLX5_ASSERT(idx >= 0 &&
		    idx < MLX5_PORT_FLEX_ITEM_NUM &&
		    (priv->flex_item_map & (1u << idx)));
	if (idx >= 0) {
		rte_spinlock_lock(&priv->flex_item_sl);
		MLX5_ASSERT(!item->refcnt);
		MLX5_ASSERT(!item->devx_fp);
		item->devx_fp = NULL;
		__atomic_store_n(&item->refcnt, 0, __ATOMIC_RELEASE);
		priv->flex_item_map &= ~(1u << idx);
		rte_spinlock_unlock(&priv->flex_item_sl);
	}
}

static uint32_t
mlx5_flex_get_bitfield(const struct rte_flow_item_flex *item,
		       uint32_t pos, uint32_t width, uint32_t shift)
{
	const uint8_t *ptr = item->pattern + pos / CHAR_BIT;
	uint32_t val, vbits;

	/* Proceed the bitfield start byte. */
	MLX5_ASSERT(width <= sizeof(uint32_t) * CHAR_BIT && width);
	MLX5_ASSERT(width + shift <= sizeof(uint32_t) * CHAR_BIT);
	if (item->length <= pos / CHAR_BIT)
		return 0;
	val = *ptr++ >> (pos % CHAR_BIT);
	vbits = CHAR_BIT - pos % CHAR_BIT;
	pos = (pos + vbits) / CHAR_BIT;
	vbits = RTE_MIN(vbits, width);
	val &= RTE_BIT32(vbits) - 1;
	while (vbits < width && pos < item->length) {
		uint32_t part = RTE_MIN(width - vbits, (uint32_t)CHAR_BIT);
		uint32_t tmp = *ptr++;

		pos++;
		tmp &= RTE_BIT32(part) - 1;
		val |= tmp << vbits;
		vbits += part;
	}
	return rte_bswap32(val <<= shift);
}

#define SET_FP_MATCH_SAMPLE_ID(x, def, msk, val, sid) \
	do { \
		uint32_t tmp, out = (def); \
		tmp = MLX5_GET(fte_match_set_misc4, misc4_v, \
			       prog_sample_field_value_##x); \
		tmp = (tmp & ~out) | (val); \
		MLX5_SET(fte_match_set_misc4, misc4_v, \
			 prog_sample_field_value_##x, tmp); \
		tmp = MLX5_GET(fte_match_set_misc4, misc4_m, \
			       prog_sample_field_value_##x); \
		tmp = (tmp & ~out) | (msk); \
		MLX5_SET(fte_match_set_misc4, misc4_m, \
			 prog_sample_field_value_##x, tmp); \
		tmp = tmp ? (sid) : 0; \
		MLX5_SET(fte_match_set_misc4, misc4_v, \
			 prog_sample_field_id_##x, tmp);\
		MLX5_SET(fte_match_set_misc4, misc4_m, \
			 prog_sample_field_id_##x, tmp); \
	} while (0)

__rte_always_inline static void
mlx5_flex_set_match_sample(void *misc4_m, void *misc4_v,
			   uint32_t def, uint32_t mask, uint32_t value,
			   uint32_t sample_id, uint32_t id)
{
	switch (id) {
	case 0:
		SET_FP_MATCH_SAMPLE_ID(0, def, mask, value, sample_id);
		break;
	case 1:
		SET_FP_MATCH_SAMPLE_ID(1, def, mask, value, sample_id);
		break;
	case 2:
		SET_FP_MATCH_SAMPLE_ID(2, def, mask, value, sample_id);
		break;
	case 3:
		SET_FP_MATCH_SAMPLE_ID(3, def, mask, value, sample_id);
		break;
	case 4:
		SET_FP_MATCH_SAMPLE_ID(4, def, mask, value, sample_id);
		break;
	case 5:
		SET_FP_MATCH_SAMPLE_ID(5, def, mask, value, sample_id);
		break;
	case 6:
		SET_FP_MATCH_SAMPLE_ID(6, def, mask, value, sample_id);
		break;
	case 7:
		SET_FP_MATCH_SAMPLE_ID(7, def, mask, value, sample_id);
		break;
	default:
		MLX5_ASSERT(false);
		break;
	}
#undef SET_FP_MATCH_SAMPLE_ID
}

/**
 * Get the flex parser sample id and corresponding mask
 * per shift and width information.
 *
 * @param[in] tp
 *   Mlx5 flex item sample mapping handle.
 * @param[in] idx
 *   Mapping index.
 * @param[in, out] pos
 *   Where to search the value and mask.
 * @param[in] is_inner
 *   For inner matching or not.
 * @param[in, def] def
 *   Mask generated by mapping shift and width.
 *
 * @return
 *   0 on success, -1 to ignore.
 */
int
mlx5_flex_get_sample_id(const struct mlx5_flex_item *tp,
			uint32_t idx, uint32_t *pos,
			bool is_inner, uint32_t *def)
{
	const struct mlx5_flex_pattern_field *map = tp->map + idx;
	uint32_t id = map->reg_id;

	*def = (RTE_BIT64(map->width) - 1) << map->shift;
	/* Skip placeholders for DUMMY fields. */
	if (id == MLX5_INVALID_SAMPLE_REG_ID) {
		*pos += map->width;
		return -1;
	}
	MLX5_ASSERT(map->width);
	MLX5_ASSERT(id < tp->devx_fp->num_samples);
	if (tp->tunnel_mode == FLEX_TUNNEL_MODE_MULTI && is_inner) {
		uint32_t num_samples = tp->devx_fp->num_samples / 2;

		MLX5_ASSERT(tp->devx_fp->num_samples % 2 == 0);
		MLX5_ASSERT(id < num_samples);
		id += num_samples;
	}
	return id;
}

/**
 * Get the flex parser mapping value per definer format_select_dw.
 *
 * @param[in] item
 *   Rte flex item pointer.
 * @param[in] flex
 *   Mlx5 flex item sample mapping handle.
 * @param[in] byte_off
 *   Mlx5 flex item format_select_dw.
 * @param[in] is_mask
 *   Spec or mask.
 * @param[in] tunnel
 *   Tunnel mode or not.
 * @param[in, def] value
 *   Value calculated for this flex parser, either spec or mask.
 *
 * @return
 *   0 on success, -1 for error.
 */
int
mlx5_flex_get_parser_value_per_byte_off(const struct rte_flow_item_flex *item,
					void *flex, uint32_t byte_off,
					bool is_mask, bool tunnel, uint32_t *value)
{
	struct mlx5_flex_pattern_field *map;
	struct mlx5_flex_item *tp = flex;
	uint32_t def, i, pos, val;
	int id;

	*value = 0;
	for (i = 0, pos = 0; i < tp->mapnum && pos < item->length * CHAR_BIT; i++) {
		map = tp->map + i;
		id = mlx5_flex_get_sample_id(tp, i, &pos, tunnel, &def);
		if (id == -1)
			continue;
		if (id >= (int)tp->devx_fp->num_samples || id >= MLX5_GRAPH_NODE_SAMPLE_NUM)
			return -1;
		if (byte_off == tp->devx_fp->sample_info[id].sample_dw_data * sizeof(uint32_t)) {
			val = mlx5_flex_get_bitfield(item, pos, map->width, map->shift);
			if (is_mask)
				val &= RTE_BE32(def);
			*value |= val;
		}
		pos += map->width;
	}
	return 0;
}

/**
 * Translate item pattern into matcher fields according to translation
 * array.
 *
 * @param dev
 *   Ethernet device to translate flex item on.
 * @param[in, out] matcher
 *   Flow matcher to configure
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] is_inner
 *   Inner Flex Item (follows after tunnel header).
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
void
mlx5_flex_flow_translate_item(struct rte_eth_dev *dev,
			      void *matcher, void *key,
			      const struct rte_flow_item *item,
			      bool is_inner)
{
	const struct rte_flow_item_flex *spec, *mask;
	void *misc4_m = MLX5_ADDR_OF(fte_match_param, matcher,
				     misc_parameters_4);
	void *misc4_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_4);
	struct mlx5_flex_item *tp;
	uint32_t i, pos = 0;
	uint32_t sample_id;

	RTE_SET_USED(dev);
	MLX5_ASSERT(item->spec && item->mask);
	spec = item->spec;
	mask = item->mask;
	tp = (struct mlx5_flex_item *)spec->handle;
	for (i = 0; i < tp->mapnum; i++) {
		struct mlx5_flex_pattern_field *map = tp->map + i;
		uint32_t val, msk, def;
		int id = mlx5_flex_get_sample_id(tp, i, &pos, is_inner, &def);

		if (id == -1)
			continue;
		MLX5_ASSERT(id < (int)tp->devx_fp->num_samples);
		if (id >= (int)tp->devx_fp->num_samples ||
		    id >= MLX5_GRAPH_NODE_SAMPLE_NUM)
			return;
		val = mlx5_flex_get_bitfield(spec, pos, map->width, map->shift);
		msk = mlx5_flex_get_bitfield(mask, pos, map->width, map->shift);
		sample_id = tp->devx_fp->sample_ids[id];
		mlx5_flex_set_match_sample(misc4_m, misc4_v,
					   def, msk & def, val & msk & def,
					   sample_id, id);
		pos += map->width;
	}
}

/**
 * Convert flex item handle (from the RTE flow) to flex item index on port.
 * Optionally can increment flex item object reference count.
 *
 * @param dev
 *   Ethernet device to acquire flex item on.
 * @param[in] handle
 *   Flow item handle from item spec.
 * @param[in] acquire
 *   If set - increment reference counter.
 *
 * @return
 *   >=0 - index on success, a negative errno value otherwise
 *         and rte_errno is set.
 */
int
mlx5_flex_acquire_index(struct rte_eth_dev *dev,
			struct rte_flow_item_flex_handle *handle,
			bool acquire)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_item *flex = (struct mlx5_flex_item *)handle;
	int ret = mlx5_flex_index(priv, flex);

	if (ret < 0) {
		errno = -EINVAL;
		rte_errno = EINVAL;
		return ret;
	}
	if (acquire)
		__atomic_fetch_add(&flex->refcnt, 1, __ATOMIC_RELEASE);
	return ret;
}

/**
 * Release flex item index on port - decrements reference counter by index.
 *
 * @param dev
 *   Ethernet device to acquire flex item on.
 * @param[in] index
 *   Flow item index.
 *
 * @return
 *   0 - on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flex_release_index(struct rte_eth_dev *dev,
			int index)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_item *flex;

	if (index >= MLX5_PORT_FLEX_ITEM_NUM ||
	    !(priv->flex_item_map & (1u << index))) {
		errno = EINVAL;
		rte_errno = -EINVAL;
		return -EINVAL;
	}
	flex = priv->flex_item + index;
	if (flex->refcnt <= 1) {
		MLX5_ASSERT(false);
		errno = EINVAL;
		rte_errno = -EINVAL;
		return -EINVAL;
	}
	__atomic_fetch_sub(&flex->refcnt, 1, __ATOMIC_RELEASE);
	return 0;
}

/*
 * Calculate largest mask value for a given shift.
 *
 *   shift      mask
 * ------- ---------------
 *    0     b111100  0x3C
 *    1     b111110  0x3E
 *    2     b111111  0x3F
 *    3     b011111  0x1F
 *    4     b001111  0x0F
 *    5     b000111  0x07
 */
static uint8_t
mlx5_flex_hdr_len_mask(uint8_t shift,
		       const struct mlx5_hca_flex_attr *attr)
{
	uint32_t base_mask;
	int diff = shift - MLX5_PARSE_GRAPH_NODE_HDR_LEN_SHIFT_DWORD;

	base_mask = mlx5_hca_parse_graph_node_base_hdr_len_mask(attr);
	return diff == 0 ? base_mask :
	       diff < 0 ? (base_mask << -diff) & base_mask : base_mask >> diff;
}

static int
mlx5_flex_translate_length(struct mlx5_hca_flex_attr *attr,
			   const struct rte_flow_item_flex_conf *conf,
			   struct mlx5_flex_parser_devx *devx,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_flex_field *field = &conf->next_header;
	struct mlx5_devx_graph_node_attr *node = &devx->devx_conf;
	uint32_t len_width, mask;

	if (field->field_base % CHAR_BIT)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "not byte aligned header length field");
	switch (field->field_mode) {
	case FIELD_MODE_DUMMY:
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "invalid header length field mode (DUMMY)");
	case FIELD_MODE_FIXED:
		if (!(attr->header_length_mode &
		    RTE_BIT32(MLX5_GRAPH_NODE_LEN_FIXED)))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported header length field mode (FIXED)");
		if (field->field_size ||
		    field->offset_mask || field->offset_shift)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "invalid fields for fixed mode");
		if (field->field_base < 0)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "negative header length field base (FIXED)");
		node->header_length_mode = MLX5_GRAPH_NODE_LEN_FIXED;
		break;
	case FIELD_MODE_OFFSET:
		if (!(attr->header_length_mode &
		    RTE_BIT32(MLX5_GRAPH_NODE_LEN_FIELD)))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported header length field mode (OFFSET)");
		node->header_length_mode = MLX5_GRAPH_NODE_LEN_FIELD;
		if (field->offset_mask == 0 ||
		    !rte_is_power_of_2(field->offset_mask + 1))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "invalid length field offset mask (OFFSET)");
		len_width = rte_fls_u32(field->offset_mask);
		if (len_width > attr->header_length_mask_width)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "length field offset mask too wide (OFFSET)");
		mask = mlx5_flex_hdr_len_mask(field->offset_shift, attr);
		if (mask < field->offset_mask)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "length field shift too big (OFFSET)");
		node->header_length_field_mask = RTE_MIN(mask,
							 field->offset_mask);
		break;
	case FIELD_MODE_BITMASK:
		if (!(attr->header_length_mode &
		    RTE_BIT32(MLX5_GRAPH_NODE_LEN_BITMASK)))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported header length field mode (BITMASK)");
		if (attr->header_length_mask_width < field->field_size)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "header length field width exceeds limit");
		node->header_length_mode = MLX5_GRAPH_NODE_LEN_BITMASK;
		mask = mlx5_flex_hdr_len_mask(field->offset_shift, attr);
		if (mask < field->offset_mask)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "length field shift too big (BITMASK)");
		node->header_length_field_mask = RTE_MIN(mask,
							 field->offset_mask);
		break;
	default:
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unknown header length field mode");
	}
	if (field->field_base / CHAR_BIT >= 0 &&
	    field->field_base / CHAR_BIT > attr->max_base_header_length)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "header length field base exceeds limit");
	node->header_length_base_value = field->field_base / CHAR_BIT;
	if (field->field_mode == FIELD_MODE_OFFSET ||
	    field->field_mode == FIELD_MODE_BITMASK) {
		if (field->offset_shift > 15 || field->offset_shift < 0)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "header length field shift exceeds limit");
		node->header_length_field_shift	= field->offset_shift;
		node->header_length_field_offset = field->offset_base;
	}
	return 0;
}

static int
mlx5_flex_translate_next(struct mlx5_hca_flex_attr *attr,
			 const struct rte_flow_item_flex_conf *conf,
			 struct mlx5_flex_parser_devx *devx,
			 struct rte_flow_error *error)
{
	const struct rte_flow_item_flex_field *field = &conf->next_protocol;
	struct mlx5_devx_graph_node_attr *node = &devx->devx_conf;

	switch (field->field_mode) {
	case FIELD_MODE_DUMMY:
		if (conf->nb_outputs)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "next protocol field is required (DUMMY)");
		return 0;
	case FIELD_MODE_FIXED:
		break;
	case FIELD_MODE_OFFSET:
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unsupported next protocol field mode (OFFSET)");
		break;
	case FIELD_MODE_BITMASK:
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unsupported next protocol field mode (BITMASK)");
	default:
		return rte_flow_error_set
			(error, EINVAL,
			 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unknown next protocol field mode");
	}
	MLX5_ASSERT(field->field_mode == FIELD_MODE_FIXED);
	if (!conf->nb_outputs)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "out link(s) is required if next field present");
	if (attr->max_next_header_offset < field->field_base)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "next protocol field base exceeds limit");
	if (field->offset_shift)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unsupported next protocol field shift");
	node->next_header_field_offset = field->field_base;
	node->next_header_field_size = field->field_size;
	return 0;
}

/* Helper structure to handle field bit intervals. */
struct mlx5_flex_field_cover {
	uint16_t num;
	int32_t start[MLX5_FLEX_ITEM_MAPPING_NUM];
	int32_t end[MLX5_FLEX_ITEM_MAPPING_NUM];
	uint8_t mapped[MLX5_FLEX_ITEM_MAPPING_NUM / CHAR_BIT + 1];
};

static void
mlx5_flex_insert_field(struct mlx5_flex_field_cover *cover,
		       uint16_t num, int32_t start, int32_t end)
{
	MLX5_ASSERT(num < MLX5_FLEX_ITEM_MAPPING_NUM);
	MLX5_ASSERT(num <= cover->num);
	if (num < cover->num) {
		memmove(&cover->start[num + 1],	&cover->start[num],
			(cover->num - num) * sizeof(int32_t));
		memmove(&cover->end[num + 1],	&cover->end[num],
			(cover->num - num) * sizeof(int32_t));
	}
	cover->start[num] = start;
	cover->end[num] = end;
	cover->num++;
}

static void
mlx5_flex_merge_field(struct mlx5_flex_field_cover *cover, uint16_t num)
{
	uint32_t i, del = 0;
	int32_t end;

	MLX5_ASSERT(num < MLX5_FLEX_ITEM_MAPPING_NUM);
	MLX5_ASSERT(num < (cover->num - 1));
	end = cover->end[num];
	for (i = num + 1; i < cover->num; i++) {
		if (end < cover->start[i])
			break;
		del++;
		if (end <= cover->end[i]) {
			cover->end[num] = cover->end[i];
			break;
		}
	}
	if (del) {
		MLX5_ASSERT(del < (cover->num - 1u - num));
		cover->num -= del;
		MLX5_ASSERT(cover->num > num);
		if ((cover->num - num) > 1) {
			memmove(&cover->start[num + 1],
				&cover->start[num + 1 + del],
				(cover->num - num - 1) * sizeof(int32_t));
			memmove(&cover->end[num + 1],
				&cover->end[num + 1 + del],
				(cover->num - num - 1) * sizeof(int32_t));
		}
	}
}

/*
 * Validate the sample field and update interval array
 * if parameters match with the 'match" field.
 * Returns:
 *    < 0  - error
 *    == 0 - no match, interval array not updated
 *    > 0  - match, interval array updated
 */
static int
mlx5_flex_cover_sample(struct mlx5_flex_field_cover *cover,
		       struct rte_flow_item_flex_field *field,
		       struct rte_flow_item_flex_field *match,
		       struct mlx5_hca_flex_attr *attr,
		       struct rte_flow_error *error)
{
	int32_t start, end;
	uint32_t i;

	switch (field->field_mode) {
	case FIELD_MODE_DUMMY:
		return 0;
	case FIELD_MODE_FIXED:
		if (!(attr->sample_offset_mode &
		    RTE_BIT32(MLX5_GRAPH_SAMPLE_OFFSET_FIXED)))
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported sample field mode (FIXED)");
		if (field->offset_shift)
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "invalid sample field shift (FIXED");
		if (field->field_base < 0)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "invalid sample field base (FIXED)");
		if (field->field_base / CHAR_BIT > attr->max_sample_base_offset)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "sample field base exceeds limit (FIXED)");
		break;
	case FIELD_MODE_OFFSET:
		if (!(attr->sample_offset_mode &
		    RTE_BIT32(MLX5_GRAPH_SAMPLE_OFFSET_FIELD)))
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported sample field mode (OFFSET)");
		if (field->field_base / CHAR_BIT >= 0 &&
		    field->field_base / CHAR_BIT > attr->max_sample_base_offset)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"sample field base exceeds limit");
		break;
	case FIELD_MODE_BITMASK:
		if (!(attr->sample_offset_mode &
		    RTE_BIT32(MLX5_GRAPH_SAMPLE_OFFSET_BITMASK)))
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported sample field mode (BITMASK)");
		if (field->field_base / CHAR_BIT >= 0 &&
		    field->field_base / CHAR_BIT > attr->max_sample_base_offset)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"sample field base exceeds limit");
		break;
	default:
		return rte_flow_error_set
			(error, EINVAL,
			 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unknown data sample field mode");
	}
	if (!match) {
		if (!field->field_size)
			return rte_flow_error_set
				(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"zero sample field width");
		if (field->field_id)
			DRV_LOG(DEBUG, "sample field id hint ignored");
	} else {
		if (field->field_mode != match->field_mode ||
		    field->offset_base | match->offset_base ||
		    field->offset_mask | match->offset_mask ||
		    field->offset_shift | match->offset_shift)
			return 0;
	}
	start = field->field_base;
	end = start + field->field_size;
	/* Add the new or similar field to interval array. */
	if (!cover->num) {
		cover->start[cover->num] = start;
		cover->end[cover->num] = end;
		cover->num = 1;
		return 1;
	}
	for (i = 0; i < cover->num; i++) {
		if (start > cover->end[i]) {
			if (i >= (cover->num - 1u)) {
				mlx5_flex_insert_field(cover, cover->num,
						       start, end);
				break;
			}
			continue;
		}
		if (end < cover->start[i]) {
			mlx5_flex_insert_field(cover, i, start, end);
			break;
		}
		if (start < cover->start[i])
			cover->start[i] = start;
		if (end > cover->end[i]) {
			cover->end[i] = end;
			if (i < (cover->num - 1u))
				mlx5_flex_merge_field(cover, i);
		}
		break;
	}
	return 1;
}

static void
mlx5_flex_config_sample(struct mlx5_devx_match_sample_attr *na,
			struct rte_flow_item_flex_field *field,
			enum rte_flow_item_flex_tunnel_mode tunnel_mode)
{
	memset(na, 0, sizeof(struct mlx5_devx_match_sample_attr));
	na->flow_match_sample_en = 1;
	switch (field->field_mode) {
	case FIELD_MODE_FIXED:
		na->flow_match_sample_offset_mode =
			MLX5_GRAPH_SAMPLE_OFFSET_FIXED;
		break;
	case FIELD_MODE_OFFSET:
		na->flow_match_sample_offset_mode =
			MLX5_GRAPH_SAMPLE_OFFSET_FIELD;
		na->flow_match_sample_field_offset = field->offset_base;
		na->flow_match_sample_field_offset_mask = field->offset_mask;
		na->flow_match_sample_field_offset_shift = field->offset_shift;
		break;
	case FIELD_MODE_BITMASK:
		na->flow_match_sample_offset_mode =
			MLX5_GRAPH_SAMPLE_OFFSET_BITMASK;
		na->flow_match_sample_field_offset = field->offset_base;
		na->flow_match_sample_field_offset_mask = field->offset_mask;
		na->flow_match_sample_field_offset_shift = field->offset_shift;
		break;
	default:
		MLX5_ASSERT(false);
		break;
	}
	switch (tunnel_mode) {
	case FLEX_TUNNEL_MODE_SINGLE:
		/* Fallthrough */
	case FLEX_TUNNEL_MODE_TUNNEL:
		na->flow_match_sample_tunnel_mode =
			MLX5_GRAPH_SAMPLE_TUNNEL_FIRST;
		break;
	case FLEX_TUNNEL_MODE_MULTI:
		/* Fallthrough */
	case FLEX_TUNNEL_MODE_OUTER:
		na->flow_match_sample_tunnel_mode =
			MLX5_GRAPH_SAMPLE_TUNNEL_OUTER;
		break;
	case FLEX_TUNNEL_MODE_INNER:
		na->flow_match_sample_tunnel_mode =
			MLX5_GRAPH_SAMPLE_TUNNEL_INNER;
		break;
	default:
		MLX5_ASSERT(false);
		break;
	}
}

/* Map specified field to set/subset of allocated sample registers. */
static int
mlx5_flex_map_sample(struct rte_flow_item_flex_field *field,
		     struct mlx5_flex_parser_devx *parser,
		     struct mlx5_flex_item *item,
		     struct rte_flow_error *error)
{
	struct mlx5_devx_match_sample_attr node;
	int32_t start = field->field_base;
	int32_t end = start + field->field_size;
	struct mlx5_flex_pattern_field *trans;
	uint32_t i, done_bits = 0;

	if (field->field_mode == FIELD_MODE_DUMMY) {
		done_bits = field->field_size;
		while (done_bits) {
			uint32_t part = RTE_MIN(done_bits,
						sizeof(uint32_t) * CHAR_BIT);
			if (item->mapnum >= MLX5_FLEX_ITEM_MAPPING_NUM)
				return rte_flow_error_set
					(error,
					 EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					 "too many flex item pattern translations");
			trans = &item->map[item->mapnum];
			trans->reg_id = MLX5_INVALID_SAMPLE_REG_ID;
			trans->shift = 0;
			trans->width = part;
			item->mapnum++;
			done_bits -= part;
		}
		return 0;
	}
	mlx5_flex_config_sample(&node, field, item->tunnel_mode);
	for (i = 0; i < parser->num_samples; i++) {
		struct mlx5_devx_match_sample_attr *sample =
			&parser->devx_conf.sample[i];
		int32_t reg_start, reg_end;
		int32_t cov_start, cov_end;

		MLX5_ASSERT(sample->flow_match_sample_en);
		if (!sample->flow_match_sample_en)
			break;
		node.flow_match_sample_field_base_offset =
			sample->flow_match_sample_field_base_offset;
		if (memcmp(&node, sample, sizeof(node)))
			continue;
		reg_start = (int8_t)sample->flow_match_sample_field_base_offset;
		reg_start *= CHAR_BIT;
		reg_end = reg_start + 32;
		if (end <= reg_start || start >= reg_end)
			continue;
		cov_start = RTE_MAX(reg_start, start);
		cov_end = RTE_MIN(reg_end, end);
		MLX5_ASSERT(cov_end > cov_start);
		done_bits += cov_end - cov_start;
		if (item->mapnum >= MLX5_FLEX_ITEM_MAPPING_NUM)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "too many flex item pattern translations");
		trans = &item->map[item->mapnum];
		item->mapnum++;
		trans->reg_id = i;
		trans->shift = cov_start - reg_start;
		trans->width = cov_end - cov_start;
	}
	if (done_bits != field->field_size) {
		MLX5_ASSERT(false);
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "failed to map field to sample register");
	}
	return 0;
}

/* Allocate sample registers for the specified field type and interval array. */
static int
mlx5_flex_alloc_sample(struct mlx5_flex_field_cover *cover,
		       struct mlx5_flex_parser_devx *parser,
		       struct mlx5_flex_item *item,
		       struct rte_flow_item_flex_field *field,
		       struct mlx5_hca_flex_attr *attr,
		       struct rte_flow_error *error)
{
	struct mlx5_devx_match_sample_attr node;
	uint32_t idx = 0;

	mlx5_flex_config_sample(&node, field, item->tunnel_mode);
	while (idx < cover->num) {
		int32_t start, end;

		/*
		 * Sample base offsets are in bytes, should be aligned
		 * to 32-bit as required by firmware for samples.
		 */
		start = RTE_ALIGN_FLOOR(cover->start[idx],
					sizeof(uint32_t) * CHAR_BIT);
		node.flow_match_sample_field_base_offset =
						(start / CHAR_BIT) & 0xFF;
		/* Allocate sample register. */
		if (parser->num_samples >= MLX5_GRAPH_NODE_SAMPLE_NUM ||
		    parser->num_samples >= attr->max_num_sample ||
		    parser->num_samples >= attr->max_num_prog_sample)
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "no sample registers to handle all flex item fields");
		parser->devx_conf.sample[parser->num_samples] = node;
		parser->num_samples++;
		/* Remove or update covered intervals. */
		end = start + 32;
		while (idx < cover->num) {
			if (end >= cover->end[idx]) {
				idx++;
				continue;
			}
			if (end > cover->start[idx])
				cover->start[idx] = end;
			break;
		}
	}
	return 0;
}

static int
mlx5_flex_translate_sample(struct mlx5_hca_flex_attr *attr,
			   const struct rte_flow_item_flex_conf *conf,
			   struct mlx5_flex_parser_devx *parser,
			   struct mlx5_flex_item *item,
			   struct rte_flow_error *error)
{
	struct mlx5_flex_field_cover cover;
	uint32_t i, j;
	int ret;

	switch (conf->tunnel) {
	case FLEX_TUNNEL_MODE_SINGLE:
		/* Fallthrough */
	case FLEX_TUNNEL_MODE_OUTER:
		/* Fallthrough */
	case FLEX_TUNNEL_MODE_INNER:
		/* Fallthrough */
	case FLEX_TUNNEL_MODE_MULTI:
		/* Fallthrough */
	case FLEX_TUNNEL_MODE_TUNNEL:
		break;
	default:
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "unrecognized tunnel mode");
	}
	item->tunnel_mode = conf->tunnel;
	if (conf->nb_samples > MLX5_FLEX_ITEM_MAPPING_NUM)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "sample field number exceeds limit");
	/*
	 * The application can specify fields smaller or bigger than 32 bits
	 * covered with single sample register and it can specify field
	 * offsets in any order.
	 *
	 * Gather all similar fields together, build array of bit intervals
	 * in ascending order and try to cover with the smallest set of sample
	 * registers.
	 */
	memset(&cover, 0, sizeof(cover));
	for (i = 0; i < conf->nb_samples; i++) {
		struct rte_flow_item_flex_field *fl = conf->sample_data + i;

		/* Check whether field was covered in the previous iteration. */
		if (cover.mapped[i / CHAR_BIT] & (1u << (i % CHAR_BIT)))
			continue;
		if (fl->field_mode == FIELD_MODE_DUMMY)
			continue;
		/* Build an interval array for the field and similar ones */
		cover.num = 0;
		/* Add the first field to array unconditionally. */
		ret = mlx5_flex_cover_sample(&cover, fl, NULL, attr, error);
		if (ret < 0)
			return ret;
		MLX5_ASSERT(ret > 0);
		cover.mapped[i / CHAR_BIT] |= 1u << (i % CHAR_BIT);
		for (j = i + 1; j < conf->nb_samples; j++) {
			struct rte_flow_item_flex_field *ft;

			/* Add field to array if its type matches. */
			ft = conf->sample_data + j;
			ret = mlx5_flex_cover_sample(&cover, ft, fl,
						     attr, error);
			if (ret < 0)
				return ret;
			if (!ret)
				continue;
			cover.mapped[j / CHAR_BIT] |= 1u << (j % CHAR_BIT);
		}
		/* Allocate sample registers to cover array of intervals. */
		ret = mlx5_flex_alloc_sample(&cover, parser, item,
					     fl, attr, error);
		if (ret)
			return ret;
	}
	/* Build the item pattern translating data on flow creation. */
	item->mapnum = 0;
	memset(&item->map, 0, sizeof(item->map));
	for (i = 0; i < conf->nb_samples; i++) {
		struct rte_flow_item_flex_field *fl = conf->sample_data + i;

		ret = mlx5_flex_map_sample(fl, parser, item, error);
		if (ret) {
			MLX5_ASSERT(false);
			return ret;
		}
	}
	if (conf->tunnel == FLEX_TUNNEL_MODE_MULTI) {
		/*
		 * In FLEX_TUNNEL_MODE_MULTI tunnel mode PMD creates 2 sets
		 * of samples. The first set is for outer and the second set
		 * for inner flex flow item. Outer and inner samples differ
		 * only in tunnel_mode.
		 */
		if (parser->num_samples > MLX5_GRAPH_NODE_SAMPLE_NUM / 2)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "no sample registers for inner");
		rte_memcpy(parser->devx_conf.sample + parser->num_samples,
			   parser->devx_conf.sample,
			   parser->num_samples *
					sizeof(parser->devx_conf.sample[0]));
		for (i = 0; i < parser->num_samples; i++) {
			struct mlx5_devx_match_sample_attr *sm = i +
				parser->devx_conf.sample + parser->num_samples;

			sm->flow_match_sample_tunnel_mode =
						MLX5_GRAPH_SAMPLE_TUNNEL_INNER;
		}
		parser->num_samples *= 2;
	}
	return 0;
}

static int
mlx5_flex_arc_type(enum rte_flow_item_type type, int in)
{
	switch (type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		return  MLX5_GRAPH_ARC_NODE_MAC;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		return in ? MLX5_GRAPH_ARC_NODE_IP : MLX5_GRAPH_ARC_NODE_IPV4;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		return in ? MLX5_GRAPH_ARC_NODE_IP : MLX5_GRAPH_ARC_NODE_IPV6;
	case RTE_FLOW_ITEM_TYPE_UDP:
		return MLX5_GRAPH_ARC_NODE_UDP;
	case RTE_FLOW_ITEM_TYPE_TCP:
		return MLX5_GRAPH_ARC_NODE_TCP;
	case RTE_FLOW_ITEM_TYPE_MPLS:
		return MLX5_GRAPH_ARC_NODE_MPLS;
	case RTE_FLOW_ITEM_TYPE_GRE:
		return MLX5_GRAPH_ARC_NODE_GRE;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		return MLX5_GRAPH_ARC_NODE_GENEVE;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		return MLX5_GRAPH_ARC_NODE_VXLAN_GPE;
	default:
		return -EINVAL;
	}
}

static int
mlx5_flex_arc_in_eth(const struct rte_flow_item *item,
		     struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct rte_flow_item_eth eth = { .hdr.ether_type = RTE_BE16(0xFFFF) };

	if (memcmp(mask, &eth, sizeof(struct rte_flow_item_eth))) {
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "invalid eth item mask");
	}
	return rte_be_to_cpu_16(spec->hdr.ether_type);
}

static int
mlx5_flex_arc_in_udp(const struct rte_flow_item *item,
		     struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct rte_flow_item_udp udp = { .hdr.dst_port = RTE_BE16(0xFFFF) };

	if (memcmp(mask, &udp, sizeof(struct rte_flow_item_udp))) {
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "invalid eth item mask");
	}
	return rte_be_to_cpu_16(spec->hdr.dst_port);
}

static int
mlx5_flex_arc_in_ipv6(const struct rte_flow_item *item,
		      struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	struct rte_flow_item_ipv6 ip = { .hdr.proto = 0xff };

	if (memcmp(mask, &ip, sizeof(struct rte_flow_item_ipv6))) {
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "invalid ipv6 item mask, full mask is desired");
	}
	return spec->hdr.proto;
}

static int
mlx5_flex_translate_arc_in(struct mlx5_hca_flex_attr *attr,
			   const struct rte_flow_item_flex_conf *conf,
			   struct mlx5_flex_parser_devx *devx,
			   struct mlx5_flex_item *item,
			   struct rte_flow_error *error)
{
	struct mlx5_devx_graph_node_attr *node = &devx->devx_conf;
	uint32_t i;

	RTE_SET_USED(item);
	if (conf->nb_inputs > attr->max_num_arc_in)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "too many input links");
	for (i = 0; i < conf->nb_inputs; i++) {
		struct mlx5_devx_graph_arc_attr *arc = node->in + i;
		struct rte_flow_item_flex_link *link = conf->input_link + i;
		const struct rte_flow_item *rte_item = &link->item;
		int arc_type;
		int ret;

		if (!rte_item->spec || !rte_item->mask || rte_item->last)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "invalid flex item IN arc format");
		arc_type = mlx5_flex_arc_type(rte_item->type, true);
		if (arc_type < 0 || !(attr->node_in & RTE_BIT32(arc_type)))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported flex item IN arc type");
		arc->arc_parse_graph_node = arc_type;
		arc->start_inner_tunnel = 0;
		/*
		 * Configure arc IN condition value. The value location depends
		 * on protocol. Current FW version supports IP & UDP for IN
		 * arcs only, and locations for these protocols are defined.
		 * Add more protocols when available.
		 */
		switch (rte_item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flex_arc_in_eth(rte_item, error);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flex_arc_in_udp(rte_item, error);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flex_arc_in_ipv6(rte_item, error);
			break;
		default:
			MLX5_ASSERT(false);
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported flex item IN arc type");
		}
		if (ret < 0)
			return ret;
		arc->compare_condition_value = (uint16_t)ret;
	}
	return 0;
}

static int
mlx5_flex_translate_arc_out(struct mlx5_hca_flex_attr *attr,
			    const struct rte_flow_item_flex_conf *conf,
			    struct mlx5_flex_parser_devx *devx,
			    struct mlx5_flex_item *item,
			    struct rte_flow_error *error)
{
	struct mlx5_devx_graph_node_attr *node = &devx->devx_conf;
	bool is_tunnel = conf->tunnel == FLEX_TUNNEL_MODE_TUNNEL;
	uint32_t i;

	RTE_SET_USED(item);
	if (conf->nb_outputs > attr->max_num_arc_out)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			 "too many output links");
	for (i = 0; i < conf->nb_outputs; i++) {
		struct mlx5_devx_graph_arc_attr *arc = node->out + i;
		struct rte_flow_item_flex_link *link = conf->output_link + i;
		const struct rte_flow_item *rte_item = &link->item;
		int arc_type;

		if (rte_item->spec || rte_item->mask || rte_item->last)
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "flex node: invalid OUT arc format");
		arc_type = mlx5_flex_arc_type(rte_item->type, false);
		if (arc_type < 0 || !(attr->node_out & RTE_BIT32(arc_type)))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				 "unsupported flex item OUT arc type");
		arc->arc_parse_graph_node = arc_type;
		arc->start_inner_tunnel = !!is_tunnel;
		arc->compare_condition_value = link->next;
	}
	return 0;
}

/* Translate RTE flex item API configuration into flaex parser settings. */
static int
mlx5_flex_translate_conf(struct rte_eth_dev *dev,
			 const struct rte_flow_item_flex_conf *conf,
			 struct mlx5_flex_parser_devx *devx,
			 struct mlx5_flex_item *item,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_flex_attr *attr = &priv->sh->cdev->config.hca_attr.flex;
	int ret;

	ret = mlx5_flex_translate_length(attr, conf, devx, error);
	if (ret)
		return ret;
	ret = mlx5_flex_translate_next(attr, conf, devx, error);
	if (ret)
		return ret;
	ret = mlx5_flex_translate_sample(attr, conf, devx, item, error);
	if (ret)
		return ret;
	ret = mlx5_flex_translate_arc_in(attr, conf, devx, item, error);
	if (ret)
		return ret;
	ret = mlx5_flex_translate_arc_out(attr, conf, devx, item, error);
	if (ret)
		return ret;
	return 0;
}

/**
 * Create the flex item with specified configuration over the Ethernet device.
 *
 * @param dev
 *   Ethernet device to create flex item on.
 * @param[in] conf
 *   Flex item configuration.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   Non-NULL opaque pointer on success, NULL otherwise and rte_errno is set.
 */
struct rte_flow_item_flex_handle *
flow_dv_item_create(struct rte_eth_dev *dev,
		    const struct rte_flow_item_flex_conf *conf,
		    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_parser_devx devx_config = { .devx_obj = NULL };
	struct mlx5_flex_item *flex;
	struct mlx5_list_entry *ent;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	flex = mlx5_flex_alloc(priv);
	if (!flex) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "too many flex items created on the port");
		return NULL;
	}
	if (mlx5_flex_translate_conf(dev, conf, &devx_config, flex, error))
		goto error;
	ent = mlx5_list_register(priv->sh->flex_parsers_dv, &devx_config);
	if (!ent) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "flex item creation failure");
		goto error;
	}
	flex->devx_fp = container_of(ent, struct mlx5_flex_parser_devx, entry);
	/* Mark initialized flex item valid. */
	__atomic_fetch_add(&flex->refcnt, 1, __ATOMIC_RELEASE);
	return (struct rte_flow_item_flex_handle *)flex;

error:
	mlx5_flex_free(priv, flex);
	return NULL;
}

/**
 * Release the flex item on the specified Ethernet device.
 *
 * @param dev
 *   Ethernet device to destroy flex item on.
 * @param[in] handle
 *   Handle of the item existing on the specified device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
flow_dv_item_release(struct rte_eth_dev *dev,
		     const struct rte_flow_item_flex_handle *handle,
		     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flex_item *flex =
		(struct mlx5_flex_item *)(uintptr_t)handle;
	uint32_t old_refcnt = 1;
	int rc;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_spinlock_lock(&priv->flex_item_sl);
	if (mlx5_flex_index(priv, flex) < 0) {
		rte_spinlock_unlock(&priv->flex_item_sl);
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "invalid flex item handle value");
	}
	if (!__atomic_compare_exchange_n(&flex->refcnt, &old_refcnt, 0, 0,
					 __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		rte_spinlock_unlock(&priv->flex_item_sl);
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex item has flow references");
	}
	/* Flex item is marked as invalid, we can leave locked section. */
	rte_spinlock_unlock(&priv->flex_item_sl);
	MLX5_ASSERT(flex->devx_fp);
	rc = mlx5_list_unregister(priv->sh->flex_parsers_dv,
				  &flex->devx_fp->entry);
	flex->devx_fp = NULL;
	mlx5_flex_free(priv, flex);
	if (rc < 0)
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex item release failure");
	return 0;
}

/* DevX flex parser list callbacks. */
struct mlx5_list_entry *
mlx5_flex_parser_create_cb(void *list_ctx, void *ctx)
{
	struct mlx5_dev_ctx_shared *sh = list_ctx;
	struct mlx5_flex_parser_devx *fp, *conf = ctx;
	uint32_t i;
	uint8_t sample_info = sh->cdev->config.hca_attr.flex.query_match_sample_info;
	int ret;

	fp = mlx5_malloc(MLX5_MEM_ZERO,	sizeof(struct mlx5_flex_parser_devx),
			 0, SOCKET_ID_ANY);
	if (!fp)
		return NULL;
	/* Copy the requested configurations. */
	fp->num_samples = conf->num_samples;
	memcpy(&fp->devx_conf, &conf->devx_conf, sizeof(fp->devx_conf));
	/* Create DevX flex parser. */
	fp->devx_obj = mlx5_devx_cmd_create_flex_parser(sh->cdev->ctx,
							&fp->devx_conf);
	if (!fp->devx_obj)
		goto error;
	/* Query the firmware assigned sample ids. */
	ret = mlx5_devx_cmd_query_parse_samples(fp->devx_obj,
						fp->sample_ids,
						fp->num_samples,
						&fp->anchor_id);
	if (ret)
		goto error;
	/* Query sample information per ID. */
	for (i = 0; i < fp->num_samples && sample_info; i++) {
		ret = mlx5_devx_cmd_match_sample_info_query(sh->cdev->ctx, fp->sample_ids[i],
							    &fp->sample_info[i]);
		if (ret)
			goto error;
	}
	DRV_LOG(DEBUG, "DEVx flex parser %p created, samples num: %u",
		(const void *)fp, fp->num_samples);
	return &fp->entry;
error:
	if (fp->devx_obj)
		mlx5_devx_cmd_destroy((void *)(uintptr_t)fp->devx_obj);
	if (fp)
		mlx5_free(fp);
	return NULL;
}

int
mlx5_flex_parser_match_cb(void *list_ctx,
			  struct mlx5_list_entry *iter, void *ctx)
{
	struct mlx5_flex_parser_devx *fp =
		container_of(iter, struct mlx5_flex_parser_devx, entry);
	struct mlx5_flex_parser_devx *org =
		container_of(ctx, struct mlx5_flex_parser_devx, entry);

	RTE_SET_USED(list_ctx);
	return !iter || !ctx || memcmp(&fp->devx_conf,
				       &org->devx_conf,
				       sizeof(fp->devx_conf));
}

void
mlx5_flex_parser_remove_cb(void *list_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flex_parser_devx *fp =
		container_of(entry, struct mlx5_flex_parser_devx, entry);

	RTE_SET_USED(list_ctx);
	MLX5_ASSERT(fp->devx_obj);
	claim_zero(mlx5_devx_cmd_destroy(fp->devx_obj));
	DRV_LOG(DEBUG, "DEVx flex parser %p destroyed", (const void *)fp);
	mlx5_free(entry);
}

struct mlx5_list_entry *
mlx5_flex_parser_clone_cb(void *list_ctx,
			  struct mlx5_list_entry *entry, void *ctx)
{
	struct mlx5_flex_parser_devx *fp;

	RTE_SET_USED(list_ctx);
	RTE_SET_USED(entry);
	fp = mlx5_malloc(0, sizeof(struct mlx5_flex_parser_devx),
			 0, SOCKET_ID_ANY);
	if (!fp)
		return NULL;
	memcpy(fp, ctx, sizeof(struct mlx5_flex_parser_devx));
	return &fp->entry;
}

void
mlx5_flex_parser_clone_free_cb(void *list_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flex_parser_devx *fp =
		container_of(entry, struct mlx5_flex_parser_devx, entry);
	RTE_SET_USED(list_ctx);
	mlx5_free(fp);
}
