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
#include "platform_itf.h"
#include "spinor_cmd.h"
#include "mxmtd_err.h"

#define SET_CMD_RPE(_cmd_meta)															\
	.cmd.val = _cmd_meta->cmd_info->cmd | (_cmd_meta->cmd_info->cmd ^ _cmd_meta->mask_cmd) << 8,	\
	.cmd.len = CHECK_PROT_8IO(_cmd_meta->cmd_info->prot[_cmd_meta->spinor_mode]) + 1

#define SET_CMD_REG(_cmd_meta, _cmd) 					\
	.cmd.val = _cmd | ((_cmd ^ _cmd_meta->mask_cmd) << 8),	\
	.cmd.len = CHECK_PROT_8IO(_cmd_meta->prot_reg) + 1

#define SET_ADDR(_pkt, _addr, _len_addr)	\
	_pkt->addr.len = _len_addr;				\
	_pkt->addr.val = _addr;

#define SET_DMY(_pkt, _len_dmy)	\
	_pkt->dummy.len = _len_dmy;

#define SET_DATA(_pkt, _data, _len_data)	\
	_pkt->data.len = _len_data;				\
    _pkt->data.buf = _data;

#define SET_BW_DTR(_pkt, _prot)								\
	_pkt->cmd.bw = (_prot >> OFS_PROT_BW_CMD) & 0xf;		\
	_pkt->cmd.dtr = (_prot >> OFS_PROT_DTR_CMD) & 0x1;		\
	_pkt->addr.bw = (_prot >> OFS_PROT_BW_ADDR) & 0xf;		\
	_pkt->addr.dtr = (_prot >> OFS_PROT_DTR_ADDR) & 0x1;	\
	_pkt->data.bw = (_prot >> OFS_PROT_BW_DATA) & 0xf;		\
	_pkt->data.dtr = (_prot >> OFS_PROT_DTR_DATA) & 0x1;

#define CONF_PKT(_pkt, _prot, _a, _len_a, _len_m, _d, _len_d, _dqs, _wm, _dir)	\
	do {																		\
		SET_ADDR(_pkt, _a, _len_a)												\
		SET_DMY(_pkt, _len_m)													\
		SET_DATA(_pkt, _d, _len_d)												\
		SET_BW_DTR(_pkt, _prot)													\
		_pkt->dqs = _dqs;														\
		_pkt->word_mode = _wm;													\
		_pkt->dir = _dir;														\
	} while(0)

/* replace macro with function to reduce code size(text) */
void template_read(pkt_t *pkt, uint32_t prot, uint64_t addr, uint8_t len_addr,
		uint8_t len_dmy, uint8_t *buf, uint64_t nbytes, uint8_t dqs, uint8_t wm)
{
	CONF_PKT(pkt, prot, addr, len_addr, len_dmy, buf, nbytes, dqs, wm, DIR_RD);
}

void template_write(pkt_t *pkt, uint32_t prot, uint64_t addr, uint8_t len_addr,
		uint8_t *buf, uint64_t nbytes, uint8_t wm)
{
	CONF_PKT(pkt, prot, addr, len_addr, 0, buf, nbytes, 0, wm, DIR_WR);
}

