/********************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2023 Intel Corporation All Rights Reserved.

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
 *********************************************************************************/

#include <errno.h>

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/dma.h>
#include <drivers/intel_gna34.h>
#include <soc.h>
#include <adsp_interrupt.h>
#include <zephyr/sys_clock.h>
#include <adsp_power.h>
#include "gna_defs_fw.h"

#define DT_DRV_COMPAT intel_gna34

#include "gna_wa.h"
#include "gna_iface.h"
#include "regs_gna_descriptor.h"
#include "gna_drv_internal.h"
#include "gna_rqueue.h"
#include "gna_model_db.h"
#include "gna_request_db.h"

/* Driver's internal functions */
#if CONFIG_INTEL_GNA34_SHARED
#if CONFIG_MULTICORE && CONFIG_SMP
#define GNA_DEVICE_LOCK					\
	{						\
		shm_acquire_with_cs(&self->shm);	\
	}
#define GNA_DEVICE_UNLOCK			\
	{					\
		shm_release(&self->shm);	\
	}
#else
#define GNA_DEVICE_LOCK   ENTER_CRITICAL_SECTION(GNA)
#define GNA_DEVICE_UNLOCK LEAVE_CRITICAL_SECTION(GNA)
#endif /* CONFIG_MULTICORE && CONFIG_SMP */
#else
#define GNA_DEVICE_LOCK
#define GNA_DEVICE_UNLOCK
#endif /* CONFIG_INTEL_GNA34_SHARED */

/* Wrapper for system time function */
static inline uint64_t gna_device_get_sys_time(void)
{
	return sys_clock_tick_get();
}

#if defined(GNA_DRV_WA_POLLING) && (GNA_DRV_WA_POLLING == 1)
void gna_device_process_isr(struct device *dev);
#endif

