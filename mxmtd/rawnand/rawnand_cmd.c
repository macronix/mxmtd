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

#include <string.h>
#include "rawnand_err.h"
#include "rawnand_cmd.h"
#include "platform_print.h"
#include "platform_itf.h"

#define BIT(x) (1U << (x))

#define SR_RDY_BIT5 BIT(5)
#define SR_RDY_BIT6 BIT(6)

/* RAW NAND Commands */
#define NAND_READ0			0x00
#define NAND_READ1			0x01
#define NAND_RNDOUT			0x05
#define NAND_RNDOUT_CONFIRM	0xe0
#define NAND_PAGEPROG		0x10
#define NAND_READOOB		0x50
#define NAND_ERASE1			0x60
#define NAND_STATUS			0x70
#define NAND_BP_STATUS_READ 0x7a
#define NAND_SEQIN			0x80
#define NAND_RNDIN			0x85
#define NAND_READID			0x90
#define NAND_ERASE2			0xd0
#define NAND_PARAM			0xec
#define NAND_GET_FEATURES	0xee
#define NAND_SET_FEATURES	0xef
#define NAND_RESET			0xff
#define NAND_READSTART		0x30
#define NAND_READCACHESEQ	0x31
#define NAND_READCACHEEND	0x3f
#define NAND_RNDOUTSTART	0xe0
#define NAND_CACHEDPROG		0x15

#define SET_POLL(_mask, _exp, _timeout) \
	.poll_en = 1, .poll.mask = (_mask), .poll.exp = (_exp), .poll.timeout_us = (_timeout)

#define SET_CMD(_cmd, _len_cmd)		\
	.cmd.len = _len_cmd,			\
	.cmd.val = _cmd

#define SET_ADDR(_addr, _len_addr)	\
	.addr.len = _len_addr,			\
	.addr.val = _addr

#define SET_DATA(_data, _len_data)	\
    .data.len = _len_data,			\
    .data.buf = _data

#define SET_PROT(_cmd, _len_cmd, _addr, _len_addr, _data, _len_data, _dir)	\
	.cmd.bw = 8,															\
	.addr.bw = 8,															\
	.data.bw = 8,															\
	.dir = _dir,															\
	SET_CMD(_cmd, _len_cmd),												\
	SET_ADDR(_addr, _len_addr),												\
	SET_DATA(_data, _len_data)

#define SET_PROT_RD(_cmd, _len_cmd, _addr, _len_addr, _data, _len_data)		\
    SET_PROT(_cmd, _len_cmd, _addr, _len_addr, _data, _len_data, DIR_RD)

#define SET_PROT_WR(_cmd, _len_cmd, _addr, _len_addr,  _data, _len_data)	\
    SET_PROT(_cmd, _len_cmd, _addr, _len_addr, _data, _len_data, DIR_WR)

int cmd_rawnand_param_page_read(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes)
{
	uint32_t timeout_us = 600; //FIX
	uint8_t sr;

	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_PARAM, 1, 0, 1, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, &sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_DEASSERT, SET_PROT_RD(NAND_READ0, 1, 0, 0, buf, nbytes)}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_read_start(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint32_t timeout_us)
{
	uint8_t sr;

	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_READ0, 1, addr, len_addr, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_READSTART, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, &sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_page_read(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes, uint8_t last)
{
	PKTS(xfer->pkts,
		{CS_CTRL_DEASSERT, SET_PROT_RD(NAND_READ0, 1, 0, 0, buf, nbytes)}
	);

	if (0 == last) {
		xfer->pkts[0].cs_ctrl = CS_CTRL_KEEP;
	}

	return platform_xfer(xfer);
}

int cmd_rawnand_page_read_rnd(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes)
{
	PKTS(xfer->pkts,
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_RNDOUT, 1, col, len_col, 0, 0)},
		{CS_CTRL_DEASSERT, SET_PROT_RD(NAND_RNDOUT_CONFIRM, 1, 0, 0, buf, nbytes)}

	);

	return platform_xfer(xfer);
}

