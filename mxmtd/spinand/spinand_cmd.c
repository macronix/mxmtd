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
#ifdef CONF_SPINAND

#include <string.h>
#include "spinand_err.h"
#include "spinand_cmd.h"
#include "platform_print.h"
#include "platform_itf.h"

#define  ADDR_CR_B0	0xb0

#define  SR_OIP		0x01
#define  SR_WEL		0x02
#define  SR_CRBSY	0x80

#define SPINAND_CMD_RDID					0x9F
#define SPINAND_CMD_READ			 		0x13
#define SPINAND_CMD_READ_CACHE			 	0x03
#define SPINAND_CMD_READ_CACHE_ALTERNATIVE	0x0B
#define SPINAND_CMD_DREAD_CACHE			 	0x3B
#define SPINAND_CMD_QREAD_CACHE			 	0x6B
#define SPINAND_CMD_2READ_CACHE			 	0xBB
#define SPINAND_CMD_4READ_CACHE			 	0xEB
#define SPINAND_CMD_READ_CACHE_SEQUENTIAL	0x31
#define SPINAND_CMD_READ_CACHE_RANDOM		0x30
#define SPINAND_CMD_READ_CACHE_END		 	0x3F
#define SPINAND_CMD_GET_FEATURE			 	0x0F
#define SPINAND_CMD_SET_FEATURE			 	0x1F
#define SPINAND_CMD_WREN          			0x06
#define SPINAND_CMD_WRDI         			0x04
#define SPINAND_CMD_PP_LOAD				 	0x02
#define SPINAND_CMD_PP_LOAD_RND				0x84
#define SPINAND_CMD_QPP_LOAD				0x32
#define SPINAND_CMD_QPP_RAND_LOAD	     	0x34
#define SPINAND_CMD_PROGRAM_EXEC			0x10
#define SPINAND_CMD_BE					 	0xD8
#define SPINAND_CMD_RESET				 	0xFF
#define SPINAND_CMD_DP				 		0xB9
#define SPINAND_CMD_RDSR					0x05
#define SPINAND_SR_ADDR                     0xC0
#define SPINAND_CMD_ECC_SR_READ		 		0x7C
#define SPINAND_CMD_BBM_WRITE		 		0xA1
#define SPINAND_CMD_BBM_READ		 		0xA5
#define SPINAND_CMD_POR_PAGE_WRITE		 	0xA2
#define SPINAND_CMD_POR_PAGE_READ		 	0xA6
#define SPINAND_CMD_ECC_WARN_PAGE		 	0xA9

#define SET_POLL(_mask, _exp, _timeout) \
	.poll_en = 1, .poll.mask = (_mask), .poll.exp = (_exp), .poll.timeout_us = (_timeout)

#define SET_CMD(_cmd, _len_cmd)	\
	.cmd.len = _len_cmd,		\
	.cmd.val = _cmd

#define SET_ADDR(_addr, _len_addr)	\
	.addr.len = _len_addr,			\
	.addr.val = _addr

#define SET_DATA(_data, _len_data)	\
    .data.len = _len_data,			\
    .data.buf = _data

#define SET_DUMMY(_len_dummy)  \
	.dummy.len = _len_dummy

#define SET_PROT(_cmd, _len_cmd, _addr, _len_addr, _len_dummy, _data, _len_data, _dir)	\
	.cmd.bw = 1,				\
	.addr.bw = 1,				\
	.data.bw = 1,				\
	.dir = _dir,				\
	SET_CMD(_cmd, _len_cmd),	\
	SET_ADDR(_addr, _len_addr),	\
	SET_DUMMY(_len_dummy),		\
	SET_DATA(_data, _len_data)

#define SET_PROT_RD(_cmd, _len_cmd, _addr, _len_addr, _len_dummy, _data, _len_data)	\
    SET_PROT(_cmd, _len_cmd, _addr, _len_addr, _len_dummy, _data, _len_data, DIR_RD)

#define SET_PROT_WR(_cmd, _len_cmd, _addr, _len_addr,  _data, _len_data)	\
    SET_PROT(_cmd, _len_cmd, _addr, _len_addr, 0,_data, _len_data, DIR_WR)

