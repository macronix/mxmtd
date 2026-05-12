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

#ifndef _SPINOR_CMD_H_
#define _SPINOR_CMD_H_

typedef struct _pkt pkt_t;

enum SPINOR_MODE {
	SPINOR_MODE_SPI,
	SPINOR_MODE_QPI,
	SPINOR_MODE_SOPI,
	SPINOR_MODE_DOPI,
	MAX_SPINOR_MODE
};

typedef struct {
	uint32_t cmd;
	uint8_t len_addr;
	uint32_t prot[MAX_SPINOR_MODE];
	uint16_t dc[MAX_SPINOR_MODE];
	uint8_t dqs:1,
			word_mode:1;
} cmd_info_t;

typedef struct _cmd_meta{
	void *priv;
	int spinor_mode;
	uint8_t mask_cmd;
	uint32_t prot_reg; /* protocol for register operation */
	cmd_info_t *cmd_info;
	struct {
		uint32_t idx_cmd_num;
	} rd;
	struct {
		uint32_t idx_cmd_num;
	} pgm;
	struct {
		uint32_t idx_cmd_num;
	} ers;
} cmd_meta_t;


/**********************        1.ID commands             *************************/
#define SPINOR_RDID					0x9F		/* Read Identification */
#define SPINOR_RES					0xAB
#define SPINOR_REMS					0x90
#define SPINOR_REMS2				0xEF
#define SPINOR_REMS4				0xDF
#define SPINOR_REMS4D				0xCF
#define SPINOR_QPIID				0xAF

/**********************         2.Register commands       *************************/
#define SPINOR_WRSR					0x01		/* Write Status Register and Configuration Register */
#define SPINOR_RDSR					0x05		/* Status read command */
#define SPINOR_WRSCUR				0x2F
#define SPINOR_RDSCUR				0x2B
#define SPINOR_WRCR					0x01		/* Write Configuration Register */
#define SPINOR_RDCR					0x15		/* Read Configuration Register */
#define SPINOR_CLSR					0x30
#define SPINOR_WRCR2				0x72		/* Write Configuration Register 2 */
#define SPINOR_RDCR2				0x71		/* Read Configuration Register 2 */
#define SPINOR_RDEAR				0xC8
#define SPINOR_WREAR				0xC5
#define SPINOR_RDFBR				0x16
#define SPINOR_WRFBR				0x17
#define SPINOR_ESFBR				0x18
#define SPINOR_WRLR					0x2C
#define SPINOR_RDLR  				0x2D
#define SPINOR_WRPASS				0x28
#define SPINOR_RDPASS				0x27
#define SPINOR_PASSULK				0x29
#define SPINOR_RDSPB				0xE2
#define SPINOR_WRSPB				0xE3
#define SPINOR_ERSSPB				0xE4
#define SPINOR_SPBLK				0xA6
#define SPINOR_RDSPBLK				0xA7
#define SPINOR_WRDPB				0xE1
#define SPINOR_RDDPB				0xE0
#define SPINOR_RDFSR				0x44
#define SPINOR_RDSFDP				0x5A
#define SPINOR_RDDMC				0x5B
#define SPINOR_CFIRD				0x5C

/**********************         3.Read commands           *************************/
#define SPINOR_READ					0x03
#define SPINOR_READ4B				0x13
#define SPINOR_FREAD				0x0B
#define SPINOR_FREAD4B				0x0C
#define SPINOR_FDTRD				0x0D
#define SPINOR_FDTRD4B				0x0E
#define SPINOR_DREAD				0x3B		/* Dual Output Fast Read */
#define SPINOR_DREAD4B				0x3C		/* Dual Output Fast Read */
#define SPINOR_2READ				0xBB		/* Dual IO Fast Read */
#define SPINOR_2READ4B				0xBC		/* Dual IO Fast Read */
#define SPINOR_2DTRD				0xBD
#define SPINOR_2DTRD4B				0xBE

#define SPINOR_QREAD				0x6B		/* Quad Output Fast Read */
#define SPINOR_QREAD4B				0x6C		/* Quad Output Fast Read */
#define SPINOR_W4READ				0xE7
#define SPINOR_4READ                0xEB		/* Quad IO Fast Read */
#define SPINOR_4READ4B				0xEC		/* Quad IO Fast Read */
#define SPINOR_4READ_TOP			0xEA		/* Quad IO Fast Read */
#define SPINOR_4DTRD				0xED
#define SPINOR_4DTRD4B				0xEE
#define SPINOR_ORAED                0x8B
#define SPINOR_ORAED4B              0x7C
#define SPINOR_8READ				0xEC		/* SOPI Read */
#define SPINOR_8DTRD				0xEE		/* DOPI Read */

#define SPINOR_RDBUF				0x25

/**********************         4.Program commands        *************************/
#define	SPINOR_WREN					0x06		/* Write Enable command */
#define	SPINOR_WRDI					0x04
#define SPINOR_PP					0x02		/* Page Program command */
#define SPINOR_4PP					0x38		/* Quad Input Fast Program */
#define SPINOR_QPP					0x32
#define SPINOR_QPP4B				0x34
#define SPINOR_PP4B					0x12		/* Page Program command */
#define SPINOR_4PP4B				0x3E		/* Quad Input Fast Program */
#define SPINOR_CP					0xAD
#define SPINOR_WRBI					0x22
#define SPINOR_WRCT					0x24
#define SPINOR_WRCF					0x31

