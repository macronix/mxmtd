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
#include "platform_print.h"
#include "platform_itf.h"
#include "mxmtd.h"
#include "mxmtd_err.h"
#include "mxmtd_config.h"

int mxmtd_setup_dev(mxmtd_t **mxmtd_ret, enum FLASH_TYPE flash_type, enum CHANNEL_TYPE ch_type, int port)
{
    int ret;
    mxmtd_t *mxmtd;
    enum HC_CHANNEL_TYPE hc_ch_type;

    mxic_pr_inf("\r\n************ [MxMTD Version: %X] ************\r\n", MXMTD_VER);

    mxmtd = calloc(1, sizeof(mxmtd_t));
    if (!mxmtd) {
    	mxic_pr_err("Allocate for mxmtd failed!\r\n");
    	return MXST_ERR_ALLOC;
    }

    /* Setup Flash */
    switch (flash_type) {
#ifdef CONF_SPINOR
    case FLASH_SPINOR:
    	mxic_pr_inf("Setup SPI NOR Flash...\r\n");
        ret = setup_spinor(mxmtd, CONF_ERS_SIZE);
        break;
#endif

#ifdef CONF_RAWNAND
    case FLASH_RAWNAND:
    	mxic_pr_inf("Setup RAW NAND Flash...\r\n");
        ret = setup_rawnand(mxmtd, CONF_RAWNAND_ECC_TYPE, CONF_RAWNAND_ECC_STEP_SIZE_DEFAULT);
        break;
#endif

#ifdef CONF_SPINAND
    case FLASH_SPINAND:
    	mxic_pr_inf("Setup SPI NAND Flash...\r\n");
        ret = setup_spinand(mxmtd, CONF_SPINAND_ECC_TYPE, CONF_SPINAND_ECC_STEP_SIZE_DEFAULT);
        break;
#endif
    //TODO: MMC
    default:
    	mxic_pr_err("The Flash type(%d) is not supported\r\n", flash_type);
        ret = MXST_ERR_NOT_SUP;
        break;

    }
    if (MXST_SUCCESS != ret) {
    	goto release_setup_dev;
    }

    /* Set-up Host controller */
    mxic_pr_dbg("Setup platform...\r\n");
    switch (ch_type) {
    case CH_PAR:
    	mxic_pr_inf("Platform: Parallel mode, port: %d\r\n", port);
    	hc_ch_type = HC_CH_PAR;
    	break;
    case CH_A:
    	mxic_pr_inf("Platform: Channel A, Port: %d\r\n", port);
		hc_ch_type = HC_CH_A;
		break;
    case CH_B:
    	mxic_pr_inf("Platform: Channel B, Port: %d\r\n", port);
		hc_ch_type = HC_CH_B;
		break;
    default:
    	hc_ch_type = HC_CH_NONE;
    	break;
    }

    ret = setup_platform(mxmtd->priv, hc_ch_type, port);
    if (MXST_SUCCESS != ret) {
    	goto release_setup_dev;
    }

    /* Obtain flash information */
    mxic_pr_dbg("Probe flash...\r\n");
    ret = mxmtd->ops.probe(mxmtd);
    if (MXST_SUCCESS != ret) {
    	goto release_setup_dev;
    }

    mxmtd->is_le = is_le();

    *mxmtd_ret = mxmtd;

    return MXST_SUCCESS;

release_setup_dev:
	mxmtd_release(&mxmtd);
	return ret;
}

void mxmtd_release(mxmtd_t **mxmtd)
{
	if (*mxmtd) {
		if ((*mxmtd)->priv) {
			release_platform((*mxmtd)->priv);
		}
		if ((*mxmtd)->ops.release) {
			(*mxmtd)->ops.release(*mxmtd);
		}
		free(*mxmtd);
		*mxmtd = 0;
	}
}
