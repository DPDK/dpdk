/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>

#undef RTE_USE_LIBBSD
#include <stdbool.h>

#include <rte_string_fns.h>

#include "telemetry_data.h"

#define RTE_TEL_UINT_HEX_STR_BUF_LEN 64

int
rte_tel_data_start_array(struct rte_tel_data *d, enum rte_tel_value_type type)
{
	enum tel_container_types array_types[] = {
			[RTE_TEL_STRING_VAL] = TEL_ARRAY_STRING,
			[RTE_TEL_INT_VAL] = TEL_ARRAY_INT,
			[RTE_TEL_UINT_VAL] = TEL_ARRAY_U64,
			[RTE_TEL_CONTAINER] = TEL_ARRAY_CONTAINER,
	};
	d->type = array_types[type];
	d->data_len = 0;
	return 0;
}

int
rte_tel_data_start_dict(struct rte_tel_data *d)
{
	d->type = TEL_DICT;
	d->data_len = 0;
	return 0;
}

int
rte_tel_data_string(struct rte_tel_data *d, const char *str)
{
	d->type = TEL_STRING;
	d->data_len = strlcpy(d->data.str, str, sizeof(d->data.str));
	if (d->data_len >= RTE_TEL_MAX_SINGLE_STRING_LEN) {
		d->data_len = RTE_TEL_MAX_SINGLE_STRING_LEN - 1;
		return E2BIG; /* not necessarily and error, just truncation */
	}
	return 0;
}

int
rte_tel_data_add_array_string(struct rte_tel_data *d, const char *str)
{
	if (d->type != TEL_ARRAY_STRING)
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_ARRAY_ENTRIES)
		return -ENOSPC;
	const size_t bytes = strlcpy(d->data.array[d->data_len++].sval,
			str, RTE_TEL_MAX_STRING_LEN);
	return bytes < RTE_TEL_MAX_STRING_LEN ? 0 : E2BIG;
}

int
rte_tel_data_add_array_int(struct rte_tel_data *d, int x)
{
	if (d->type != TEL_ARRAY_INT)
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_ARRAY_ENTRIES)
		return -ENOSPC;
	d->data.array[d->data_len++].ival = x;
	return 0;
}

int
rte_tel_data_add_array_u64(struct rte_tel_data *d, uint64_t x)
{
	if (d->type != TEL_ARRAY_U64)
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_ARRAY_ENTRIES)
		return -ENOSPC;
	d->data.array[d->data_len++].u64val = x;
	return 0;
}

int
rte_tel_data_add_array_container(struct rte_tel_data *d,
		struct rte_tel_data *val, int keep)
{
	if (d->type != TEL_ARRAY_CONTAINER ||
			(val->type != TEL_ARRAY_U64
			&& val->type != TEL_ARRAY_INT
			&& val->type != TEL_ARRAY_STRING))
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_ARRAY_ENTRIES)
		return -ENOSPC;

	d->data.array[d->data_len].container.data = val;
	d->data.array[d->data_len++].container.keep = !!keep;
	return 0;
}

/* To suppress compiler warning about format string. */
#if defined(RTE_TOOLCHAIN_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#elif defined(RTE_TOOLCHAIN_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

static int
rte_tel_uint_to_hex_encoded_str(char *buf, size_t buf_len, uint64_t val,
				uint8_t display_bitwidth)
{
#define RTE_TEL_HEX_FORMAT_LEN 16

	uint8_t spec_hex_width = (display_bitwidth + 3) / 4;
	char format[RTE_TEL_HEX_FORMAT_LEN];

	if (display_bitwidth != 0) {
		if (snprintf(format, RTE_TEL_HEX_FORMAT_LEN, "0x%%0%u" PRIx64,
				spec_hex_width) >= RTE_TEL_HEX_FORMAT_LEN)
			return -EINVAL;

		if (snprintf(buf, buf_len, format, val) >= (int)buf_len)
			return -EINVAL;
	} else {
		if (snprintf(buf, buf_len, "0x%" PRIx64, val) >= (int)buf_len)
			return -EINVAL;
	}

	return 0;
}

#if defined(RTE_TOOLCHAIN_GCC)
#pragma GCC diagnostic pop
#elif defined(RTE_TOOLCHAIN_CLANG)
#pragma clang diagnostic pop
#endif

int
rte_tel_data_add_array_uint_hex(struct rte_tel_data *d, uint64_t val,
				uint8_t display_bitwidth)
{
	char hex_str[RTE_TEL_UINT_HEX_STR_BUF_LEN];
	int ret;

	ret = rte_tel_uint_to_hex_encoded_str(hex_str,
			RTE_TEL_UINT_HEX_STR_BUF_LEN, val, display_bitwidth);
	if (ret != 0)
		return ret;

	return rte_tel_data_add_array_string(d, hex_str);
}