ErrorCode gna_device_process_request(gna_device *self, gna_request_internal *request)
{

	uint32_t gna_base_addr = self->base_addr;

	GNA_DESC_MMU_DIS *current_gna_descriptor;

#if CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR
	current_gna_descriptor = &(self->run_gna_descriptor);
#else
	current_gna_descriptor = (GNA_DESC_MMU_DIS *)(request->run_ctx);
#endif /* CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR */

	/* copy CTX */
	uint8_t *dst = (uint8_t *)current_gna_descriptor;
	uint8_t *src = request->my_ctx;

	memcpy(dst, src, request->ctx_size);

#if CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR
	current_gna_descriptor->bits.bar1 = MBAR_VALUE_INPUT((uint32_t)(request->input));
	current_gna_descriptor->bits.bar2 = MBAR_VALUE_OUTPUT((uint32_t)(request->output));
	current_gna_descriptor->bits.bar3 = MBAR_VALUE_SCRATCH((uint32_t)(request->run_ctx));
	current_gna_descriptor->bits.bar4 = MBAR_VALUE_STATE((uint32_t)(request->xnn_state));
#if CONFIG_INTEL_GNA34_ACC_CTL
	current_gna_descriptor->bits.mlmt1 = (uint32_t)(request->input_size);
	current_gna_descriptor->bits.mlmt2 = (uint32_t)(request->output_size);
	current_gna_descriptor->bits.mlmt4 = (uint32_t)(request->xnn_state_size);
#endif /* CONFIG_INTEL_GNA34_ACC_CTL */
#else  /* 4 BARs */
	/* Setup BARs */

	/* Fixed BAR assignment
	 * BAR1 <- input
	 * BAR2 <- (state != NULL)? state : output
	 */

	current_gna_descriptor->bits.bar1 = (uint32_t)(request->input);

	if (request->xnn_state != NULL) {
		/* Assign state to BAR2 */
		current_gna_descriptor->bits.bar2 = (uint32_t)(request->xnn_state);

	} else {
		/* assign Output to BAR2 */
		current_gna_descriptor->bits.bar2 = (uint32_t)(request->output);
	}
#endif /* CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR */

#if CONFIG_MULTICORE && CONFIG_SMP
	uint32_t current_core_mask;
	uint32_t request_core_mask;

	if (self->current_core != request->core_id) {
		/* new core setup for request */
		current_core_mask = BIT(self->current_core);
		request_core_mask = BIT(request->core_id);

		/* clear current core */
		self->cores_ie_mask &= ~current_core_mask;
		ACE_DINT[self->current_core].ie[ACE_INTL_ML] = 0;

		/* setup new core */
		self->cores_ie_mask |= request_core_mask;
		self->current_core = request->core_id;
		ACE_DINT[self->current_core].ie[ACE_INTL_ML] = 1;
	}
#endif

	/* write-back all descriptor changes to RAM before execution */
	sys_cache_data_flush_and_invd_range(current_gna_descriptor, request->ctx_size);

	/* setup HW */
	adsphal_gna_set_desc_base(gna_base_addr, (uint32_t)(current_gna_descriptor));

#if !defined(GNA_DRV_WA_POLLING) || (GNA_DRV_WA_POLLING == 0)
	adsphal_gna_set_completion_int_on(gna_base_addr);
	adsphal_gna_set_error_int_on(gna_base_addr);
	adsphal_gna_set_non_posted_int_on(gna_base_addr);
#endif

#if CONFIGFW_ADSP_GNA_VERSION < CONFIGFW_ADSP_GNA_4_5_VERSION
	adsphal_gna_set_gnamode(gna_base_addr, GNAMODE_XNN);
#endif /* CONFIGFW_ADSP_GNA_VERSION < CONFIGFW_ADSP_GNA_4_5_VERSION */

#if defined(GNA_DRV_WA_LOAD_BARS) && (GNA_DRV_WA_LOAD_BARS == 1)
	/* Load BARs by FW */
	adsphal_gna_bar_fw_preload_on(gna_base_addr);
	adsphal_gna_set_mbar0(gna_base_addr, (current_gna_descriptor)->bits.bar0);
	adsphal_gna_set_mbar1(gna_base_addr, (current_gna_descriptor)->bits.bar1);
	adsphal_gna_set_mbar2(gna_base_addr, (current_gna_descriptor)->bits.bar2);
#if CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR
	adsphal_gna_set_mbar3(gna_base_addr, (current_gna_descriptor)->bits.bar3);
	adsphal_gna_set_mbar4(gna_base_addr, (current_gna_descriptor)->bits.bar4);
#endif /* CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR */
#if CONFIG_INTEL_GNA34_7BAR
	adsphal_gna_set_mbar5(gna_base_addr, (current_gna_descriptor)->bits.bar5);
#if CONFIG_INTEL_GNA34_ACC_CTL
	/* set memory limit registers */
	adsphal_gna_set_mlmt0(gna_base_addr, current_gna_descriptor->bits.mlmt0);
	adsphal_gna_set_mlmt1(gna_base_addr, current_gna_descriptor->bits.mlmt1);
	adsphal_gna_set_mlmt2(gna_base_addr, current_gna_descriptor->bits.mlmt2);
	adsphal_gna_set_mlmt3(gna_base_addr, current_gna_descriptor->bits.mlmt3);
	adsphal_gna_set_mlmt4(gna_base_addr, current_gna_descriptor->bits.mlmt4);
	adsphal_gna_set_mlmt5(gna_base_addr, current_gna_descriptor->bits.mlmt5);
#endif /* CONFIG_INTEL_GNA34_ACC_CTL */
#endif /* CONFIG_INTEL_GNA34_7BAR */
#else
	/* Use HW preload from GNA Descriptor */
	adsphal_gna_bar_fw_preload_off(gna_base_addr);
#endif

#if CONFIG_INTEL_GNA34_ACC_CTL
	/* Enable Memory Access Control */
	adsphal_gna_set_acc_ctl_on(gna_base_addr);
#endif /* CONFIG_INTEL_GNA34_ACC_CTL */

	adsphal_gna_set_interrupts_on(gna_base_addr);

	/* Update request status GNA_REQUEST_IN_PROGRESS */
	request->status = GNA_REQUEST_IN_PROGRESS;

	/* Update time-stamp info in the request - start of processing */
	request->request->start_timestmp = gna_device_get_sys_time();

#if CONFIG_INTEL_GNA34_HW_STATS
	/* Enable and configure HW statistic counters */
	adsphal_gna_enable_compute_stats(gna_base_addr, GNACOMP_STAT_TOTAL_STALL_CYCLES);
#endif

#if CONFIGFW_ADSP_GNA_VERSION < CONFIGFW_ADSP_GNA_4_5_VERSION
#if defined(GNA_DRV_WA_NMEMRFX) && (GNA_DRV_WA_NMEMRFX == 1)
	adsphal_gna_set_nmemrfx(gna_base_addr);
#endif
#endif /* CONFIGFW_ADSP_GNA_VERSION < CONFIGFW_ADSP_GNA_4_5_VERSION */

	/* start inference */
	adsphal_gna_start_acceleration(gna_base_addr);

#if defined(GNA_DRV_WA_POLLING) && (GNA_DRV_WA_POLLING == 1)
	GNASTS_REG status_reg;

	status_reg = (GNASTS_REG)adsphal_gna_get_gna_status(gna_base_addr);
	while (!(status_reg.bits.intr_status) && !(status_reg.bits.scr_completed)) {
		/* wait */
		status_reg = (GNASTS_REG)(adsphal_gna_get_gna_status(gna_base_addr));
	}
	gna_device_process_isr(self->gen_dev);
#endif /* polling */

	return ADSP_SUCCESS;
}

