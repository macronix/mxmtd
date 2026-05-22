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

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <malloc.h>
#include "mxmtd.h"
#include "mxmtd_err.h"
#include "flash_test.h"
#include "platform_print.h"

#define BIT(x) (1U << (x))

/* for read and program */
typedef int (*op_t)(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes);

uint32_t check_keyin(uint32_t from, uint32_t to)
{
	uint32_t i;

	do {
		char c;

		if (1 == scanf("%lu", &i)) {
			if (from <= i && to >= i) {
				mxic_pr_inf("Enter: %lu\r\n", i);
				break;
			}
		}
		while ((c = getchar()) != '\r' && c != '\n' && c != EOF);
		mxic_pr_inf("Invalid input, please enter a valid integer\r\n");
	} while (1);

	return i;
}

static void elapsed_time(uint32_t size_data, struct timeval *start, struct timeval *stop, char *msg)
{
	uint32_t s, ms;

	if (start->tv_usec > stop->tv_usec) {
			stop->tv_usec += 1000000;
			stop->tv_sec -= 1;
	}
	ms = (uint32_t)(stop->tv_usec - start->tv_usec) / 1000U;
	s = (uint32_t)(stop->tv_sec - start->tv_sec);

	mxic_pr_inf(" %Xh\t| %d.%03d\t\t| %d\t(KiB/s)\t| %s\r\n",
			size_data, s, ms, ((size_data * 1000) / (s * 1000 + ms)) / 1024, msg);
}

static void pr_test_range(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes, char *s)
{
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t size_page = mxmtd->ops.get_page_size(mxmtd);
	uint32_t nblks = mxmtd->ops.get_chip_size(mxmtd) / size_blk;
	uint32_t npages = size_blk / size_page;

	mxic_pr_inf("<%s> Test range: "
			"%08X-%08Xh ~ "
			"%08X: %08Xh, "
			"Block: %d/%d, "
			"Page: %d/%d, "
			"nbytes: %08Xh\r\n",
			s,
			(uint32_t )(addr >> 32), (uint32_t)(addr),
			(uint32_t )((addr + nbytes - 1) >> 32), (uint32_t)(addr + nbytes - 1),
			(uint32_t)(addr >> fmsb32(size_blk)), nblks,
			(uint32_t)(addr >> fmsb32(size_page)) % npages, npages,
			(uint32_t)nbytes);
}

static int flash_nand_find_gb(mxmtd_t *mxmtd, uint64_t *addr)
{
	uint64_t size_chip = mxmtd->ops.get_chip_size(mxmtd);
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t addr_tmp = (*addr);

	if (0 == mxmtd->ops.nand.chk_bb) {
		return MXST_SUCCESS;
	}

	if ((addr_tmp + size_blk) > size_chip) {
		return MXST_ERR_PARAM;
	}

	while (mxmtd->ops.nand.chk_bb(mxmtd, addr_tmp)) {
		mxic_pr_war("Block %d is bad\r\n", addr_tmp >> fmsb32(size_blk));
		addr_tmp += size_blk;
		if (addr_tmp >= size_chip) {

			return MXST_ERR_NO_GB;
		}
	}
	(*addr) = addr_tmp;

	return MXST_SUCCESS;
}

static int flash_speed_ers(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes)
{
	int ret;
	struct timeval start, stop;

	gettimeofday(&start, NULL);
	ret = mxmtd->ops.erase(mxmtd, addr, nbytes);
	gettimeofday(&stop, NULL);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	elapsed_time(nbytes, &start, &stop, "Block Erase");

	return ret;
}

static int flash_speed(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes, uint32_t nbytes_a_time,
		uint8_t *buf, op_t op, char *s)
{
	int ret;
	struct timeval start, stop;
	uint32_t n;
	uint32_t nbytes_tmp = nbytes;

	if (0 == buf) {
		return MXST_ERR_PTR_NULL;
	}

	nbytes_tmp = nbytes;
	gettimeofday(&start, NULL);
	while (nbytes_tmp) {
		n = nbytes_tmp > nbytes_a_time ? nbytes_a_time : nbytes_tmp;
		ret = op(mxmtd, addr, buf, n);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		nbytes_tmp -= n;
		addr += n;
		buf += n;
	}
	gettimeofday(&stop, NULL);
	elapsed_time(nbytes, &start, &stop, s);

	return MXST_SUCCESS;
}

