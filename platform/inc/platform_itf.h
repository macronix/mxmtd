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

#ifndef _PLATFORM_ITF_H
#define _PLATFORM_ITF_H

#include <stdint.h>

typedef struct _xfer_info xfer_info_t;

#define MAX_CMD_LEN 3
#define MAX_ADDR_LEN 6

enum HC_PROT_TYPE {
	HC_PROT_NONE,
	HC_PROT_xSPI,
	HC_PROT_RAWNAND,
	HC_PROT_MMC,
};

enum HC_CHANNEL_TYPE {
	HC_CH_NONE,
	HC_CH_PAR,
	HC_CH_A,
	HC_CH_B,
};

enum HC_TYPE {
	HC_MXIC_UEFC,
	HC_MXIC_COCOTB,
	HC_STM32_L476,
};
enum HC_XFER_MODE_TYPE {
	HC_XFER_MODE_IO,
	HC_XFER_MODE_MAP,
	HC_XFER_MODE_DMA,
	MAX_HC_XFER_MODE
};

/**
 * b'[03:00] Data DTR   , b'[07:04] Data Bus Width
 * b'[11:08] Address DTR, b'[15:12] Address Bus Width
 * b'[19:16] Command DTR, b'[23:20] Command Bus Width
 **/
#define PROT_IO(c, a, d)	0x##c##0##a##0##d##0
#define PROT_DTR(c, a, d)	0x0##c##0##a##0##d
#define PROT_1S_1S_1S 		(PROT_IO(1, 1, 1) | PROT_DTR(0, 0, 0))
#define PROT_1S_1D_1D		(PROT_IO(1, 1, 1) | PROT_DTR(0, 1, 1))

#define PROT_1S_1S_2S		(PROT_IO(1, 1, 2) | PROT_DTR(0, 0, 0))
#define PROT_1S_2S_2S		(PROT_IO(1, 2, 2) | PROT_DTR(0, 0, 0))
#define PROT_1S_1D_2D		(PROT_IO(1, 1, 2) | PROT_DTR(0, 1, 1))
#define PROT_1S_2D_2D		(PROT_IO(1, 2, 2) | PROT_DTR(0, 1, 1))

#define PROT_1S_1S_4S		(PROT_IO(1, 1, 4) | PROT_DTR(0, 0, 0))
#define PROT_1S_4S_4S		(PROT_IO(1, 4, 4) | PROT_DTR(0, 0, 0))
#define PROT_4S_4S_4S		(PROT_IO(4, 4, 4) | PROT_DTR(0, 1, 1))
#define PROT_1S_1D_4D		(PROT_IO(1, 1, 4) | PROT_DTR(0, 1, 1))
#define PROT_1S_4D_4D		(PROT_IO(1, 4, 4) | PROT_DTR(0, 1, 1))
#define PROT_4S_4D_4D		(PROT_IO(4, 4, 4) | PROT_DTR(0, 1, 1))

#define PROT_1S_1S_8S		(PROT_IO(1, 1, 8) | PROT_DTR(0, 0, 0))
#define PROT_1S_8S_8S		(PROT_IO(1, 8, 8) | PROT_DTR(0, 0, 0))
#define PROT_8S_8S_8S		(PROT_IO(8, 8, 8) | PROT_DTR(0, 0, 0))
#define PROT_8D_8D_8D		(PROT_IO(8, 8, 8) | PROT_DTR(1, 1, 1))

#define OFS_PROT_BW_CMD		20
#define OFS_PROT_DTR_CMD	16
#define OFS_PROT_BW_ADDR	12
#define OFS_PROT_DTR_ADDR	8
#define OFS_PROT_BW_DATA	4
#define OFS_PROT_DTR_DATA	0

#define GET_PROT_DTR(x)		((x) & 0x010101)
#define GET_PROT_IO(x)		((x) & 0xf0f0f0)
#define ACCU_PROT_IO(x)		((((x) >> OFS_PROT_BW_CMD) & 0xf) | \
							(((x) >> OFS_PROT_BW_ADDR) & 0xf) | \
							(((x) >> OFS_PROT_BW_DATA) & 0xf))