int cmd_rawnand_cache_read_seq(struct _xfer_info *xfer, uint8_t *buf, uint32_t nbytes, uint8_t last,
		uint32_t timeout_us)
{
	uint8_t sr;

	PKTS(xfer->pkts,
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_READCACHESEQ, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, &sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_READ0, 1, 0, 0, buf, nbytes)},
		{CS_CTRL_DEASSERT}
	);

	if (last) {
		xfer->pkts[0].cmd.val = NAND_READCACHEEND;
	} else {
		xfer->pkts[3].pkt_ctrl = PKT_CTRL_SKIP;
	}

	return platform_xfer(xfer);
}

int cmd_rawnand_page_pgm(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint8_t last, uint32_t timeout_us)
{
	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_SEQIN, 1, addr, len_addr, buf, nbytes)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_PAGEPROG, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_DEASSERT}
	);

	if (0 == last) {
		xfer->pkts[1].pkt_ctrl = PKT_CTRL_END;
	}

	return platform_xfer(xfer);
}

int cmd_rawnand_page_pgm_rnd(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us)
{
	PKTS(xfer->pkts,
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_RNDIN, 1, col, len_col, buf, nbytes)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_PAGEPROG, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_DEASSERT}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_cache_pgm_start(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us)
{
	timeout_us += 100; //FIX

	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_SEQIN, 1, addr, len_addr, buf, nbytes)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_CACHEDPROG, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_cache_pgm(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us)
{
	timeout_us += 100; //FIX

	PKTS(xfer->pkts,
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_SEQIN, 1, addr, len_addr, buf, nbytes)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_CACHEDPROG, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_cache_pgm_end(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us)
{
	PKTS(xfer->pkts,
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_SEQIN, 1, addr, len_addr, buf, nbytes)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_PAGEPROG, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_DEASSERT}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_block_erase(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *sr,
		uint32_t timeout_us)
{
	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_ERASE1, 1, addr, len_addr, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_WR(NAND_ERASE2, 1, 0, 0, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_DEASSERT}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_rdid(xfer_info_t *xfer, uint32_t addr, uint8_t *id, uint8_t len_id)
{
	PKTS(xfer->pkts,
		{SET_PROT_RD(NAND_READID, 1, addr, 1, id, len_id)}
	);
	return platform_xfer(xfer);
}

int cmd_rawnand_reset(xfer_info_t *xfer)
{
	PKTS(xfer->pkts,
		{SET_PROT_WR(NAND_RESET, 1, 0, 0, 0, 0)}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_bp_status_read(xfer_info_t *xfer, uint32_t addr, uint8_t len_addr,
		uint8_t *bpsr)
{
	PKTS(xfer->pkts,
		{SET_PROT_RD(NAND_BP_STATUS_READ, 1, addr, len_addr, bpsr, 1)}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_get_feature(xfer_info_t *xfer, uint32_t addr, uint8_t *buf)
{
	uint8_t sr;
	uint32_t timeout_us = 10; //FIX

	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_GET_FEATURES, 1, addr, 1, 0, 0)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, &sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_READ0, 1, 0, 0, buf, 4)},
		{CS_CTRL_DEASSERT}
	);

	return platform_xfer(xfer);
}

int cmd_rawnand_set_feature(xfer_info_t *xfer, uint32_t addr, uint8_t *buf)
{
	uint8_t sr;
	uint32_t timeout_us = 10; //FIX

	PKTS(xfer->pkts,
		{CS_CTRL_ASSERT, SET_PROT_WR(NAND_SET_FEATURES, 1, addr, 1, buf, 4)},
		{CS_CTRL_KEEP, SET_PROT_RD(NAND_STATUS, 1, 0, 0, &sr, 1),
				SET_POLL(SR_RDY_BIT6, SR_RDY_BIT6, timeout_us)},
		{CS_CTRL_DEASSERT}
	);

	return platform_xfer(xfer);
}

#endif /* #ifdef CONF_RAWNAND */