int flash_performance_test(mxmtd_t *mxmtd, uint32_t blk)
{
	int ret = MXST_SUCCESS;
	char str[100] = {};
	char *header = " Size\t| Elapsed\t| Throughput\t| Description\r\n";
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t size_page = mxmtd->ops.get_page_size(mxmtd);
	uint32_t n, npage_per_blk = size_blk / size_page;
	uint64_t addr = blk * size_blk;
	uint8_t *buf = malloc(size_blk);

	if (0 == buf) {
		return MXST_ERR_ALLOC;
	}

	ret = flash_nand_find_gb(mxmtd, &addr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	for (n = 0; n < size_blk; n++) {
		buf[n] = rand() % 0x100;
	}

	mxic_pr_inf("----Performance test, <START>\r\n");
	mxic_pr_inf("--Program speed test\r\n");
	mxic_pr_inf(header);
	for (n = 1; n <= npage_per_blk; n >= 16 ? n *= 2 : n++) {
		ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
		if (MXST_SUCCESS != ret) {
			goto release_flash_speed_test;
		}
		if (n >= 16 && (2 * n <= npage_per_blk)) {
			n *= 2;
		}
		sprintf(str, "Program a block, %2lu page at a time", n);
		ret = flash_speed(mxmtd, addr, size_blk, n * size_page, buf, mxmtd->ops.program, str);
		if (MXST_SUCCESS != ret) {
			goto release_flash_speed_test;
		}
	}

	mxic_pr_inf("--Read speed test\r\n");
	mxic_pr_inf(header);
	for (n = 1; n <= npage_per_blk; n >= 16 ? n *= 2 : n++) {
		sprintf(str, "Read    a block, %2lu page at a time", n);
		ret = flash_speed(mxmtd, addr, size_blk, n * size_page, buf, mxmtd->ops.read, str);
		if (MXST_SUCCESS != ret) {
			goto release_flash_speed_test;
		}
	}

	mxic_pr_inf("--Erase speed test\r\n");
	mxic_pr_inf(header);
	flash_speed_ers(mxmtd, addr, size_blk);
	mxic_pr_inf("--\r\n");

release_flash_speed_test:
mxic_pr_inf("----[%s] Performance test\r\n\r\n", MXST_SUCCESS == ret ? "PASS" : "FAIL");
	if (buf) {
		free(buf);
	}
	return ret;
}

static int flash_epr_verify_in_blk(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf_wr, uint8_t *buf_rd,
		uint64_t nbytes, uint8_t en_pgm)
{
	int ret;

	ret = mxmtd->ops.erase(mxmtd, addr & ~((uint64_t)mxmtd->ops.get_blk_size(mxmtd) - 1),
			mxmtd->ops.get_blk_size(mxmtd));
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (en_pgm) {
		ret = mxmtd->ops.program(mxmtd, addr, buf_wr, nbytes);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	ret = mxmtd->ops.read(mxmtd, addr, buf_rd, nbytes);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	if (-1 == mxmtd->ops.get_ecc_state(mxmtd)) {
		mxic_pr_err("The error bits can not be corrected\r\n");

	} else if (mxmtd->ops.get_ecc_state(mxmtd)) {
		mxic_pr_war("The max error bits(%d) in the ECC step have been corrected\r\n",
				mxmtd->ops.get_ecc_state(mxmtd));
	}

	if (memcmp(buf_rd, buf_wr, nbytes)) {
		uint32_t n;

		mxic_pr_err("The read data is not match expectations\r\n");

		for (n = 0; n < nbytes; n++) {
			if (buf_wr[n] != buf_rd[n]) {
				mxic_pr_inf("(%04d exp [%02X] ret [%02X] @ %02X, IOs: %s%s%s%s%s%s%s%s\r\n",
					n, buf_wr[n], buf_rd[n], buf_wr[n] ^ buf_rd[n],
					((buf_wr[n] ^ buf_rd[n]) & BIT(7)) ? "7" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(6)) ? "6" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(5)) ? "5" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(4)) ? "4" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(3)) ? "3" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(2)) ? "2" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(1)) ? "1" : " ",
					((buf_wr[n] ^ buf_rd[n]) & BIT(0)) ? "0" : " ");
			}
		}
		return MXST_ERR_CMP;
	}

	return MXST_SUCCESS;
}