/**********************         5.Erase commands          *************************/

#define SPINOR_SE					0x20
#define SPINOR_SE4B					0x21
#define SPINOR_BE32K				0x52
#define SPINOR_BE32K4B				0x5C
#define SPINOR_BE					0xD8
#define SPINOR_BE4B					0xDC
#define SPINOR_CE					0xC7		/* Bulk Erase command */

/**********************         6.Mode setting commands    *************************/
#define SPINOR_DP					0xB9
#define SPINOR_RDP					0xAB
#define SPINOR_ENSO					0xB1
#define SPINOR_EXSO					0xC1
#define SPINOR_ENSA					0xB1
#define SPINOR_EXSA					0xC1
#define SPINOR_ESRY					0x70
#define SPINOR_DSRY					0x80
#define SPINOR_EQPI					0x35		/* Enable QPI Mode */
#define SPINOR_WPSEL				0x68
#define SPINOR_SBL					0xC0
#define SPINOR_EN4B					0xB7
#define SPINOR_EX4B					0xE9
#define SPINOR_FMEN					0x41

/**********************         7.Reset commands          *************************/

#define SPINOR_RSTEN				0x66
#define SPINOR_RST					0x99
#define SPINOR_RSTQPI				0xF5		/* Exit QPI Mode */

/**********************         8.Security commands       *************************/

#define SPINOR_GBLK					0x7E
#define SPINOR_GBULK				0x98
#define SPINOR_SBLK					0x36
#define SPINOR_SBULK				0x39
#define SPINOR_RDBLOCK				0x3C
#define SPINOR_RDBLOCK2				0xFB
#define SPINOR_RDPLOCK				0x3F

/**********************         9.Suspend/Resume commands  *************************/

#define SPINOR_PGMERS_SUSPEND		0xB0
#define SPINOR_PGMERS_RESUME		0x30
#define SPINOR_NOP					0x00

#define ADDR_CR2_OPI                0x00000000
    #define DATA_CR2_OPI_DTR_EN     0x02
    #define DATA_CR2_OPI_STR_EN     0x01
    #define DATA_CR2_OPI_SPI_EN     0x00
#define ADDR_CR2_DQS                0x00000200
    #define DATA_CR2_DQS_STR_EN     0x02
    #define DATA_CR2_DQS_DTR_PRE    0x01
#define ADDR_CR2_DC                 0x00000300

void template_read(pkt_t *pkt, uint32_t prot, uint64_t addr, uint8_t len_addr,
		uint8_t len_dmy, uint8_t *buf, uint64_t nbytes, uint8_t dqs, uint8_t wm);
void template_write(pkt_t *pkt, uint32_t prot, uint64_t addr, uint8_t len_addr,
		uint8_t *buf, uint64_t nbytes, uint8_t wm);

int cmd_spinor_rdid(cmd_meta_t *cmd_meta, uint8_t *id);
int cmd_spinor_rdcr(cmd_meta_t *cmd_meta, uint8_t *cr);
int cmd_spinor_wrcr(cmd_meta_t *cmd_meta, uint8_t *cr);
int cmd_spinor_rdsr(cmd_meta_t *cmd_meta, uint8_t *sr);
int cmd_spinor_rdlr(cmd_meta_t *cmd_meta, uint8_t *lr);
int cmd_spinor_wrlr(cmd_meta_t *cmd_meta, uint8_t *lr);
int cmd_spinor_rddpb(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *dpb);
int cmd_spinor_wrdpb(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *dpb);
int cmd_spinor_rdspb(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *spb);
int cmd_spinor_wrspb(cmd_meta_t *cmd_meta, uint32_t addr);
int cmd_spinor_ersspb(cmd_meta_t *cmd_meta);
int cmd_spinor_wpsel(cmd_meta_t *cmd_meta);
int cmd_spinor_rdscur(cmd_meta_t *cmd_meta, uint8_t *scur);
int cmd_spinor_wrscur(cmd_meta_t *cmd_meta, uint8_t *scur);
int cmd_spinor_wrsr(cmd_meta_t *cmd_meta, uint8_t *sr, uint8_t len_sr);
int cmd_spinor_rdcr2(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *cr2);
int cmd_spinor_wrcr2(cmd_meta_t *cmd_meta, uint32_t addr, uint8_t *cr2);
int cmd_spinor_en4b(cmd_meta_t *cmd_meta);
int cmd_spinor_rsten(cmd_meta_t *cmd_meta);
int cmd_spinor_rst(cmd_meta_t *cmd_meta);
int cmd_spinor_wren(cmd_meta_t *cmd_meta);
int cmd_spinor_eqpi(cmd_meta_t *cmd_meta);
int cmd_spinor_rstqpi(cmd_meta_t *cmd_meta);
int cmd_spinor_rdsfdp(cmd_meta_t *cmd_meta, uint32_t addr, void *sfdp, uint32_t nbytes);


int cmd_spinor_read(cmd_meta_t *cmd_meta, uint64_t addr, uint8_t *buf, uint64_t nbytes);
int cmd_spinor_program(cmd_meta_t *cmd_meta, uint64_t addr, uint8_t *buf, uint64_t nbytes);
int cmd_spinor_erase(cmd_meta_t *cmd_meta, uint64_t addr);

int cmd_spinor_preamble_calibration(cmd_meta_t *cmd_meta);

#endif
