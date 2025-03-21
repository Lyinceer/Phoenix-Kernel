// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-api-common.h"
#include "pablo-kunit-test.h"
#include "hardware/sfr/pablo-sfr-dlfe-v1_0.h"
#include "pmio.h"
#include "hardware/api/pablo-hw-api-dlfe.h"
#include "is-param.h"
#include "pablo-hw-api-common-ctrl.h"

#define CALL_HWAPI_OPS(op, args...)	test_ctx.ops->op(args)

#define SET_CR(R, value)				\
	do {						\
		u32 *addr = (u32 *)(test_ctx.base + R);	\
		*addr = value;				\
	} while (0)
#define GET_CR(R)			*(u32 *)(test_ctx.base + R)

static struct dlfe_test_ctx {
	void				*base;
	const struct dlfe_hw_ops	*ops;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct dlfe_param_set		p_set;
} test_ctx;

/* Define the testcases. */
static void pablo_hw_api_dlfe_hw_reset_kunit_test(struct kunit *test)
{
	SET_CR(DLFE_R_YUV_DLFECINFIFOCURR_ENABLE, 1);
	SET_CR(DLFE_R_YUV_DLFECINFIFOPREV_ENABLE, 1);
	SET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE, 1);

	CALL_HWAPI_OPS(reset, test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE), 0);
}

static void pablo_hw_api_dlfe_hw_init_kunit_test(struct kunit *test)
{
	CALL_HWAPI_OPS(init, test_ctx.pmio);

	/* s_otf */
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_CONFIG), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_CONFIG), 1);

	/* s_model_buf */
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BUFFER_INSTRUCTION_BASE_ADDR), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BUFFER_MODEL_BASE_ADDR), (1 << 30));
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BUFFER_INPUT_BASE_ADDR), (1 << 31));
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BUFFER_OUTPUT_BASE_ADDR), 0);

	/* s_cloader */
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_STAT_RDMACL_EN), 1);
}

static void pablo_hw_api_dlfe_hw_s_core_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct dlfe_param_set *p_set;
	u32 w, h, val;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	/* TC#1. DLFE Chain configuration. */
	w = __LINE__;
	h = __LINE__;
	p_set->curr_in.width = w;
	p_set->curr_in.height = h;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	val = (w << 16) | h;
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_SOURCE_IMAGE_DIMENSION), val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_DESTINATION_IMAGE_DIMENSION), val);
	KUNIT_EXPECT_NE(test, GET_CR(DLFE_R_H_BLANK), 0);

	/* TC#2. Set CRC seed with 0. */
	p_set->crc_seed = 0;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_STREAM_CRC), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_STREAM_CRC), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_STREAM_CRC), 0);

	/* TC#3. Set CRC seed with non-zero. */

	p_set->crc_seed = val = __LINE__;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_STREAM_CRC), (u8)val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_STREAM_CRC), (u8)val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_STREAM_CRC), (u8)val);
}

static void pablo_hw_api_dlfe_hw_s_path_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct dlfe_param_set *p_set;
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	u32 val;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	/* TC#1. Enable every path. */
	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));
	p_set->curr_in.cmd = 1;
	p_set->prev_in.cmd = 1;
	p_set->wgt_out.cmd = 1;

	CALL_HWAPI_OPS(s_path, pmio, p_set, &frame_cfg);

	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_in_en, 0x3);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_out_en, 0x1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BYPASS), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BYPASS_WEIGHT), 0);
	val = (1 << 8) | 1;
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_ENABLE_PATH), val);

	/* TC#2. Disable every path. */
	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));
	p_set->curr_in.cmd = 0;
	p_set->prev_in.cmd = 0;
	p_set->wgt_out.cmd = 0;

	CALL_HWAPI_OPS(s_path, pmio, p_set,  &frame_cfg);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_in_en, 0x0);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_out_en, 0x0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_ENABLE_PATH), 0);
}

static void pablo_hw_api_dlfe_hw_g_int_en_kunit_test(struct kunit *test)
{
	u32 int_en[PCC_INT_ID_NUM] = { 0 };

	CALL_HWAPI_OPS(g_int_en, int_en);

	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_0], INT0_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_1], INT1_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_CMDQ_INT], 0);
	KUNIT_EXPECT_EQ(test, int_en[PCC_COREX_INT], 0);
}

