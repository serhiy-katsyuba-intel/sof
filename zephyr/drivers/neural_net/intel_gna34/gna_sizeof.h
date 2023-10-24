/******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2023 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery of
 * the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing
 *
 *******************************************************************************/

#ifndef ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_SIZEOF_H_
#define ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_SIZEOF_H_

#include "gna_wa.h"

/*! -------------------- GNA sizeofs ----------------------*/
/*
 * sizeof gna_rqueue_t type
 */
#define SIZE_OF_GNA_RQUEUE_T 16

/*
 * sizeof gna_rqueue_elem_t type
 */
#define SIZE_OF_GNA_RQUEUE_ELEM_T 8

/*! The size of intel_gna_model_header. */
#define SIZE_OF_GNA_MODEL_HEADER 64
/*! -------------------------------------------------------*/

#define SIZE_OF_GNA_REQUEST_DB_T 12
#define SIZE_OF_GNA_MODEL_DB_T   12

#define SIZE_OF_GNA_REQUEST_DB_ELEM_T 64
#define SIZE_OF_GNA_MODEL_DB_ELEM_T   128 /* 72 + reserved */

/*! - GNA pools and databases sizes - number of elements -- */
#define GNA_RQUEUE_QUANTITY     CONFIG_INTEL_GNA34_MAX_PENDING_REQUESTS /* 16 */
#define GNA_REQUEST_DB_QUANTITY (GNA_RQUEUE_QUANTITY * 2)               /* 32 */
#define GNA_MODEL_DB_QUANTITY   CONFIG_INTEL_GNA34_MAX_MODELS           /* 10 */

/*! -------- GNA pools and databases sizes in bytes ------- */
#define GNA_RQUEUE_SIZE     (SIZE_OF_GNA_RQUEUE_ELEM_T * GNA_RQUEUE_QUANTITY)
#define GNA_REQUEST_DB_SIZE (SIZE_OF_GNA_REQUEST_DB_ELEM_T * GNA_REQUEST_DB_QUANTITY)
#define GNA_MODEL_DB_SIZE   (SIZE_OF_GNA_MODEL_DB_ELEM_T * GNA_MODEL_DB_QUANTITY)

#endif /* ZEPHYR_NEURAL_NET_INTEL_GNA34_GNA_SIZEOF_H_ */
