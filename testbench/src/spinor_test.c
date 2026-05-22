/*
 * Copyright (c) 2025 Macronix International Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mxmtd_config.h"
#ifdef CONF_SPINOR

#include <stdlib.h>
#include "platform_print.h"
#include "spinor_test.h"
#include "flash_test.h"
#include "mxmtd_err.h"
#include "mxmtd.h"

#ifdef CONF_ENABLE_ASP
static int spinor_asp_rnd_lock_test(mxmtd_t *mxmtd, uint32_t ntimes, int target)
{
	int ret, n = 0;
	int lock_mode;

	mxic_pr_inf("----ASP %s Random Lock/Unlock test,target: %lu times, <START>\r\n",
			(ASP_SPB == target) ? "SPB" : "DPB", ntimes);

	while (n < ntimes) {
		uint32_t addr, nbytes, r = rand();

		if (0 == (n & 63) || (ntimes - 1) == n) {
			mxic_pr_inf("ASP Random Test @ %d\r\n", n);
		}

		addr = rand() % mxmtd->spinor.id_info->size_chip;
		nbytes = (0 == r) ? 0 : (mxmtd->spinor.id_info->size_chip - addr) % r;

		ret = mxmtd->ops.asp_lock(mxmtd, addr, nbytes, target);
		if (MXST_SUCCESS != ret) {
			goto release_spinor_asp_rnd_lock;
		}

		ret = mxmtd->ops.asp_is_lock(mxmtd, addr, nbytes, &lock_mode);
		if (MXST_SUCCESS != ret) {
			goto release_spinor_asp_rnd_lock;
		}

		if ((ASP_SPB == target && ASP_SPB_LOCK != lock_mode) ||
				(ASP_DPB == target && ASP_DPB_LOCK != lock_mode)) {
			ret = MXST_ERR_LOCK;
			goto release_spinor_asp_rnd_lock;
		}

		ret = mxmtd->ops.asp_unlock(mxmtd, addr, nbytes, target);
		if (MXST_SUCCESS != ret) {
			goto release_spinor_asp_rnd_lock;
		}

		ret = mxmtd->ops.asp_is_lock(mxmtd, addr, nbytes, &lock_mode);
		if (MXST_SUCCESS != ret) {
			goto release_spinor_asp_rnd_lock;
		}

		if (ASP_UNLOCK != lock_mode) {
			ret = MXST_ERR_UNLOCK;
			goto release_spinor_asp_rnd_lock;
		}

		n++;
	}

release_spinor_asp_rnd_lock:
	mxic_pr_inf("----[%s] ASP %s Lock/Unlock test,target: %lu times\r\n",
			(MXST_SUCCESS == ret) ? "PASS" : "FAIL", (ASP_SPB == target) ? "SPB" : "DPB", n);

	return ret;
}

static int spinor_asp_unlock_chip_test(mxmtd_t *mxmtd)
{
	int ret;
	int lock_mode = ASP_MIX_LOCK;

	mxic_pr_inf("----ASP Chip Lock/Unlock test <START>\r\n");

	ret = mxmtd->ops.asp_unlock(mxmtd, 0, mxmtd->spinor.id_info->size_chip - 1, ASP_SPB);
	if (MXST_SUCCESS != ret) {
		goto release_spinor_asp_unlock_chip_test;
	}

	ret = mxmtd->ops.asp_unlock(mxmtd, 0, mxmtd->spinor.id_info->size_chip - 1, ASP_DPB);
	if (MXST_SUCCESS != ret) {
		goto release_spinor_asp_unlock_chip_test;
	}

	ret = mxmtd->ops.asp_is_lock(mxmtd, 0, mxmtd->spinor.id_info->size_chip - 1, &lock_mode);
	if (MXST_SUCCESS != ret) {
		goto release_spinor_asp_unlock_chip_test;
	}

	if (ASP_UNLOCK != lock_mode) {
		mxic_pr_err("Unlock all chip failed\r\n");
		ret = MXST_ERR_UNLOCK;
		goto release_spinor_asp_unlock_chip_test;
	}

release_spinor_asp_unlock_chip_test:
	mxic_pr_inf("----[%s] ASP Chip Lock/Unlock test\r\n", (MXST_SUCCESS == ret) ? "PASS" : "FAIL");

	return ret;
}

static int spinor_asp_partial_lock_test(mxmtd_t *mxmtd)
{
	int ret, lock_mode;
	uint32_t addr, nbytes_blk;

	mxic_pr_inf("----ASP Partial Lock/Unlock test <START>\r\n");

	nbytes_blk = mxmtd->spinor.id_info->size_block;

	/* Lock block 1 ~ 3 */
	addr = nbytes_blk;
	ret = mxmtd->ops.asp_lock(mxmtd, addr, nbytes_blk * 3, ASP_DPB);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	/* check block 1 ~ 3 */
	ret = mxmtd->ops.asp_is_lock(mxmtd, addr, nbytes_blk * 3, &lock_mode);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (ASP_DPB_LOCK != lock_mode) {
		mxic_pr_err("Lock block 0 ~ 2 failed\r\n");
		ret = MXST_ERR_LOCK;
		goto release_spinor_asp_partial_lock_test;
	}

	/* Unlock block 2 */
	addr = nbytes_blk * 2;
	ret = mxmtd->ops.asp_unlock(mxmtd, addr, nbytes_blk, ASP_DPB);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	/* check block 1 ~ 3 */
	addr = nbytes_blk;
	ret = mxmtd->ops.asp_is_lock(mxmtd, addr, nbytes_blk * 3, &lock_mode);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (ASP_PARTIAL_LOCK != lock_mode) {
		ret = MXST_ERR_LOCK;
		mxic_pr_err("Unlock block 1 failed\r\n");
		goto release_spinor_asp_partial_lock_test;
	}

	/* Unlock block 1 ~ 3 */
	addr = nbytes_blk;
	ret = mxmtd->ops.asp_unlock(mxmtd, addr, nbytes_blk * 3, ASP_DPB);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	/* check block 1 ~ 3 */
	ret = mxmtd->ops.asp_is_lock(mxmtd, addr, nbytes_blk * 3, &lock_mode);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (ASP_UNLOCK != lock_mode) {
		mxic_pr_err("Unlock block 0 ~ 2 failed\r\n");
		ret = MXST_ERR_UNLOCK;
		goto release_spinor_asp_partial_lock_test;
	}

