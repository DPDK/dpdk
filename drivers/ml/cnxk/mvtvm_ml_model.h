/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _MVTVM_ML_MODEL_H_
#define _MVTVM_ML_MODEL_H_

#include <dlpack/dlpack.h>
#include <jansson.h>
#include <rte_common.h>

#if defined(RTE_CC_GCC) || defined(RTE_CC_CLANG)
__rte_diagnostic_push
RTE_PRAGMA(GCC diagnostic ignored "-Wstrict-prototypes")
#include <tvm/runtime/c_runtime_api.h>
__rte_diagnostic_pop
#else
#include <tvm/runtime/c_runtime_api.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include <rte_mldev.h>

#include "cnxk_ml_io.h"

struct cnxk_ml_dev;
struct cnxk_ml_model;
struct cnxk_ml_layer;

/* Maximum number of objects per model */
#define ML_MVTVM_MODEL_OBJECT_MAX 3

/* Magic number for TVM parameter blobs. */
#define TVM_NDARRAY_LIST_MAGIC 0xF7E58D4F05049CB7ULL

/* TVM parameter names structure */
struct mvtvm_ml_param_names {
	char **name;
	size_t count;
};

/* TVM object / artifact info structure */
struct mvtvm_ml_model_object {
	/* Name */
	char name[RTE_ML_STR_MAX];

	/* Buffer address */
	uint8_t *buffer;

	/* Buffer size */
	int64_t size;

	/* Offset */
	uint32_t offset;
};

/* Glow model callback functions */
typedef int (*tvmrt_glow_layer_load_cb)(void *device, uint16_t model_id, const char *layer_name,
					uint8_t *buffer, size_t size, uint16_t *index);
typedef int (*tvmrt_glow_layer_unload_cb)(void *device, uint16_t model_id, const char *layer_name);
typedef int (*tvmrt_io_alloc_cb)(void *device, uint16_t model_id, const char *layer_name,
				 uint64_t **input_qbuffer, uint64_t **output_qbuffer);
typedef int (*tvmrt_io_free_cb)(void *device, uint16_t model_id, const char *layer_name);
typedef int (*tvmrt_malloc_cb)(const char *name, size_t size, uint32_t align, void **addr);
typedef int (*tvmrt_free_cb)(const char *name);
typedef int (*tvmrt_quantize_cb)(void *device, uint16_t model_id, const char *layer_name,
				 const DLTensor **deq_tensor, void *qbuffer);
typedef int (*tvmrt_dequantize_cb)(void *device, uint16_t model_id, const char *layer_name,
				   void *qbuffer, const DLTensor **deq_tensor);
typedef int (*tvmrt_inference_cb)(void *device, uint16_t index, void *input, void *output,
				  uint16_t nb_batches);

struct tvmrt_glow_callback {
	tvmrt_glow_layer_load_cb tvmrt_glow_layer_load;
	tvmrt_glow_layer_unload_cb tvmrt_glow_layer_unload;
	tvmrt_io_alloc_cb tvmrt_io_alloc;
	tvmrt_io_free_cb tvmrt_io_free;
	tvmrt_malloc_cb tvmrt_malloc;
	tvmrt_free_cb tvmrt_free;
	tvmrt_quantize_cb tvmrt_quantize;
	tvmrt_dequantize_cb tvmrt_dequantize;
	tvmrt_inference_cb tvmrt_inference;
};

/* Model fast-path stats */
struct mvtvm_ml_model_xstats {
	/* Total TVM runtime latency, sum of all inferences */
	uint64_t tvm_rt_latency_tot;

	/* TVM runtime latency */
	uint64_t tvm_rt_latency;

	/* Minimum TVM runtime latency */
	uint64_t tvm_rt_latency_min;

	/* Maximum TVM runtime latency */
	uint64_t tvm_rt_latency_max;

	/* Total jobs dequeued */
	uint64_t dequeued_count;

	/* Hardware stats reset index */
	uint64_t tvm_rt_reset_count;
};

struct mvtvm_ml_model_data {
	/* Model objects */
	struct mvtvm_ml_model_object so;
	struct mvtvm_ml_model_object json;
	struct mvtvm_ml_model_object params;

	/* TVM runtime callbacks */
	struct tvmrt_glow_callback cb;

	/* TVM Graph Module */
	TVMModuleHandle graph_module;

	/* Shared object memfd used to load the TVM module */
	int fd;

	/* TVM Function Handles */
	TVMFunctionHandle load_params;
	TVMFunctionHandle set_input_zero_copy;
	TVMFunctionHandle set_output_zero_copy;
	TVMFunctionHandle run;

	/* Model I/O info */
	struct cnxk_ml_io_info info;

	/* Stats for burst ops */
	struct mvtvm_ml_model_xstats *burst_xstats;

	/* Input Tensor */
	DLTensor input_tensor[ML_CNXK_MODEL_MAX_INPUT_OUTPUT];

	/* Output Tensor */
	DLTensor output_tensor[ML_CNXK_MODEL_MAX_INPUT_OUTPUT];
};

enum cnxk_ml_model_type mvtvm_ml_model_type_get(struct rte_ml_model_params *params);
int mvtvm_ml_model_blob_parse(struct rte_ml_model_params *params,
			      struct mvtvm_ml_model_object *object);
int mvtvm_ml_model_get_layer_id(struct cnxk_ml_model *model, const char *layer_name,
				uint16_t *layer_id);
void mvtvm_ml_model_io_info_set(struct cnxk_ml_model *model);
struct cnxk_ml_io_info *mvtvm_ml_model_io_info_get(struct cnxk_ml_model *model, uint16_t layer_id);
void mvtvm_ml_model_info_set(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model);
void mvtvm_ml_layer_print(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_layer *layer, FILE *fp);
int mvtvm_ml_model_json_parse(struct cnxk_ml_model *model);

#endif /* _MVTVM_ML_MODEL_H_ */
