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

#include <malloc.h>
#include <stdio.h>
#include "spinor_sfdp.h"
#include "spinor_cmd.h"
#include "mxmtd.h"
#include "mxmtd_err.h"
#include "platform_print.h"

#define SFDP_PARAM_ID_BFPT				0xff00
#define SFDP_PARAM_ID_XSPI_PROF10		0xff05
#define SFDP_PARAM_ID_SECTOR_MAP		0xff81
#define SFDP_PARAM_ID_4B_ADDR_IT		0xff84
#define SFDP_PARAM_ID_SCCR_MAP			0xff87
#define SFDP_PARAM_ID_CMD_SEQ_8D8D8D	0xff0a

#define SFDP_PARAM_ID(_header)	(((_header)->id_msb << 8) | (_header)->id_lsb)
#define SFDP_TABLE_PTR(_header)	( \
		((_header)->ptr_param_table[2] << 16) | \
		((_header)->ptr_param_table[1] << 8) | \
		((_header)->ptr_param_table[0]) \
		)
#define SFDP_TABLE_SIZE(_header)	((_header)->ndwords * 4)

#define CHK_PARAM_ID(_header, _NAME) \
do { \
	if (SFDP_PARAM_ID_##_NAME != SFDP_PARAM_ID(_header)) { \
		mxic_pr_err("comparison of %s Header ID failed\r\n", #_NAME); \
		return MXST_ERR_NOT_MATCH; \
	} \
} while (0)

typedef struct {
	uint8_t id_lsb;
	uint8_t minor;
	uint8_t major;
	uint8_t ndwords;
	uint8_t ptr_param_table[3];
	uint8_t id_msb;
} param_header_t;

typedef struct {
	uint32_t signature;
	uint8_t minor;
	uint8_t major;
	uint8_t num_param_header;
	uint8_t reserved;

	param_header_t bfpt_header;
} sfdp_header_t;

static void sfdp_pr_sfdp_header(sfdp_header_t *header)
{
	mxic_pr_dbg("SFDP Header, nParams: %d, Version: %d.%d\r\n",
			header->num_param_header + 1, header->major, header->minor);

	for (int n = 0; n < (sizeof(sfdp_header_t) - sizeof(param_header_t)) / 4; n++) {
		mxic_pr_dbg("  - DW %02d, %08X\r\n", n, ((uint32_t *)header)[n]);
	}
}

static void sfdp_pr_param_header(param_header_t *header, char *name)
{
	mxic_pr_dbg("Parameter Header[%s], ID: %04X, Table Ptr: %06X, Table nDWords: %d, Version: %d.%d\r\n",
			name, SFDP_PARAM_ID(header), SFDP_TABLE_PTR(header), header->ndwords,
			header->major, header->minor);

	for (int n = 0; n < sizeof(param_header_t) / 4; n++) {
		mxic_pr_dbg("  - DW %02d, %08X\r\n", n, ((uint32_t *)header)[n]);
	}
}

static int sfdp_get_param_table(mxmtd_t *mxmtd, param_header_t *header, uint32_t *table, char *name)
{
	int ret;

	ret = cmd_spinor_rdsfdp(mxmtd->spinor.cmd_meta, SFDP_TABLE_PTR(header), (uint8_t *)table, SFDP_TABLE_SIZE(header));
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxic_pr_dbg("Parameter Table [%s]:\r\n", name);
	for (int n = 0; n < header->ndwords; n++) {
		mxic_pr_dbg("  - DW %02d, %08X\r\n", n, table[n]);
	}

	return ret;
}

static int sfdp_parse_common(mxmtd_t *mxmtd, param_header_t *header)
{
	int ret;
	uint32_t *table = 0;
	char buf[5] = {};

	sfdp_pr_param_header(header, "");

	ret = snprintf(buf, sizeof(buf), "%04X", SFDP_PARAM_ID(header));
	if (0 > ret) {
		return MXST_ERR_PARAM;
	}

	table = malloc(SFDP_TABLE_SIZE(header));
	if (0 == table) {
		return MXST_ERR_ALLOC;
	}
	ret = sfdp_get_param_table(mxmtd, header, table, buf);
	free(table);

	return ret;
}

static int sfdp_parse_bfpt(mxmtd_t *mxmtd, param_header_t *header)
{
	int ret;
	uint32_t *bfpt = 0;

	CHK_PARAM_ID(header, BFPT);

	sfdp_pr_param_header(header, "BFPT");

	bfpt = malloc(SFDP_TABLE_SIZE(header));
	if (0 == bfpt) {
		return MXST_ERR_ALLOC;
	}
	ret = sfdp_get_param_table(mxmtd, header, bfpt, "BFPT");
	free(bfpt);

	return ret;
}

int sfdp_parsing(mxmtd_t *mxmtd)
{
	int ret;
	sfdp_header_t header_sfdp;
	param_header_t *header_param = 0;

	ret = cmd_spinor_rdsfdp(mxmtd->spinor.cmd_meta, 0, &header_sfdp, sizeof(sfdp_header_t));
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	sfdp_pr_sfdp_header(&header_sfdp);

	if (memcmp(&header_sfdp.signature, "SFDP", 4)) {
		mxic_pr_err("comparison of SFDP signature failed\r\n");
		return MXST_ERR_NOT_SUP;
	}

	ret = sfdp_parse_bfpt(mxmtd, &header_sfdp.bfpt_header);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (0 == header_sfdp.num_param_header) {
		return MXST_SUCCESS;
	}

	/* Parse other parameter headers & tables */
	header_param = malloc(header_sfdp.num_param_header * 8);
	if (0 == header_param) {
		return MXST_ERR_ALLOC;
	}

	ret = cmd_spinor_rdsfdp(mxmtd->spinor.cmd_meta, sizeof(sfdp_header_t), header_param,
			header_sfdp.num_param_header * 8);
	if (MXST_SUCCESS != ret) {
		free(header_param);
		return ret;
	}

	for (int n = 0; n < header_sfdp.num_param_header; n++) {
		switch (SFDP_PARAM_ID(header_param)) {
//		case SFDP_PARAM_ID_XSPI_PROF10:
//			sfdp_parse_param_header(header_param, "xSPI Profile 1.0");
//			break;
//		case SFDP_PARAM_ID_SECTOR_MAP:
//			sfdp_parse_param_header(header_param, "Sector Map");
//			break;
//		case SFDP_PARAM_ID_4B_ADDR_IT:
//			sfdp_parse_param_header(header_param, "4-Bytes Address IT");
//			break;
//		case SFDP_PARAM_ID_SCCR_MAP:
//			sfdp_parse_param_header(header_param, "SCC Reg Map");
//			break;
//		case SFDP_PARAM_ID_CMD_SEQ_8D8D8D:
//			break;
		default:
			ret = sfdp_parse_common(mxmtd, &header_param[n]);
			break;
		}
	}
	free(header_param);

	return ret;
}

#endif /* #ifdef CONF_SPINOR */
