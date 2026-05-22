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

/* Safety bound: missing PKT_CTRL_END would otherwise walk off into memory. */
#define PLATFORM_MAX_PKTS 1024U

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

/* Packet-level polling: re-issues the same packet until exp matches or timeout. */
static int exec_poll(xfer_info_t *xfer, pkt_t *pkt)
{
	int ret;
	uint32_t us = pkt->poll.timeout_us;

	if (!pkt->data.buf || pkt->data.len < 1) {
		mxic_pr_err("Polling requires a valid RX buffer (data.buf) with len >= 1\r\n");
		return MXST_ERR_PARAM;
	}
	if (!us) {
		mxic_pr_err("Polling timeout is 0 us\r\n");
		return MXST_ERR_TIMEOUT;
	}

	do {
		ret = xfer->ops.xfer(xfer);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		mxic_pr_dbg_a("polling, data: %08X, mask: %08X\r\n",
				pkt->data.buf[0], pkt->poll.mask);

		xfer->ops.delay_us(1);
		us--;
	} while (pkt->poll.exp != (pkt->poll.mask & pkt->data.buf[0]) && us);

	if (pkt->poll.exp != (pkt->poll.mask & pkt->data.buf[0])) {
		mxic_pr_err("Polling timeout (%lu us) failed. mask: %02Xh, ret val: %02Xh, exp: %02Xh\r\n",
				pkt->poll.timeout_us, pkt->poll.mask, pkt->data.buf[0],
				pkt->poll.exp);
		return MXST_ERR_TIMEOUT;
	}

	return MXST_SUCCESS;
}

static inline int exec_xfer(xfer_info_t *xfer, pkt_t *pkt)
{
	print_pkt(pkt);

	if (pkt->poll_en) {
		return exec_poll(xfer, pkt);
	}
	return xfer->ops.xfer(xfer);
}

int platform_xfer(xfer_info_t *xfer)
{
	int ret = MXST_SUCCESS;
	pkt_t *pkt;
	pkt_t *orig_pkts;
	uint32_t n = 0;

	if (!xfer || !xfer->pkts) {
		return MXST_ERR_PTR_NULL;
	}

	orig_pkts = xfer->pkts;
	pkt = orig_pkts;
	while (PKT_CTRL_END != pkt->pkt_ctrl) {
		if (n++ >= PLATFORM_MAX_PKTS) {
			mxic_pr_err("Packet list missing PKT_CTRL_END (>%u packets)\r\n", PLATFORM_MAX_PKTS);
			xfer->pkts = orig_pkts;
			return MXST_ERR_PARAM;
		}
		if (PKT_CTRL_SKIP == pkt->pkt_ctrl) {
			pkt++;
			continue;
		}
		xfer->pkts = pkt;
		ret = exec_xfer(xfer, pkt);
		if (MXST_SUCCESS != ret) {
			break;
		}
		pkt++;
	}

	/* Restore original packet pointer for callers that reuse the xfer object. */
	xfer->pkts = orig_pkts;
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