/* ISR routine */
void gna_device_process_isr(struct device *dev)
{
	ErrorCode ec;
	gna_request_internal *reqInternal;
	gna_request_internal *newReqInt;
	gna_request *request;
	gna_device *self = (gna_device *)dev->data;
	uint32_t gna_base_addr = self->base_addr;

	/* get request entry from queue */
	GNA_DEVICE_LOCK;
	ec = gna_rqueue_pop(self, &reqInternal);

	if (ec != ADSP_SUCCESS) {
		/* clear HW - abort and cancel all errors and return */
		adsphal_gna_clear_device(gna_base_addr);

		/* unexpected error - we need request for state update */
		GNA_DEVICE_UNLOCK;

		return;
	}

#if CONFIG_MULTICORE && CONFIG_SMP
	uint32_t current_core = arch_proc_id();

	if ((self->current_core != current_core) || (reqInternal->core_id != current_core)) {
		/* simple return - we lost current request */
		GNA_DEVICE_UNLOCK;
		return;
	}
#endif

	request = reqInternal->request;

	/* update time-stamp info - end of processing */
	request->stop_timestmp = gna_device_get_sys_time();

#if CONFIG_INTEL_GNA34_HW_STATS
	/* read GNA cycle statistics from HW */
	request->ptc_cycles = adsphal_gna_get_perf_total_cycles(gna_base_addr);
	request->psc_cycles = adsphal_gna_get_perf_stall_cycles(gna_base_addr);
#endif

	/* update status info - status and HW status */
	reqInternal->hw_status = adsphal_gna_get_gna_status(gna_base_addr);
	ec = adsphal_gna_check_status(&(reqInternal->hw_status));
	if (ec == ADSP_SUCCESS) {
		reqInternal->status = GNA_REQUEST_COMPLETED;
	} else {
		reqInternal->status = GNA_REQUEST_ERRED;
	}

	/* clear GNA HW device */
	adsphal_gna_clear_device(gna_base_addr);

	/* check for not queue empty and call gna_device_process_request()
	 * this function return request address but this request remains in the queue
	 */
	ec = gna_rqueue_get(self, &newReqInt);
	if (ec == ADSP_SUCCESS) {
		/* We have new entry in the queue so lets start new inference */
		ec = gna_device_process_request(self, newReqInt);

	} else {
		/* queue processing is done */
		self->queue_processing_active = false;

		/* for empty queue disable power */
		gna_device_power_off(self);
	}

	/* call callback function with processed request info. */
	reqInternal->callbackFn(dev, reqInternal->context, request->request_id, reqInternal->status,
				reqInternal->hw_status);

	/* return internal request to pool */
	gna_request_db_free(self, reqInternal);
	GNA_DEVICE_UNLOCK;
}

