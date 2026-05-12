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

#include "platform_print.h"
#include "platform_conf.h"
#include "platform_itf.h"
#include "mxmtd_err.h"

#define ENHC_NONE       0x01
#define ENHC_START      0x02
#define ENHC_KEEP       0x04
#define ENHC_END        0x08

#define ENHC_DMY_BYTE0  0x69
#define ENHC_DMY_BYTE1  0x96

static void inline print_pkt(pkt_t *pkt)
{
#if PR_LEVEL > 3

    mxic_pr_dbg_a("------CMD: %08X\r\n", pkt->cmd.val);

    mxic_pr_dbg_a("<CMD>  BW: %d, DTR: %d, LEN: %d, OpCode: %04Xh\r\n",
        pkt->cmd.bw,
        pkt->cmd.dtr,
        pkt->cmd.len,
        pkt->cmd.val);
            
    mxic_pr_dbg_a("<ADDR> BW: %d, DTR: %d, LEN: %d, Address: %X%08Xh\r\n",
        pkt->addr.bw,
        pkt->addr.dtr,
        pkt->addr.len,
        (uint32_t)(pkt->addr.val >> 32),
		(uint32_t)pkt->addr.val);
    
    mxic_pr_dbg_a("<DUMMY> Len: %d\r\n",
        pkt->dummy.len);
    
    mxic_pr_dbg_a("<DATA> BW: %d, DTR: %d, LEN: %X%08Xh, data buf addr: %08X, DQS: %d, Word Mode: %d\r\n",
        pkt->data.bw,
        pkt->data.dtr,
        (uint32_t)(pkt->data.len >> 32),
		(uint32_t)pkt->data.len,
		pkt->data.buf,
        pkt->dqs,
        pkt->word_mode);

    mxic_pr_dbg_a("---\r\n");
#endif
}

static int exec_poll(xfer_info_t *xfer)
{
	int ret;
	uint32_t us = xfer->pkts->poll.timeout_us;

	do {
		ret = xfer->ops.xfer(xfer);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		mxic_pr_dbg_a("polling, data: %08X, mask: %08X\r\n",
				xfer->pkts->data.buf[0], xfer->pkts->poll.mask);

		xfer->ops.delay_us(1);
		us--;
	} while (xfer->pkts->poll.exp != (xfer->pkts->poll.mask & xfer->pkts->data.buf[0]) && us);

	if (xfer->pkts->poll.exp != (xfer->pkts->poll.mask & xfer->pkts->data.buf[0])) {
		mxic_pr_err("Polling timeout (%lu us) failed. mask: %02Xh, ret val: %02Xh, exp: %02Xh\r\n",
				xfer->pkts->poll.timeout_us, xfer->pkts->poll.mask, xfer->pkts->data.buf[0],
				xfer->pkts->poll.exp);
		return MXST_ERR_TIMEOUT;
	}

	return MXST_SUCCESS;
}

static inline int exec_xfer(xfer_info_t *xfer)
{
	print_pkt(xfer->pkts);

	if (xfer->pkts->poll_en) {
		return exec_poll(xfer);
	}
	return xfer->ops.xfer(xfer);
}

int platform_xfer(xfer_info_t *xfer)
{
	int ret = MXST_SUCCESS;

	while (PKT_CTRL_END != xfer->pkts->pkt_ctrl) {
		if (PKT_CTRL_SKIP == xfer->pkts->pkt_ctrl) {
			xfer->pkts++;
			continue;
		}
		ret = exec_xfer(xfer);
		if (MXST_SUCCESS != ret) {
			break;
		}
		xfer->pkts++;
	}
	return ret;
}

int setup_platform(xfer_info_t *xfer, int ch_type, int port)
{
	int ret = MXST_SUCCESS;

    switch (CONF_HC_TYPE) {
    case HC_MXIC_UEFC:
        ret = setup_mxic_uefc(xfer, ch_type, port);
        break;
    default:
        mxic_pr_err("Host controller initial function can't be found\r\n");
        return MXST_ERR_NOT_SUP;
    };

    if (MXST_SUCCESS != ret) {
        return ret;
    }

    if (!xfer->ops.xfer || !xfer->ops.delay_us) {
        mxic_pr_err("xfer->ops.xfer or xfer->ops.delay_us is NULL!");
        return MXST_ERR_PTR_NULL;
    }

    return ret;
}

void release_platform(xfer_info_t *xfer)
{
    switch (CONF_HC_TYPE) {
    case HC_MXIC_UEFC:
        release_mxic_uefc(xfer);
        break;
    default:
    	break;
    };
}
