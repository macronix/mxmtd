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
#ifdef CONF_RAWNAND

#include "platform_print.h"
#include "rawnand_test.h"
#include "flash_test.h"
#include "mxmtd_err.h"
#include "mxmtd.h"

static int rawnand_testbench(mxmtd_t *mxmtd)
{
	int ret = MXST_SUCCESS;
	uint32_t n, nblks_max, npages_max, blk, page, npages;

	/* number of blocks per chip */
	nblks_max = mxmtd->ops.get_chip_size(mxmtd) / mxmtd->ops.get_blk_size(mxmtd);
	/* number of pages per block */
	npages_max = mxmtd->ops.get_blk_size(mxmtd) / mxmtd->ops.get_page_size(mxmtd);

	mxic_pr_inf("1. NOP (Number Of Program) Test\r\n"
				"2. ECC Error Bit Test\r\n"
				"3. Bad Block Test\r\n"
				"4. Performance Test\r\n"
				"5. Random (address & Length) Test\r\n"
				"6. Whole Chip Erase/Program/Read Verification\r\n"
				"7. Torture Test\r\n"
				"8. Erase all blocks\r\n"
				"0. Test the above items(1-6)\r\n");
	switch (check_keyin(0, 8)) {
	case 1:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		mxic_pr_inf("Please enter page number:\r\n");
		page = check_keyin(0, npages_max - 1);
		mxic_pr_inf("Please enter the number of pages:\r\n");
		npages = check_keyin(1, npages_max);
		mxic_pr_inf("Block : %d, Page: %d, Num of Pages: %d\r\n", blk, page, npages);
		ret = flash_nand_nop_test(mxmtd, blk, page, npages);
		break;
	case 2:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		ret = flash_nand_error_bit_test(mxmtd, blk);
		break;
	case 3:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		ret = flash_nand_bb_test(mxmtd, blk);
		break;
	case 4:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		ret = flash_performance_test(mxmtd, blk);
		break;
	case 5:
		mxic_pr_inf("Please enter times:\r\n");
		n  = check_keyin(1, UINT32_MAX);
		ret = flash_random_test(mxmtd, n);
		break;
	case 6:
		ret = flash_epr_verify_test(mxmtd);
		break;
	case 7:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(1, nblks_max - 1);
		mxic_pr_inf("Please enter times (0 for infinite loop):\r\n");
		n  = check_keyin(0, UINT32_MAX);
		ret = flash_torture_test(mxmtd, blk, n);
		break;
	case 8:
		mxic_pr_inf("Please re-enter '8' to ensure the start of the entire chip erase procedure\r\n");
		check_keyin(7, 7);
		ret = flash_erase_all(mxmtd);
		break;
	case 0:
		mxic_pr_inf("Please enter block number:\r\n");
		blk = check_keyin(0, nblks_max - 1);
		mxic_pr_inf("Please enter page number:\r\n");
		page = check_keyin(0, npages_max - 1);
		mxic_pr_inf("Please enter the number of pages for NOP Test:\r\n");
		npages = check_keyin(1, npages_max);
		mxic_pr_inf("Please enter times for Random Test:\r\n");
		n  = check_keyin(1, UINT32_MAX);

		flash_nand_nop_test(mxmtd, blk, page, npages);
		flash_nand_error_bit_test(mxmtd, blk);
		flash_nand_bb_test(mxmtd, blk);
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

int rawnand_test()
{
	int ret;
	mxmtd_t *mxmtd = 0;

	ret = mxmtd_setup_dev(&mxmtd, FLASH_RAWNAND, CH_B, 0);
	if (MXST_SUCCESS == ret) {
		ret = rawnand_testbench(mxmtd);
	}
	mxmtd_release(&mxmtd);

	return ret;
}

#endif /* #ifdef CONF_RAWNAND */