#define DLFE_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
	 BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                        \
	 BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                        \
	 BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
static void pablo_hw_api_dlfe_hw_g_int_grp_en_kunit_test(struct kunit *test)
{
	u32 int_grp_en;

	int_grp_en = CALL_HWAPI_OPS(g_int_grp_en);

	KUNIT_EXPECT_EQ(test, int_grp_en, DLFE_INT_GRP_EN_MASK);
}

static void pablo_hw_api_dlfe_hw_wait_idle_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int ret;

	pmio = test_ctx.pmio;

	/* TC#1. Timeout to wait idleness. */
	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	/* TC#2. Succeed to wait idleness. */
	SET_CR(DLFE_R_IDLENESS_STATUS, 1);

	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_dlfe_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_CR);
	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_DBG_STATE);
	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_MODE_NUM);
}

static void pablo_hw_api_dlfe_hw_is_occurred_kunit_test(struct kunit *test)
{
	bool occur;
	u32 status;
	ulong type;

	/* TC#1. No interrupt. */
	status = 0;
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	/* TC#2. Test each interrupt. */
	status = BIT_MASK(INTR0_DLFE_FRAME_START_INT);
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	type = BIT_MASK(INT_FRAME_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_COREX_END_INT_0);
	type = BIT_MASK(INT_COREX_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_SETTING_DONE_INT);
	type = BIT_MASK(INT_SETTING_DONE);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_CINFIFO_0_OVERFLOW_ERROR_INT);
	type = BIT_MASK(INT_ERR0);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR1_DLFE_VOTF_LOST_FLUSH_INT);
	type = BIT_MASK(INT_ERR1);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	/* TC#3. Test interrupt ovarlapping. */
	status = BIT_MASK(INTR0_DLFE_FRAME_START_INT);
	type = BIT_MASK(INTR0_DLFE_FRAME_START_INT) | BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	status = BIT_MASK(INTR0_DLFE_FRAME_START_INT) | BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);
}

static void pablo_hw_api_dlfe_hw_s_strgen_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_strgen, pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_CONFIG), (1 << 5));
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_CONFIG), (1 << 5));
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_IP_USE_CINFIFO_NEW_FRAME_IN), 0);
}

static struct kunit_case pablo_hw_api_dlfe_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_dlfe_hw_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_g_int_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_g_int_grp_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_s_strgen_kunit_test),
	{},
};

static struct pablo_mmio *pablo_hw_api_dlfe_pmio_init(void)
{
	struct pmio_config *pcfg;
	struct pablo_mmio *pmio;

	pcfg = &test_ctx.pmio_config;

	pcfg->name = "DLFE";
	pcfg->mmio_base = test_ctx.base;
	pcfg->cache_type = PMIO_CACHE_NONE;

	dlfe_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	if (pmio_field_bulk_alloc(pmio, &test_ctx.pmio_fields,
				pcfg->fields, pcfg->num_fields))
		goto err_field_bulk_alloc;

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
err_init:
	return NULL;
}

static void pablo_hw_api_dlfe_pmio_deinit(void)
{
	if (test_ctx.pmio_fields) {
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		test_ctx.pmio_fields = NULL;
	}

	if (test_ctx.pmio) {
		pmio_exit(test_ctx.pmio);
		test_ctx.pmio = NULL;
	}
}

static int pablo_hw_api_dlfe_kunit_test_init(struct kunit *test)
{
	memset(&test_ctx, 0, sizeof(struct dlfe_test_ctx));

	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.base);

	test_ctx.ops = dlfe_hw_g_ops();

	test_ctx.pmio = pablo_hw_api_dlfe_pmio_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.pmio);

	return 0;
}

static void pablo_hw_api_dlfe_kunit_test_exit(struct kunit *test)
{
	pablo_hw_api_dlfe_pmio_deinit();
	kunit_kfree(test, test_ctx.base);
}

struct kunit_suite pablo_hw_api_dlfe_kunit_test_suite = {
	.name = "pablo-hw-api-dlfe-kunit-test",
	.init = pablo_hw_api_dlfe_kunit_test_init,
	.exit = pablo_hw_api_dlfe_kunit_test_exit,
	.test_cases = pablo_hw_api_dlfe_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_dlfe_kunit_test_suite);

MODULE_LICENSE("GPL");