ErrorCode gna_device_power_on(gna_device *self)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	uint32_t gna_base_addr = self->base_addr;

	/* first prevent power gating on ML domain */
	ACE_PWRCTL->wpmlpg = 0x1; /* enable ML 0 */
	/* wait for HW response */
	while (ACE_PWRSTS->mlpgs != 0x1) {
		xmp_spin();
	}

	adsphal_ml_set_ownership(gna_base_addr, 0x3);

	/* disable clock gating */
	adsphal_ml_set_icgd(gna_base_addr, 1);
	adsphal_ml_set_dcgd(gna_base_addr, 1);

	adsphal_ml_set_power_on(gna_base_addr);

#if CONFIGFW_ADSP_GNA_VERSION < CONFIGFW_ADSP_GNA_4_5_VERSION
#if defined(GNA_DRV_WA_PMQID) && (GNA_DRV_WA_PMQID == 0)
	adsphal_gna_set_quiteidle_on(gna_base_addr);
#endif
#if defined(GNA_DRV_WA_DIS_ERCO) && (GNA_DRV_WA_DIS_ERCO == 1)
	adsphal_gna_clr_erco(gna_base_addr);
#endif
#endif /* CONFIGFW_ADSP_GNA_VERSION < CONFIGFW_ADSP_GNA_4_5_VERSION */
#if defined(GNA_DRV_WA_DCG) && (GNA_DRV_WA_DCG == 1)
	adsphal_gna_set_ovr_val(gna_base_addr, 0xffffffff);
#endif

	return ADSP_SUCCESS;
}

ErrorCode gna_device_power_off(gna_device *self)
{
	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	uint32_t gna_base_addr = self->base_addr;

	/* Turn OFF the GNA power domain */
	adsphal_ml_set_power_off(gna_base_addr);

	return ADSP_SUCCESS;
}

void gna_activate_request_queue(const gna_device *self)
{
	ErrorCode ec;
	gna_request_internal *newReqInt;

	/* check for not queue empty and call gna_device_process_request()
	 * this function return request address but this request remains in the queue
	 */
	ec = gna_rqueue_get((gna_device *)self, &newReqInt);
	if (ec == ADSP_SUCCESS) {
		gna_device_power_on((gna_device *)self);

		((gna_device *)self)->queue_processing_active = true;

		/* We have new entry in the queue so lets start new inference */
		ec = gna_device_process_request((gna_device *)self, newReqInt);
	}
}

/*
 * API functions
 */

