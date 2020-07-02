/*
 *   Copyright(c) 2019-2020 Broadcom Limited.
 *   All rights reserved.
 */

#include "bitstring.h"
#include "hcapi_cfa_defs.h"
#include <errno.h>
#include "assert.h"

/* HCAPI CFA common PUT APIs */
int hcapi_cfa_put_field(uint64_t *data_buf,
			const struct hcapi_cfa_layout *layout,
			uint16_t field_id, uint64_t val)
{
	assert(layout);

	if (field_id > layout->array_sz)
		/* Invalid field_id */
		return -EINVAL;

	if (layout->is_msb_order)
		bs_put_msb(data_buf,
			   layout->field_array[field_id].bitpos,
			   layout->field_array[field_id].bitlen, val);
	else
		bs_put_lsb(data_buf,
			   layout->field_array[field_id].bitpos,
			   layout->field_array[field_id].bitlen, val);
	return 0;
}

int hcapi_cfa_put_fields(uint64_t *obj_data,
			 const struct hcapi_cfa_layout *layout,
			 struct hcapi_cfa_data_obj *field_tbl,
			 uint16_t field_tbl_sz)
{
	int i;
	uint16_t bitpos;
	uint8_t bitlen;
	uint16_t field_id;

	assert(layout);
	assert(field_tbl);

	if (layout->is_msb_order) {
		for (i = 0; i < field_tbl_sz; i++) {
			field_id = field_tbl[i].field_id;
			if (field_id > layout->array_sz)
				return -EINVAL;
			bitpos = layout->field_array[field_id].bitpos;
			bitlen = layout->field_array[field_id].bitlen;
			bs_put_msb(obj_data, bitpos, bitlen,
				   field_tbl[i].val);
		}
	} else {
		for (i = 0; i < field_tbl_sz; i++) {
			field_id = field_tbl[i].field_id;
			if (field_id > layout->array_sz)
				return -EINVAL;
			bitpos = layout->field_array[field_id].bitpos;
			bitlen = layout->field_array[field_id].bitlen;
			bs_put_lsb(obj_data, bitpos, bitlen,
				   field_tbl[i].val);
		}
	}
	return 0;
}

/* HCAPI CFA common GET APIs */
int hcapi_cfa_get_field(uint64_t *obj_data,
			const struct hcapi_cfa_layout *layout,
			uint16_t field_id,
			uint64_t *val)
{
	assert(layout);
	assert(val);

	if (field_id > layout->array_sz)
		/* Invalid field_id */
		return -EINVAL;

	if (layout->is_msb_order)
		*val = bs_get_msb(obj_data,
				  layout->field_array[field_id].bitpos,
				  layout->field_array[field_id].bitlen);
	else
		*val = bs_get_lsb(obj_data,
				  layout->field_array[field_id].bitpos,
				  layout->field_array[field_id].bitlen);
	return 0;
}
