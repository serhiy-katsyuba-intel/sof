/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
 * Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef TESTER_GNA_TEST
#define TESTER_GNA_TEST

#include <stddef.h>
#include <stdint.h>
#include <sof/lib/inference_service.h>
#include "tester.h"

#define GNA_BUILTIN_MODELS_COUNT	2
#define GNA_MODEL_CTX_MEM_MAX_SIZE	0x400
#define GNA_REQUEST_CTX_MEM_MAX_SIZE	0x3000
#define GNA_MODEL_MEMORY_ALIGNMENT	64
#define GNA_REQUEST_POLL_INTERVAL_US	100
#define GNA_REQUEST_POLL_RETRIES	100

struct gna_builtin_data {
	const uint8_t *const model;	/**< Pointer to the GNA model */
	uint32_t model_size;		/**< Size of the GNA model in bytes */
	const uint8_t *in_buff;		/**< Pointer to the input buffer */
	uint32_t in_buff_size;		/**< Size of the input buffer in bytes */
	const uint8_t *ref_buff;	/**< Pointer to the output buffer */
	uint32_t ref_buff_size;		/**< Size of the output buffer in bytes */
};

struct gna_test_ipc_data {
	struct tester_init_config tester_config;/**< base tester configuration */
	uint32_t test_mode;			/**< Test mode */
	uint32_t test_iterations;		/**< Number of test runs */
	uint32_t model_id;			/**< Model ID to test with */
};

enum gna_test_mode {
	async,	/**< Asynchronous mode */
	yield,	/**< Yield mode */
};

struct gna_test_data {
	struct gna_instance_data *gna;			/**< Pointer to the GNA instance data. */
	struct gna_model_ctx *model_ctx;		/**< Pointer to the GNA model. */
	struct gna_request_ctx *request_ctx;		/**< Pointer to the GNA request. */
	const struct gna_builtin_data *builtin_data;	/**< Pointer to the GNA builtin data. */
	uint32_t test_mode;				/**< Test mode to run the GNA test. */
	uint32_t iterations;				/**< Number of iterations to run */
	uint32_t gna_dev_instance;			/**< GNA device instance to run on. */
	struct inference_model gna_model;		/**< GNA model to test. */
};

extern const struct tester_test_case_interface tester_interface_gna_test;

#endif /* TESTER_GNA_TEST */