release_spinor_asp_partial_lock_test:
	mxic_pr_inf("----[%s] ASP Partial Lock/Unlock test\r\n",
			(MXST_SUCCESS == ret) ? "PASS" : "FAIL");

	return ret;
}

static int spinor_asp_test(mxmtd_t *mxmtd, uint32_t ntimes)
{
	int ret;

	ret = mxmtd->ops.query_lock_mode(mxmtd);
	if (BP_MODE == ret) {
		mxic_pr_err("Now is BP mode, but it should be ASP mode\r\n");
		return MXST_SUCCESS;
	}

	ret = spinor_asp_unlock_chip_test(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_asp_partial_lock_test(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_asp_rnd_lock_test(mxmtd, ntimes, ASP_SPB);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_asp_rnd_lock_test(mxmtd, ntimes, ASP_DPB);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_asp_unlock_chip_test(mxmtd);
    if (MXST_SUCCESS != ret) {
        return ret;
    }

	return MXST_SUCCESS;
}
#endif

static int spinor_testbench(mxmtd_t *mxmtd)
{
	int ret = MXST_SUCCESS;
	uint32_t n, nblks_max, blk;

	/* number of blocks per chip */
	nblks_max = mxmtd->ops.get_chip_size(mxmtd) / mxmtd->ops.get_blk_size(mxmtd);

	mxic_pr_inf("1. Performance Test\r\n"
				"2. Random (address & Length) Test\r\n"
				"3. Whole Chip Erase/Program/Read Verification\r\n"
				"4. Torture Test\r\n"
				"5. Erase all blocks\r\n"
				"6. ASP lock\r\n"
				"0. Test the above items(1-3)\r\n");
	switch (check_keyin(0, 6)) {
	case 1:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		ret = flash_performance_test(mxmtd, blk);
		break;
	case 2:
		mxic_pr_inf("Please enter times:\r\n");
		n  = check_keyin(1, UINT32_MAX);
		ret = flash_random_test(mxmtd, n);
		break;
	case 3:
		ret = flash_epr_verify_test(mxmtd);
		break;
	case 4:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(1, nblks_max - 1);
		mxic_pr_inf("Please enter times (0 for infinite loop):\r\n");
		n  = check_keyin(0, UINT32_MAX);
		ret = flash_torture_test(mxmtd, blk, n);
		break;
	case 5:
		mxic_pr_inf("Please re-enter '5' to ensure the start of the entire chip erase procedure\r\n");
		check_keyin(5, 5);
		ret = flash_erase_all(mxmtd);
		break;
	case 6:
		if (0 == mxmtd->spinor.wpsel) {
			mxic_pr_war("WPSEL in security register may be OTP/irreversible\r\n");
			mxic_pr_inf("Please enter 1 to write WPSEL in security register:\r\n");
			n  = check_keyin(0, 1);
			if (1 == n) {
				ret = mxmtd->ops.asp_set_wpsel(mxmtd);
				if (MXST_SUCCESS != ret) {
					return ret;
				}
			}
		}
		mxic_pr_inf("Please enter times:\r\n");
		n  = check_keyin(1, UINT32_MAX);
		ret = spinor_asp_test(mxmtd, n);
		break;
	case 0:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		mxic_pr_inf("Please enter times for Random Test:\r\n");
		n  = check_keyin(1, UINT32_MAX);

		flash_performance_test(mxmtd, blk);
		flash_random_test(mxmtd, n);
		flash_epr_verify_test(mxmtd);
		break;
	default:
		mxic_pr_inf("Do nothing!\r\n");
		break;
	}

	return ret;
}

int spinor_test()
{
	int ret;
	mxmtd_t *mxmtd = 0;

	ret = mxmtd_setup_dev(&mxmtd, FLASH_SPINOR, CH_A, 0);
	if (MXST_SUCCESS == ret) {
		flash_dump_info(mxmtd);
		ret = spinor_testbench(mxmtd);
	}
	mxmtd_release(&mxmtd);

	return ret;
}

#endif /* #ifdef CONF_SPINOR */