int flash_epr_verify_test(mxmtd_t *mxmtd)
{
	int ret, n;
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t nblks = mxmtd->ops.get_chip_size(mxmtd) / size_blk;
	uint64_t addr;
	uint32_t nbytes = size_blk;
	uint8_t *buf_rd = malloc(nbytes);
	uint8_t *buf_wr = malloc(nbytes);

	mxic_pr_inf("----Whole chip (%d blocks) write/read test, <START>\r\n", nblks);

	if (0 == buf_rd || 0 == buf_wr) {
		mxic_pr_err("Allocate buffer failed, size(%08X)\r\n", nbytes);
		goto release_flash_epr_verify_test;
	}

	for (addr = 0; addr < mxmtd->ops.get_chip_size(mxmtd); addr += nbytes) {
		pr_test_range(mxmtd, addr, nbytes, "Flash Erase/Program/Read Verify Test");

		if (mxmtd->ops.nand.chk_bb && mxmtd->ops.nand.chk_bb(mxmtd, addr)) {
			mxic_pr_war("Bad block %d is found!\r\n", (uint32_t)addr >> fmsb32(size_blk));
			continue;
		}

		mxic_pr_inf("***<1> 0xFF pattern verify...\r\n");
		memset(buf_wr, 0xff, nbytes);
		ret = flash_epr_verify_in_blk(mxmtd, addr, buf_wr, buf_rd, nbytes, 0);
		if (MXST_SUCCESS != ret) {
			goto release_flash_epr_verify_test;
		}

		mxic_pr_inf("***<2> Sequential pattern verify...\r\n");
		for (n = 0; n < nbytes; n++) {
			buf_wr[n] = n % 0x100;
		}
		ret = flash_epr_verify_in_blk(mxmtd, addr, buf_wr, buf_rd, nbytes, 1);
		if (MXST_SUCCESS != ret) {
			goto release_flash_epr_verify_test;
		}

		mxic_pr_inf("***<3> Random pattern verify...\r\n");
		for (n = 0; n < nbytes; n++) {
			buf_wr[n] = rand() % 0x100;
		}
		ret = flash_epr_verify_in_blk(mxmtd, addr, buf_wr, buf_rd, nbytes, 1);
		if (MXST_SUCCESS != ret) {
			goto release_flash_epr_verify_test;
		}
	}

release_flash_epr_verify_test:
	mxic_pr_inf("----[%s] Whole chip read/write test\r\n\r\n",
			MXST_SUCCESS == ret ? "PASS" : "FAIL");

	if (buf_rd) {
		free(buf_rd);
	}
	if (buf_wr) {
		free(buf_wr);
	}

	return ret;
}

static void flash_nand_error_inject(mxmtd_t *mxmtd, uint8_t *buf_page, int bits_err)
{
	uint8_t nsteps = mxmtd->ops.nand.get_ecc_nsteps(mxmtd);
	uint16_t size_step = mxmtd->ops.nand.get_ecc_step_size(mxmtd);
	uint32_t m, n, i, j;
	uint32_t ofs;

	for (m = 0; m < nsteps; m++) {
		for (n = 0, j = 0; n < size_step; n++) {
			for (i = 0; i < 8; i++) {
				if (m == (nsteps - 1)) {
					/* The last setp is backward search */
					ofs = m * size_step + size_step - 1 - n;
				} else {
					/* The other setps are forward search */
					ofs = m * size_step + n;
				}
				if (buf_page[ofs] & (1U << i)) {
					j++;
					buf_page[ofs] &= ~(1U << i);
					if (j >= bits_err) {
						n = size_step;
						break;
					}
				}
			}
		}
	}
}