int cmd_spinor_preamble_calibration(cmd_meta_t *cmd_meta)
{
	int ret;
	xfer_info_t *xfer = cmd_meta->priv;
	uint8_t len_cali = (cmd_meta->cmd_info->dc[cmd_meta->spinor_mode] & 0xff) - 4;
	uint8_t *buf_rd;

	buf_rd = malloc(len_cali);
	if (!buf_rd) {
		return MXST_ERR_ALLOC;
	}

	xfer->buf_cali = malloc(16);
	if (!xfer->buf_cali) {
		free(buf_rd);
		return MXST_ERR_ALLOC;
	}

	/* Pattern for preamble bit */
	xfer->buf_cali[0] = 0x00;
	xfer->buf_cali[1] = 0x00;
	xfer->buf_cali[2] = 0xff;
	xfer->buf_cali[3] = 0xff;
	xfer->buf_cali[4] = 0x00;
	xfer->buf_cali[5] = 0xff;
	xfer->buf_cali[6] = 0x00;
	xfer->buf_cali[7] = 0x08;
	xfer->buf_cali[8] = 0xF7;
	xfer->buf_cali[9] = 0x00;
	xfer->buf_cali[10] = 0x00;
	xfer->buf_cali[11] = 0xff;
	xfer->buf_cali[12] = 0xf7;
	xfer->buf_cali[13] = 0x08;
	xfer->buf_cali[14] = 0xf7;
	xfer->buf_cali[15] = 0x00;

	PKTS(xfer->pkts, {SET_CMD_RPE(cmd_meta)});
	template_read(&xfer->pkts[0],
				cmd_meta->cmd_info->prot[cmd_meta->spinor_mode],
				0,
				cmd_meta->cmd_info->len_addr,
				4,
				buf_rd,
				len_cali,
				cmd_meta->cmd_info->dqs,
				0);

	ret = platform_xfer(xfer);
	free(buf_rd);
	free(xfer->buf_cali);
	xfer->buf_cali = 0;

	return ret;
}

int inline cmd_spinor_read(cmd_meta_t *cmd_meta, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_RPE(cmd_meta)});

	template_read(&xfer->pkts[0],
			cmd_meta->cmd_info->prot[cmd_meta->spinor_mode],
			addr,
			cmd_meta->cmd_info->len_addr,
			cmd_meta->cmd_info->dc[cmd_meta->spinor_mode] & 0xff,
			buf,
			nbytes,
			cmd_meta->cmd_info->dqs,
			cmd_meta->cmd_info->word_mode);

	return platform_xfer(xfer);
}

int inline cmd_spinor_program(cmd_meta_t *cmd_meta, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_RPE(cmd_meta)});

	template_write(&xfer->pkts[0],
			cmd_meta->cmd_info->prot[cmd_meta->spinor_mode],
			addr,
			cmd_meta->cmd_info->len_addr,
			buf,
			nbytes,
			cmd_meta->cmd_info->word_mode);

    return platform_xfer(xfer);
}

int inline cmd_spinor_erase(cmd_meta_t *cmd_meta, uint64_t addr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_RPE(cmd_meta)});

	template_write(&xfer->pkts[0],
			cmd_meta->cmd_info->prot[cmd_meta->spinor_mode],
			addr,
			cmd_meta->cmd_info->len_addr,
			0,
			0,
			0);

	return platform_xfer(xfer);
}

int inline cmd_spinor_rdid(cmd_meta_t *cmd_meta, uint8_t *id)
{
    int ret;
    xfer_info_t *xfer = cmd_meta->priv;

    PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDID)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, id, 6, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, id, 3, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, id, 3, 0, 0);
        break;
    }

    ret = platform_xfer(xfer);

    if (PROT_8D_8D_8D == cmd_meta->prot_reg) {
        for (int n = 0; n < 3; n++) {
            id[n] = id[n * 2  + 1];
        }
    }
    return ret;
}

int cmd_spinor_rdsr(cmd_meta_t *cmd_meta, uint8_t *sr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDSR)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, sr, 1, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, sr, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, sr, 1, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_rdcr(cmd_meta_t *cmd_meta, uint8_t *cr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDCR)});

	switch (cmd_meta->prot_reg) {
	case PROT_8D_8D_8D:
		template_read(&xfer->pkts[0], cmd_meta->prot_reg, 1, 4, 4, cr, 1, 1, 0);
		break;
	case PROT_8S_8S_8S:
		template_read(&xfer->pkts[0], cmd_meta->prot_reg, 1, 4, 4, cr, 1, 0, 0);
		break;
	default:
		template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, cr, 1, 0, 0);
		break;
	}

    return platform_xfer(xfer);
}