#define CHECK_PROT_IO(_prot, c, a, d)	(GET_PROT_IO(_prot) == PROT_IO(c, a, d))
#define CHECK_PROT_8IO(_prot)			CHECK_PROT_IO(_prot, 8, 8, 8)

typedef struct _pkt {
	uint8_t cs_ctrl;
			/* CS assert and de-assert in a packet */
			#define CS_CTRL_REGULAR		0
			/* CS assert only */
			#define CS_CTRL_ASSERT		1
			/* CS keeps previous level */
			#define CS_CTRL_KEEP		2
			/* CS de-assert only */
			#define CS_CTRL_DEASSERT	3
	uint8_t pkt_ctrl;
			/* Execute the package transfer */
			#define PKT_CTRL_ENABLE		0
			/* Return the packages transfer */
			#define PKT_CTRL_END		1
			/* Skip the package transfer */
			#define PKT_CTRL_SKIP		2
	uint8_t dir:1,
			#define DIR_RD 0
			#define DIR_WR 1
			dqs:1,
			word_mode:1,
			poll_en:1;

	struct {
		uint32_t timeout_us;
		uint8_t mask;
		uint8_t exp;
		uint8_t ret_val;
	} poll;
	struct {
		uint32_t val;
		uint8_t len;
		uint8_t bw;
		uint8_t dtr:1;
	} cmd;
	struct {
		uint64_t val;
		uint8_t len;
		uint8_t bw;
		uint8_t dtr:1;
	} addr;
	struct {
		uint8_t len;
	} dummy;
	struct {
		uint8_t *buf;
		uint64_t len;
		uint8_t bw;
		uint8_t dtr:1;
	} data;
} pkt_t;

#define PKTS(_name, _pkt...) _name = (pkt_t[]){_pkt, {.pkt_ctrl = PKT_CTRL_END}}

struct _xfer_info {
	enum HC_TYPE hc_type;
	enum HC_PROT_TYPE hc_prot_type;

	uint32_t base_addr;
	uint32_t base_addr_map;
	uint32_t max_map_size;

	pkt_t *pkts;

	uint8_t *buf_cali;
	uint8_t initialized:1,		 	/* Device is initialized and ready */
			 hc_busy:1,		    	/* Host controller is in progress */
			 en_hc_para:1,			/* Enable host controller parallel mode */
			 en_flash_enhc:1,		/* Enable flash enhance mode */
			 is_cmd_cmd:1;

	/**
	 * Recommend to set caps_hc within the initialization function of each host controller driver.
	 **/
	struct {
		uint8_t dtr:1,
				word_mode:1,
				/**
				 * Support for 8/4/2/1 IOs
				 * For example:
				 * 1.Host controller suport 8IO, 4IO, 2IO, 1IO
				 *   - txrx_io should be set to SET_HC_CAPS_IO(8)
				 * 2.Host controller suport 4IO, 2IO, 1IO
				 *   - txrx_io should be set to SET_HC_CAPS_IO(4)
				 **/
				txrx_io:4;
				#define SET_HC_CAPS_IO(x)				((x) ? ((x) | ((x) - 1)) : 0)
				#define CHECK_HC_CAPS_IO(_xfer, x)		((x) <= _xfer->caps_hc.txrx_io)
				#define CHECK_HC_CAPS_DTR(_xfer)		(_xfer->caps_hc.dtr)
				#define CHECK_HC_CAPS_WORD_MODE(_xfer) 	(_xfer->caps_hc.word_mode)
	} caps_hc;

	struct {		
		int (*xfer)(xfer_info_t *xfer);
		void (*delay_us)(uint32_t us);
	} ops;

	void *ctrlr_priv;

	union {
		struct {
			uint32_t channel_conf;
		} uefc;
	};

};

int setup_platform(xfer_info_t *xfer, int ch_type, int port);
void release_platform(xfer_info_t *xfer);
int platform_xfer(xfer_info_t *xfer);

#endif