int flash_nand_error_bit_test(mxmtd_t *mxmtd, uint32_t blk)
{
	int ret, n, m;
	uint32_t page = 3;
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t size_page = mxmtd->ops.get_page_size(mxmtd);
	uint64_t addr = blk * size_blk;
	uint8_t *buf_rd = malloc(size_page);
	uint8_t *buf_wr = malloc(size_page);
	uint8_t *buf_wr_err = malloc(size_page);

	mxic_pr_inf("----Nand ECC test, <START>\r\n");

	if (0 == buf_rd || 0 == buf_wr || 0 == buf_wr_err) {
		mxic_pr_err("Allocate buffer failed, size(%08X)\r\n", size_page);
		goto release_flash_nand_error_bit_test;
	}

	ret = flash_nand_find_gb(mxmtd, &addr);
	if (MXST_SUCCESS != ret) {
		goto release_flash_nand_error_bit_test;
	}

	mxic_pr_inf("Test range: %08X-%08Xh ~ %08X: %08Xh, Block: %d, Page: %d\r\n",
			(uint32_t )(addr >> 32), (uint32_t)(addr),
			(uint32_t )((addr + size_page - 1) >> 32), (uint32_t)(addr + size_page - 1),
			(uint32_t)(addr >> fmsb32(size_blk)),
			page);

	for (n = 0; n < size_page; n++) {
		buf_wr[n] = rand() % 0x100;
		buf_wr_err[n] = buf_wr[n];
	}

	for (n = 0; n < mxmtd->ops.nand.get_ecc_strength(mxmtd) + 1; n++) {
		ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
		if (MXST_SUCCESS != ret) {
			goto release_flash_nand_error_bit_test;
		}

		/* Program random data with ECC */
		ret = mxmtd->ops.program(mxmtd, addr + page * size_page, buf_wr, size_page);
		if (MXST_SUCCESS != ret) {
			goto release_flash_nand_error_bit_test;
		}

		flash_nand_error_inject(mxmtd, buf_wr_err, n + 1);
		mxic_pr_inf("-- Inject %d error bit to each step within page\r\n", n + 1);
		/* Program error bit without ECC */
		mxmtd->ops.nand.set_ecc_en(mxmtd, 0);
		ret = mxmtd->ops.program(mxmtd, addr + page * size_page, buf_wr_err, size_page);
		if (MXST_SUCCESS != ret) {
			goto release_flash_nand_error_bit_test;
		}

		/* Read data with ECC */
		mxmtd->ops.nand.set_ecc_en(mxmtd, 1);
		ret = mxmtd->ops.read(mxmtd, addr + page * size_page, buf_rd, size_page);
		if (MXST_SUCCESS != ret) {
			goto release_flash_nand_error_bit_test;
		}
		if (-1 == mxmtd->ops.get_ecc_state(mxmtd)) {
			mxic_pr_err("The error bits can not be corrected\r\n");

		} else if (mxmtd->ops.get_ecc_state(mxmtd)) {
			mxic_pr_war("The max error bits(%d) in the ECC step have been corrected\r\n",
					mxmtd->ops.get_ecc_state(mxmtd));
		}

		if (memcmp(buf_rd, buf_wr, size_page)) {
			mxic_pr_err("Read data is not matched program data\r\n");
			for (m = 0; m < size_page; m++) {
				if (buf_wr[m] != buf_rd[m]) {
					mxic_pr_dbg("(%04d exp [%02X] ret [%02X], err inject[%02X]\r\n",
						n, buf_wr[m], buf_rd[m], buf_wr_err[m]);
				}
			}
			ret = MXST_ERR_CMP;
			goto release_flash_nand_error_bit_test;
		}
	}
	mxic_pr_inf("Read data is matched program data\r\n");

	ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
	if (MXST_SUCCESS != ret) {
		goto release_flash_nand_error_bit_test;
	}

release_flash_nand_error_bit_test:
	if (n == mxmtd->ops.nand.get_ecc_strength(mxmtd)) {
		if (-1 == mxmtd->ops.get_ecc_state(mxmtd)) {
			ret = MXST_SUCCESS;
		}
	} else {
		ret = MXST_ERR_ECC;
	}
	mxic_pr_inf("----[%s] Nand ECC test\r\n\r\n", MXST_SUCCESS == ret ? "PASS" : "FAIL");

	if (buf_rd) {
		free(buf_rd);
	}
	if (buf_wr) {
		free(buf_wr);
	}
	if (buf_wr_err) {
		free(buf_wr_err);
	}

	return ret;
}