int cmd_spinor_wrcr(cmd_meta_t *cmd_meta, uint8_t *cr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRCR)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    case PROT_8S_8S_8S:
    	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 1, 4, cr, 1, 0);
        break;
    default:
    	return MXST_ERR_NOT_SUP;
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_wrsr(cmd_meta_t *cmd_meta, uint8_t *sr, uint8_t len_sr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRSR)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    case PROT_8S_8S_8S:
    	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, sr, len_sr, 0);
        break;
    default:
    	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, sr, len_sr, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_rdcr2(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *cr2)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDCR2)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 4, cr2, 1, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 4, cr2, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 0, cr2, 1, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_wrcr2(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *cr2)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRCR2)});
    template_write(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, cr2, 1, 0);

    return platform_xfer(xfer);
}

/* Command only */
int cmd_spinor_en4b(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_EN4B)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_ex4b(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_EX4B)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_rsten(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RSTEN)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_rst(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RST)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_eqpi(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_EQPI)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_rstqpi(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RSTQPI)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_wren(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WREN)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_rdsfdp(cmd_meta_t *cmd_meta, uint32_t addr, void *sfdp, uint32_t nbytes)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDSFDP)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 20, sfdp, nbytes, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 20, sfdp, nbytes, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 3, 8, sfdp, nbytes, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_rdlr(cmd_meta_t *cmd_meta, uint8_t *lr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDLR)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, lr, 1, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, lr, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, lr, 1, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_wrlr(cmd_meta_t *cmd_meta, uint8_t *lr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRLR)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 0, lr, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, lr, 1, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_rddpb(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *dpb)
{
	xfer_info_t *xfer = cmd_meta->priv;
	uint8_t len_dmy = cmd_meta->cmd_info->dc[cmd_meta->spinor_mode] & 0xff;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDDPB)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, len_dmy, dpb, 1, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, len_dmy, dpb, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 0, dpb, 1, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_wrdpb(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *dpb)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRDPB)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, dpb, 1, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_rdspb(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *spb)
{
	xfer_info_t *xfer = cmd_meta->priv;
	uint8_t len_dmy = cmd_meta->cmd_info->dc[cmd_meta->spinor_mode] & 0xff;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDSPB)});

    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, len_dmy, spb, 1, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, len_dmy, spb, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 0, spb, 1, 0, 0);
        break;
    }

    return platform_xfer(xfer);
}

int cmd_spinor_wrspb(cmd_meta_t *cmd_meta, uint32_t addr)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRSPB)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, addr, 4, 0, 0, 0);

    return platform_xfer(xfer);
}

int cmd_spinor_ersspb(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_ERSSPB)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

	return platform_xfer(xfer);
}

int cmd_spinor_wpsel(cmd_meta_t *cmd_meta)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WPSEL)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, 0, 0);

	return platform_xfer(xfer);
}

int cmd_spinor_rdscur(cmd_meta_t *cmd_meta, uint8_t *scur)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_RDSCUR)});
    switch (cmd_meta->prot_reg) {
    case PROT_8D_8D_8D:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, scur, 1, 1, 0);
        break;
    case PROT_8S_8S_8S:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 4, 4, scur, 1, 0, 0);
        break;
    default:
    	template_read(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, 0, scur, 1, 0, 0);
        break;
    }

	return platform_xfer(xfer);
}

int cmd_spinor_wrscur(cmd_meta_t *cmd_meta, uint8_t *scur)
{
	xfer_info_t *xfer = cmd_meta->priv;

	PKTS(xfer->pkts, {SET_CMD_REG(cmd_meta, SPINOR_WRSCUR)});
	template_write(&xfer->pkts[0], cmd_meta->prot_reg, 0, 0, scur, 1, 0);

	return platform_xfer(xfer);
}

#endif /* #ifdef CONF_SPINOR */