ErrorCode gna_device_get_caps(const struct device *dev, gna_capabilities *caps)
{
	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL((caps != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL((self != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	*caps = self->capabilities;
	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

ErrorCode gna_device_restore(const struct device *dev)
{
	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_device *self = (gna_device *)dev->data;

	irq_enable(DT_INST_IRQN(0));
	ACE_DINT[self->current_core].ie[ACE_INTL_ML] = 1;

	return ADSP_SUCCESS;
}

int gna_device_init(const struct device *dev)
{
	ErrorCode ec = ADSP_SUCCESS;

	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);

	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL((self != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	/*
	 * TODO: find alignment check macro and us in this function
	 * RETURN_EC_ON_FAIL(IS_ALIGNED(self, GET_SIZE(DCACHE_LINE_ALIGNMENT)),
	 * ADSP_GNA_DRV_BUFF_INVALID_ALIGNMENT);
	 */

	/* save data provided by DT */
	uint32_t gna_base_addr = self->base_addr;
	int irq_no = self->irq_no;

	RETURN_EC_ON_FAIL((gna_base_addr != (uintptr_t)NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);

	/* clear GNA device instance */
	memset(self, 0, sizeof(gna_device));

	/* restore base address and irq number */
	self->base_addr = gna_base_addr;
	self->irq_no = irq_no;

#if CONFIG_INTEL_GNA34_SHARED
	/* Initialize shared memory */

	/* shmid_s *shm_to_init = &self->shm; */
	void *segptr = __offset_by(&self->shm, sizeof(self->shm));
	size_t segsz = sizeof(gna_device) - sizeof(self->shm);

	ec = shm_initialize(shm_to_init, segptr, segsz);
	HALT_ON_ERROR(ec);
#endif /* CONFIG_INTEL_GNA34_SHARED */

	/* Lock device */
	GNA_DEVICE_LOCK;

	/* Power ON */
	ec = gna_device_power_on(self);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	/* Get capabilities, check version */
	self->capabilities.version = adsphal_gna_get_version(gna_base_addr);
	if (self->capabilities.version != CONFIGFW_ADSP_GNA_VERSION) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->capabilities.version == CONFIGFW_ADSP_GNA_VERSION),
			  ADSP_GNA_HW_NOT_COMPATIBLE);

	self->capabilities.mmu_enabled = adsphal_gna_get_mmu_present(gna_base_addr);
	/* This version support only mmu disabled mode, change */
	RETURN_EC_ON_FAIL((self->capabilities.mmu_enabled == 0),
			  ADSP_GNA_HW_NOT_COMPATIBLE);

	self->capabilities.ae_supported = adsphal_gna_get_ae_support(gna_base_addr);
	self->int_buff_size = adsphal_gna_get_internal_buffer_size(gna_base_addr);
	self->ce_num = adsphal_gna_get_ce_num(gna_base_addr);
	self->ple_num = adsphal_gna_get_ple_num(gna_base_addr);
	self->afe_num = adsphal_gna_get_afe_num(gna_base_addr);

	/* register access done - power off */
	ec = gna_device_power_off(self);

	/* setup gna_device, lists, memory
	 * request queue, model directory, request directory
	 */

	/* Request Queue init */
	ec = gna_rqueue_init(self);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	self->queue_processing_active = false;

	/* Model List setup - we need this list for UUID search */
	ec = gna_model_db_init(self);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	/* Request List setup - should be provided by user? */
	ec = gna_request_db_init(self);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	self->current_core = arch_proc_id();
	uint32_t current_core_mask = BIT(self->current_core);
#if !defined(GNA_DRV_WA_POLLING) || (GNA_DRV_WA_POLLING == 0)
	/* register and enable ML interrupt */
	IRQ_CONNECT(DT_INST_IRQN(0), IRQ_DEFAULT_PRIORITY, gna_device_process_isr,
		    DEVICE_DT_INST_GET(0), 0);
	irq_enable(DT_INST_IRQN(0));
	ACE_DINT[self->current_core].ie[ACE_INTL_ML] = 1;
#endif

	self->cores_ie_mask = current_core_mask;
	self->registered_cores_mask = current_core_mask;

	/* mark gna_device as initialized and set it global for the system */
	self->init_done = 1;

#if defined(GNA_DRV_WA_POLLING) && (GNA_DRV_WA_POLLING == 1)
	self->gen_dev = dev; /* generic device pointer */
#endif

	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

ErrorCode gna_init_model(const struct device *dev, gna_model_id **model_id, void *model_ctx,
			 void *gna_buffer, GNA_MODEL_UUID model_global_id)
{
	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model_id != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model_ctx != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(gna_buffer != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	/* lock device before access */
	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	ErrorCode ec;
	gna_model_internal *myModel = NULL;

	/* check for existing model with this UUID */
	ec = gna_model_db_search_uuid(self, &myModel, model_global_id);
	if ((ec == ADSP_SUCCESS) || (myModel != NULL)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL(ec != ADSP_SUCCESS, ADSP_GNA_MODEL_EXISTS); /* model exits */
	RETURN_EC_ON_FAIL(myModel == NULL, ADSP_GNA_MODEL_EXISTS);    /* model exits */

	/* allocate model id and model internal structure */

	ec = gna_model_db_alloc(self, &myModel);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	*model_id = &(myModel->model_id);
	myModel->model_id.model_global_id = model_global_id;
	myModel->model_id.gna_ctx_ptr = gna_buffer;
	myModel->model_id.model_ctx_ptr = model_ctx;

	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

ErrorCode gna_setup_model(const struct device *dev, gna_model_id *model_id,
			  uint32_t model_ldt_num_entries, uint32_t model_ro_size, void *model)
{
	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model_id != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL((model_ldt_num_entries > 0) &&
				  (model_ldt_num_entries <= GNA_MODEL_MAX_LACNT),
			  ADSP_GNA_MODEL_INVALID_LAYER_COUNT);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	ErrorCode ec;
	GNA_DESC_MMU_DIS *gna_desc;

	/* Check if model_id is already initialized */
	gna_model_id *model_id_holder;

	GNA_DEVICE_UNLOCK;
	ec = gna_get_model(dev, &model_id_holder, model_id->model_global_id);
	RETURN_ON_ERROR(ec);
	RETURN_EC_ON_FAIL(model_id_holder != NULL, ADSP_GNA_MODEL_NOT_FOUND);

	GNA_DEVICE_LOCK;
	/* Model id is the first structure of model internal so cast this to internal structure */
	gna_model_internal *myModel = (gna_model_internal *)model_id;

	ec = gna_model_db_setup(self, myModel);
	myModel->model_id.model_ro_ptr = model;
	myModel->model_id.model_ro_size = model_ro_size;
	myModel->model_id.model_ldt_num_entries = model_ldt_num_entries;

	/* Setup GNA descriptor */
	gna_desc = (GNA_DESC_MMU_DIS *)myModel->model_id.model_ctx_ptr;

	/* Initialize descriptor to zero */
	memset(gna_desc, 0, sizeof(GNA_DESC_MMU_DIS));

	/* Flush model RO data from D-cache */
	if (model_ro_size > 0)
		sys_cache_data_flush_and_invd_range(model, model_ro_size);

#if !CONFIG_INTEL_GNA34_7BAR
	gna_desc->bits.maxaddr =
		0xffffffC0; /* setup max address - in MMU_disablde mode
			     * this protection is useless
			     */
	gna_desc->bits.labase = 0x01;                 /* setup BAR0 offset 0 */
#else
	/* Flush LDT data from D-cache */
	if (model_id->model_ldt_size > 0)
		sys_cache_data_flush_and_invd_range(model_id->model_ldt_ptr,
						   model_id->model_ldt_size);

	gna_desc->bits.labase = 0x06;                 /* setup BAR5 (index 6) offset 0 */
	gna_desc->bits.bar5 = MBAR_VALUE_LDT((uint32_t)model_id->model_ldt_ptr);
#endif
	gna_desc->bits.lacnt = model_ldt_num_entries; /* default - whole model execution */
	gna_desc->bits.bar0 = MBAR_VALUE_RO((uint32_t)model);
#if CONFIG_INTEL_GNA34_6BAR || CONFIG_INTEL_GNA34_7BAR
	/* setup scratch BAR - BAR3 */
	gna_desc->bits.bar3 = MBAR_VALUE_SCRATCH((uint32_t)myModel->model_id.gna_ctx_ptr);
#endif
#if CONFIG_INTEL_GNA34_ACC_CTL
	gna_desc->bits.mlmt0 = model_ro_size;
	gna_desc->bits.mlmt5 = model_id->model_ldt_size;
	gna_desc->bits.mlmt3 = model_id->model_scratch_size;
#endif

	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

ErrorCode gna_destroy_model(const struct device *dev, const gna_model_id *model_id)
{
	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model_id != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	gna_model_internal *myModel = (gna_model_internal *)model_id;
	ErrorCode ec = gna_model_db_free(self, myModel);

	GNA_DEVICE_UNLOCK;
	return ec;
}

ErrorCode gna_get_model(const struct device *dev, gna_model_id **model_id,
			GNA_MODEL_UUID model_global_id)
{
	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model_id != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	ErrorCode ec;
	gna_model_internal *myModel;

	ec = gna_model_db_search_uuid(self, &myModel, model_global_id);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	*model_id = &(myModel->model_id);
	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

ErrorCode gna_init_request(const struct device *dev, gna_request *request,
			   const gna_model_id *model_id, void *state, uint32_t model_state_size,
			   uint32_t layer_offset, uint32_t layer_count)
{
	ErrorCode ec;
	gna_model_internal *model_int;
	gna_request_internal *request_int;

	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(request != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(model_id != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	/* search for model - can't init request without model */
	ec = gna_model_db_search(self, &model_int, model_id);
	if ((ec != ADSP_SUCCESS) || (model_int == NULL)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);
	RETURN_EC_ON_FAIL(model_int != NULL, ADSP_GNA_ERROR);

	/* allocate new internal request structure - if exist return */
	ec = gna_request_db_alloc(self, &request_int);
	if ((ec != ADSP_SUCCESS) || (request_int == NULL)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);
	RETURN_EC_ON_FAIL(request_int != NULL, ADSP_GNA_ERROR);

	/* Init internal request and assign request */
	ec = gna_request_db_setup(self, request_int, request);
	if (ec != ADSP_SUCCESS) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);

	/* Setup model related fields in the internal request */
	request_int->run_ctx = model_int->model_id.gna_ctx_ptr;
	request_int->my_ctx = model_int->model_id.model_ctx_ptr;
	request_int->ctx_size =
		GNA_DESC_SIZE;
	request_int->xnn_state = state;
	request_int->xnn_state_size = model_state_size;

	GNA_DEVICE_UNLOCK;

	/* setup fields in user request */
	request->submit_timestmp = 0;
	request->start_timestmp = 0;
	request->stop_timestmp = 0;
	request->state = state;
	request->model_state_size = model_state_size;
#if CONFIG_INTEL_GNA34_HW_STATS
	request->ptc_cycles = 0;
	request->psc_cycles = 0;
#endif
	request->model_id = (gna_model_id *)model_id;

	return ADSP_SUCCESS;
}

ErrorCode gna_request_enqueue(const struct device *dev, const gna_request *request,
			      const void *input, uint32_t input_size, void *output,
			      uint32_t output_size, pfn_gna_request_done request_callback,
			      void *request_ctx)
{
	ErrorCode ec;
	gna_request_internal *request_int;

	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(request != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(request_callback != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	/* At least one data pointer (input, output, state) should be valid (!=NULL) */
	RETURN_EC_ON_FAIL(((input != NULL) || (output != NULL) || (request->state != NULL)),
			  ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	/* find request internal structure */
	ec = gna_request_db_search(self, &request_int, request);
	if ((ec != ADSP_SUCCESS) || (request_int == NULL)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);
	RETURN_EC_ON_FAIL(request_int != NULL, ADSP_GNA_REQUEST_NOT_EXISTS_ERROR);

	/* update time-stamp - enqueue time */
	request_int->request->submit_timestmp = gna_device_get_sys_time();

	/* setup input/output */
	request_int->input = input;
	request_int->input_size = input_size;
	request_int->output = output;
	request_int->output_size = output_size;

	/* store callback and context pointers */
	request_int->callbackFn = request_callback;
	request_int->context = request_ctx;

	request_int->core_id = arch_proc_id();

#if CONFIG_MULTICORE && CONFIG_SMP
	uint32_t request_core_mask = BIT(request_int->core_id);
	struct device *my_dev = (struct device *)dev;

	/* check if this core has registered isr */
	if (!(self->registered_cores_mask & request_core_mask)) {
		/* register handler from current core == request core */
		ACE_DINT[self->current_core].ie[ACE_INTL_ML] = 1;
		/* update mask */
		self->registered_cores_mask |= request_core_mask;
	}
#endif

	/* push request to processing queue */
	gna_rqueue_push((gna_device *)self, request_int);

	/* start HW if queue was empty */
	if (!(self->queue_processing_active)) {
		gna_activate_request_queue(self);
	}

	GNA_DEVICE_UNLOCK;

	return ADSP_SUCCESS;
}

ErrorCode gna_get_request_status(const struct device *dev, gna_request *request,
				 gna_request_status *status, uint32_t *hw_status)
{
	ErrorCode ec;
	gna_request_internal *req_internal;

	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(request != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(status != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(hw_status != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	/* search for request */
	ec = gna_request_db_search(self, &req_internal, request);
	if ((ec != ADSP_SUCCESS) || (req_internal == NULL)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);
	RETURN_EC_ON_FAIL(req_internal != NULL, ADSP_GNA_REQUEST_NOT_EXISTS_ERROR);

	/* Read status of the request */
	*status = req_internal->status;
	*hw_status = req_internal->hw_status;

	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

ErrorCode gna_abort_request(const struct device *dev, gna_request *request)
{
	ErrorCode ec;
	gna_request_internal *req_internal;

	RETURN_EC_ON_FAIL((dev != NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);
	gna_device *self = (gna_device *)dev->data;

	RETURN_EC_ON_FAIL(self != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);
	RETURN_EC_ON_FAIL(request != NULL, ADSP_ERROR_NULL_POINTER_AS_PARAM);

	uint32_t gna_base_addr = self->base_addr;

	RETURN_EC_ON_FAIL((gna_base_addr != (uintptr_t)NULL), ADSP_ERROR_NULL_POINTER_AS_PARAM);

	GNA_DEVICE_LOCK;
	if (!(self->init_done)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_EC_ON_FAIL((self->init_done), ADSP_GNA_DEV_NOT_INITIALIZED);

	/* search for request */
	ec = gna_request_db_search(self, &req_internal, request);
	if ((ec != ADSP_SUCCESS) || (req_internal == NULL)) {
		GNA_DEVICE_UNLOCK;
	}
	RETURN_ON_ERROR(ec);
	RETURN_EC_ON_FAIL(req_internal != NULL, ADSP_GNA_REQUEST_NOT_EXISTS_ERROR);

	/* check if on HW */
	if (req_internal->status == GNA_REQUEST_IN_PROGRESS) {
		/* This request is queued on HW - check current HW state, abort and clear if
		 * required
		 */
		if (adsphal_gna_get_exec_active(gna_base_addr)) {
			adsphal_gna_clear_device(gna_base_addr);
		}
	}

	/* remove from queue */
	ec = gna_rqueue_remove(self, req_internal);
	RETURN_ON_ERROR(ec);
	/* delete from request database */
	ec = gna_request_db_free(self, req_internal);
	RETURN_ON_ERROR(ec);

	/* check for not queue empty and call gna_device_process_request()
	 * this function return request address but this request remains in the queue
	 */
	ec = gna_rqueue_get(self, &req_internal);
	if (ec == ADSP_SUCCESS) {
		/* We have new entry in the queue so lets start new inference */
		ec = gna_device_process_request(self, req_internal);

	} else {
		/* queue processing is done */
		self->queue_processing_active = false;

		/* for empty queue disable power */
		gna_device_power_off(self);
	}

	GNA_DEVICE_UNLOCK;
	return ADSP_SUCCESS;
}

/*
 * Driver initialization structures used for DT registration.
 */

static const struct gna_driver_api intel_gna34_api_funcs = {
	.device_get_caps = gna_device_get_caps,
	.init_model = gna_init_model,
	.setup_model = gna_setup_model,
	.destroy_model = gna_destroy_model,
	.get_model = gna_get_model,
	.init_request = gna_init_request,
	.request_enqueue = gna_request_enqueue,
	.get_request_status = gna_get_request_status,
	.abort_request = gna_abort_request,
	.restore = gna_device_restore,
};

struct gna_driver_config {
	uint32_t instance_no;
};

#define GNA34_DEVICE_INIT(n)                                                                       \
	static struct gna_driver_config intel_gna34_driver_config_##n = {                          \
		.instance_no = n,                                                                  \
	};                                                                                         \
	static gna_device intel_gna34_driver_data_##n = {                                          \
		.base_addr = DT_INST_REG_ADDR_BY_IDX(n, 0),                                        \
		.irq_no = DT_INST_IRQN(n),                                                         \
	};                                                                                         \
												   \
	DEVICE_DT_INST_DEFINE(n, gna_device_init, NULL, &intel_gna34_driver_data_##n,              \
			      &intel_gna34_driver_config_##n, POST_KERNEL,                         \
			      CONFIG_INTEL_GNA34_INIT_PRIORITY, &intel_gna34_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(GNA34_DEVICE_INIT)