int flash_nand_bb_test(mxmtd_t *mxmtd, uint32_t blk)
{
	int ret;
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint64_t addr = blk * size_blk;
	uint8_t is_bb;

	mxic_pr_inf("----Nand bad block test, <START>\r\n");

	ret = flash_nand_find_gb(mxmtd, &addr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxic_pr_inf("Block %d is good\r\n",
			(uint32_t)(addr >> fmsb32(size_blk)));

	mxic_pr_inf("Erase block %d then program its oob as a bad block\r\n",
			(uint32_t)(addr >> fmsb32(size_blk)));
	ret = mxmtd->ops.nand.set_bb(mxmtd, addr, 1);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	is_bb = mxmtd->ops.nand.chk_bb(mxmtd, addr);
	mxic_pr_war("Check block %d, its %s\r\n",
			(uint32_t)(addr >> fmsb32(size_blk)),
			is_bb ? "bad" : "good");

	ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxic_pr_inf("----[%s] Nand bad block test\r\n\r\n",
			is_bb ? "PASS" : "FAIL");

	return ret;
}

int flash_nand_nop_test(mxmtd_t *mxmtd, uint32_t blk, uint32_t page, uint8_t npages)
{
	int ret = MXST_SUCCESS, nop, nop_max = 5000;
	uint32_t n;
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t size_page = mxmtd->ops.get_page_size(mxmtd);
	uint32_t size_rw = npages * size_page;
	uint64_t addr = blk * size_blk;
	uint8_t *buf_rd = malloc(size_rw);
	uint8_t *buf_wr = malloc(size_rw);
	uint32_t *ecc_state_accu = calloc(mxmtd->ops.nand.get_ecc_strength(mxmtd) + 1, sizeof(uint32_t));
	int ecc_state;

	mxic_pr_inf("----Nand NOP test, block: %lu, page: %lu, npages: %d, <START>\r\n",
			blk, page, npages);

	if (0 == buf_rd || 0 == buf_wr) {
		mxic_pr_err("Allocate buffer failed, size(%08X)\r\n", size_blk);
		goto release_flash_nand_nop_test;
	}

	ret = flash_nand_find_gb(mxmtd, &addr);
	if (MXST_SUCCESS != ret) {
		goto release_flash_nand_nop_test;
	}

	for (n = 0; n < size_rw; n++) {
		buf_wr[n] = rand() % 0x100;
	}

	ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
	if (MXST_SUCCESS != ret) {
		goto release_flash_nand_nop_test;
	}

	mxic_pr_inf("ECC error bits usage status (Max. NOP = %d):\r\n", nop_max);
	for (nop = 0; nop < nop_max; nop++) {
		ret = mxmtd->ops.program(mxmtd, addr + page * size_page, buf_wr, size_rw);
		if (MXST_SUCCESS != ret) {
			goto release_flash_nand_nop_test;
		}

		ret = mxmtd->ops.read(mxmtd, addr + page * size_page, buf_rd, size_rw);
		if (MXST_SUCCESS != ret) {
			goto release_flash_nand_nop_test;
		}

		ecc_state = mxmtd->ops.get_ecc_state(mxmtd);

		if (-1 == ecc_state) {
			ecc_state_accu[mxmtd->ops.nand.get_ecc_strength(mxmtd)] = nop;
			break;
		}

		if (ecc_state && 0 == ecc_state_accu[ecc_state - 1]) {
			ecc_state_accu[ecc_state - 1] = nop;
		}
	}

	for (n = 0; n < mxmtd->ops.nand.get_ecc_strength(mxmtd); n++) {
		mxic_pr_inf("ECC used %d : NOP = %d\r\n", n + 1, ecc_state_accu[n]);
	}

	mxic_pr_inf("Can not be corrected from NOP = %d\r\n",
			ecc_state_accu[mxmtd->ops.nand.get_ecc_strength(mxmtd)]);

	ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
	if (MXST_SUCCESS != ret) {
		goto release_flash_nand_nop_test;
	}

release_flash_nand_nop_test:

	mxic_pr_inf("----[%s] Nand NOP test\r\n\r\n",
			MXST_SUCCESS == ret ? "PASS" : "FAIL");
	if (buf_rd) {
		free(buf_rd);
	}

	if (buf_wr) {
		free(buf_wr);
	}
	return ret;
}

int flash_random_test(mxmtd_t *mxmtd, uint32_t ntimes)
{
	int ret;
	uint64_t addr, nbytes;
	uint32_t size_chip = mxmtd->ops.get_chip_size(mxmtd);
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t nblks = size_chip / size_blk;
	uint32_t blk, n, m = 0;
	uint8_t *buf_rd = malloc(size_blk);
	uint8_t *buf_wr = malloc(size_blk);

	mxic_pr_inf("----random Erase/Program/Read test,target: %lu times, <START>\r\n", ntimes);

	if (0 == buf_rd || 0 == buf_wr) {
		mxic_pr_err("Allocate buffer failed, size(%08X)\r\n", size_blk);
		ret = MXST_ERR_ALLOC;
		goto release_flash_random_test;
	}

	while (m < ntimes) {
		if (mxmtd->ops.nand.chk_bb) {
			do {
				blk = rand() % nblks;
				addr = blk * size_blk;
			} while (mxmtd->ops.nand.chk_bb(mxmtd, addr));
		} else {
			blk = rand() % nblks;
			addr = blk * size_blk;
		}

		addr += rand() % size_blk;
		nbytes = rand() % (size_blk - (addr % size_blk));

		if (0 == (m & 63) || (ntimes - 1) == m) {
			mxic_pr_inf("Flash Random Test @ %d\r\n", m);
		}

		for (n = 0; n < nbytes; n++) {
			buf_wr[n] = rand() % 0xff;
		}

		ret = flash_epr_verify_in_blk(mxmtd, addr, buf_wr, buf_rd, nbytes, 1);
		if (MXST_SUCCESS != ret) {
			char s[100] = {};
			sprintf(s, "COMPARISON FAILED @ %lu", m);
			pr_test_range(mxmtd, addr, nbytes, s);
			goto release_flash_random_test;
		}
		m++;
	}

release_flash_random_test:
	mxic_pr_inf("----[%s] random Erase/Program/Read test, target: %lu times\r\n\r\n",
			(MXST_SUCCESS == ret) ? "PASS" : "FAIL", m);
	if (buf_rd) {
		free(buf_rd);
	}

	if (buf_wr) {
		free(buf_wr);
	}

	return ret;
}

int flash_torture_test(mxmtd_t *mxmtd, uint32_t blk, uint32_t ntimes)
{
	int ret, n;
	uint64_t addr;
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);
	uint32_t m = 0;
	uint8_t *buf_rd = malloc(size_blk);
	uint8_t *buf_wr = malloc(size_blk);
	uint8_t ptn[] = {0x55, 0xaa, 0xf0, 0x0f, 00, 0xff};

	mxic_pr_inf("----Torture test, "
			"Pattern: 0xff, 0x00, 0x0f, 0xf0, 0xaa, 0x55, "
			"target: %lu times, <START>\r\n", ntimes);

	if (0 == buf_rd || 0 == buf_wr) {
		mxic_pr_err("Allocate buffer failed, size(%08X)\r\n", size_blk);
		ret = MXST_ERR_ALLOC;
		goto release_flash_torture_test;
	}

	addr = blk * size_blk;
	while (ntimes ? (m < ntimes) : 1) {

		if (0 == (m & 15)) {
			mxic_pr_inf("Torture Test @ %d\r\n", m);
		}
		n = sizeof(ptn);
		while (n--) {
			memset(buf_wr, ptn[n], size_blk);
			ret = flash_epr_verify_in_blk(mxmtd, addr, buf_wr, buf_rd, size_blk,
					(0xff == ptn[n]) ? 0 : 1);
			if (MXST_SUCCESS != ret) {
				mxic_pr_err("Failed, times: %d, pattern: %02X\r\n", m, ptn[n]);
				goto release_flash_torture_test;
			}
		}
		m++;
	}
release_flash_torture_test:
	mxic_pr_inf("----[%s] Torture test\r\n\r\n", (MXST_SUCCESS == ret) ? "PASS" : "FAIL");
	if (buf_rd) {
		free(buf_rd);
	}

	if (buf_wr) {
		free(buf_wr);
	}

	return ret;
}

int flash_erase_all(mxmtd_t *mxmtd)
{
	int ret;
	uint64_t addr = 0;
	uint32_t size_blk = mxmtd->ops.get_blk_size(mxmtd);

	for (addr = 0; addr < mxmtd->ops.get_chip_size(mxmtd); addr += size_blk) {
		if (mxmtd->ops.nand.chk_bb && mxmtd->ops.nand.chk_bb(mxmtd, addr)) {
			mxic_pr_war("Skip bad block %lu\r\n", (uint32_t)(addr >> fmsb32(size_blk)));
			continue;
		}
		ret = mxmtd->ops.erase(mxmtd, addr, size_blk);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	return MXST_SUCCESS;
}