static bool
valid_name(const char *name)
{
	char allowed[128] = {
			['0' ... '9'] = 1,
			['A' ... 'Z'] = 1,
			['a' ... 'z'] = 1,
			['_'] = 1,
			['/'] = 1,
	};
	while (*name != '\0') {
		if ((size_t)*name >= RTE_DIM(allowed) || allowed[(int)*name] == 0)
			return false;
		name++;
	}
	return true;
}

int
rte_tel_data_add_dict_string(struct rte_tel_data *d, const char *name,
		const char *val)
{
	struct tel_dict_entry *e = &d->data.dict[d->data_len];
	size_t nbytes, vbytes;

	if (d->type != TEL_DICT)
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_DICT_ENTRIES)
		return -ENOSPC;

	if (!valid_name(name))
		return -EINVAL;

	d->data_len++;
	e->type = RTE_TEL_STRING_VAL;
	vbytes = strlcpy(e->value.sval, val, RTE_TEL_MAX_STRING_LEN);
	nbytes = strlcpy(e->name, name, RTE_TEL_MAX_STRING_LEN);
	if (vbytes >= RTE_TEL_MAX_STRING_LEN ||
			nbytes >= RTE_TEL_MAX_STRING_LEN)
		return E2BIG;
	return 0;
}

int
rte_tel_data_add_dict_int(struct rte_tel_data *d, const char *name, int val)
{
	struct tel_dict_entry *e = &d->data.dict[d->data_len];
	if (d->type != TEL_DICT)
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_DICT_ENTRIES)
		return -ENOSPC;

	if (!valid_name(name))
		return -EINVAL;

	d->data_len++;
	e->type = RTE_TEL_INT_VAL;
	e->value.ival = val;
	const size_t bytes = strlcpy(e->name, name, RTE_TEL_MAX_STRING_LEN);
	return bytes < RTE_TEL_MAX_STRING_LEN ? 0 : E2BIG;
}

int
rte_tel_data_add_dict_u64(struct rte_tel_data *d,
		const char *name, uint64_t val)
{
	struct tel_dict_entry *e = &d->data.dict[d->data_len];
	if (d->type != TEL_DICT)
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_DICT_ENTRIES)
		return -ENOSPC;

	if (!valid_name(name))
		return -EINVAL;

	d->data_len++;
	e->type = RTE_TEL_UINT_VAL;
	e->value.u64val = val;
	const size_t bytes = strlcpy(e->name, name, RTE_TEL_MAX_STRING_LEN);
	return bytes < RTE_TEL_MAX_STRING_LEN ? 0 : E2BIG;
}

int
rte_tel_data_add_dict_container(struct rte_tel_data *d, const char *name,
		struct rte_tel_data *val, int keep)
{
	struct tel_dict_entry *e = &d->data.dict[d->data_len];

	if (d->type != TEL_DICT || (val->type != TEL_ARRAY_U64
			&& val->type != TEL_ARRAY_INT
			&& val->type != TEL_ARRAY_STRING
			&& val->type != TEL_DICT))
		return -EINVAL;
	if (d->data_len >= RTE_TEL_MAX_DICT_ENTRIES)
		return -ENOSPC;

	if (!valid_name(name))
		return -EINVAL;

	d->data_len++;
	e->type = RTE_TEL_CONTAINER;
	e->value.container.data = val;
	e->value.container.keep = !!keep;
	const size_t bytes = strlcpy(e->name, name, RTE_TEL_MAX_STRING_LEN);
	return bytes < RTE_TEL_MAX_STRING_LEN ? 0 : E2BIG;
}

int
rte_tel_data_add_dict_uint_hex(struct rte_tel_data *d, const char *name,
			       uint64_t val, uint8_t display_bitwidth)
{
	char hex_str[RTE_TEL_UINT_HEX_STR_BUF_LEN];
	int ret;

	ret = rte_tel_uint_to_hex_encoded_str(hex_str,
			RTE_TEL_UINT_HEX_STR_BUF_LEN, val, display_bitwidth);
	if (ret != 0)
		return ret;


	return rte_tel_data_add_dict_string(d, name, hex_str);
}

struct rte_tel_data *
rte_tel_data_alloc(void)
{
	return malloc(sizeof(struct rte_tel_data));
}

void
rte_tel_data_free(struct rte_tel_data *data)
{
	free(data);
}