int cmd_spinand_write_enable(xfer_info_t *xfer)
{
    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_WREN, 1, 0, 0, 0, 0)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_write_disable(xfer_info_t *xfer)
{
    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_WRDI, 1, 0, 0, 0, 0)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_rdid(xfer_info_t *xfer, uint8_t *id, uint8_t len_id)
{
    PKTS(xfer->pkts,
        {SET_PROT_RD(SPINAND_CMD_RDID, 1, 0, 0, 8, id, len_id)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_reset(xfer_info_t *xfer, uint8_t *sr)
{
	uint32_t timeout_us = 3000; //FIX

    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_RESET, 1, 0, 0, 0, 0)},
		{SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, SPINAND_SR_ADDR, 1, 0, sr, 1),
				SET_POLL(SR_OIP | SR_CRBSY, 0, timeout_us)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_get_feature(xfer_info_t *xfer, uint8_t addr, uint8_t *buf)
{
    PKTS(xfer->pkts,
        {SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, addr, 1, 0, buf, 1)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_set_feature(xfer_info_t *xfer, uint8_t addr, uint8_t *buf)
{
    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_SET_FEATURE, 1, addr, 1, buf, 1)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_rd_sr(xfer_info_t *xfer, uint8_t *sr)
{
    PKTS(xfer->pkts,
        {SET_PROT_RD(SPINAND_CMD_RDSR, 1, 0, 0, 0, sr, 1)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_rd_eccsr(xfer_info_t *xfer, uint8_t *eccsr)
{
	PKTS(xfer->pkts,
		{SET_PROT_RD(SPINAND_CMD_ECC_SR_READ, 1, 0, 0, 8, eccsr, 1)}
	);

	return platform_xfer(xfer);
}

int cmd_spinand_block_erase(xfer_info_t *xfer, uint32_t addr, uint8_t len_addr, uint8_t *sr,
		uint32_t timeout_us)
{
    PKTS(xfer->pkts,
    	{SET_PROT_WR(SPINAND_CMD_WREN, 1, 0, 0, 0, 0)},
        {SET_PROT_WR(SPINAND_CMD_BE, 1, addr, len_addr, 0, 0)},
		{SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, SPINAND_SR_ADDR, 1, 0, sr, 1),
				SET_POLL(SR_OIP | SR_WEL, 0, timeout_us)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_program_load(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes)
{
    PKTS(xfer->pkts,
    	{SET_PROT_WR(SPINAND_CMD_WREN, 1, 0, 0, 0, 0)},
        {SET_PROT_WR(SPINAND_CMD_PP_LOAD, 1, col, len_col, buf, nbytes)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_program_load_rnd(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes)
{
	PKTS(xfer->pkts,
    	{SET_PROT_WR(SPINAND_CMD_PP_LOAD_RND, 1, col, len_col, buf, nbytes)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_program_execute(xfer_info_t *xfer, uint32_t row, uint8_t len_row, uint8_t *sr,
		uint32_t timeout_us)
{
    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_PROGRAM_EXEC, 1, row, len_row, 0, 0)},
		{SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, SPINAND_SR_ADDR, 1, 0, sr, 1),
            SET_POLL(SR_OIP | SR_CRBSY | SR_WEL, 0, timeout_us)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_page_read(xfer_info_t *xfer, uint32_t row, uint8_t len_row, uint32_t timeout_us)
{
	uint8_t sr;

    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_READ, 1, row, len_row, 0, 0)},
		{SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, SPINAND_SR_ADDR, 1, 0, &sr, 1),
				SET_POLL(SR_OIP | SR_CRBSY, 0, timeout_us)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_read_cache(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes)
{
    PKTS(xfer->pkts,
    	{SET_PROT_RD(SPINAND_CMD_READ_CACHE, 1, col, len_col, 8, buf, nbytes)}
    );

    return platform_xfer(xfer);
}

int cmd_spinand_read_cache_seq(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes, uint8_t end, uint32_t timeout_us)
{
	uint8_t sr;

    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_READ_CACHE_SEQUENTIAL, 1, 0, 0, 0, 0)},
		{SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, SPINAND_SR_ADDR, 1, 0, &sr, 1),
				SET_POLL(SR_OIP | SR_CRBSY, 0, timeout_us)},
    	{SET_PROT_RD(SPINAND_CMD_READ_CACHE, 1, col, len_col, 8, buf, nbytes)}
    );

    if (end) {
    	xfer->pkts->cmd.val = SPINAND_CMD_READ_CACHE_END;
    }

	return platform_xfer(xfer);
}

int cmd_spinand_param_page_read(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes)
{
	uint8_t pp[2] = {0x40, 0x00};
	uint32_t timeout_us = 100; //FIX
	uint8_t sr;

    PKTS(xfer->pkts,
        {SET_PROT_WR(SPINAND_CMD_SET_FEATURE, 1, ADDR_CR_B0, 1, &pp[0], 1)},
		{SET_PROT_WR(SPINAND_CMD_READ, 1, 0x000001, 3, 0, 0)},
		{SET_PROT_RD(SPINAND_CMD_GET_FEATURE, 1, SPINAND_SR_ADDR, 1, 0, &sr, 1),
				SET_POLL(SR_OIP | SR_CRBSY, 0, timeout_us)},
		{SET_PROT_RD(SPINAND_CMD_READ_CACHE, 1, 0x00, 2, 8, buf, nbytes)},
		{SET_PROT_WR(SPINAND_CMD_SET_FEATURE, 1, ADDR_CR_B0, 1, &pp[1], 1)}
    );

    return platform_xfer(xfer);
}

#endif /* #ifdef CONF_SPINAND */
