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

#ifndef _MXMTD_ERR_H_
#define _MXMTD_ERR_H_

/* Range of common error code : [0x00000fff: 0x00000000]*/

#define MXST_ERR_MXMTD(x)			(0x00000000U | (x))
#define MXST_SUCCESS				MXST_ERR_MXMTD(0x00000000U)
#define MXST_ERR_ERASE				MXST_ERR_MXMTD(0x00000001U)
#define MXST_ERR_READ				MXST_ERR_MXMTD(0x00000002U)
#define MXST_ERR_PROGRAM			MXST_ERR_MXMTD(0x00000003U)
#define MXST_ERR_NOT_SUP			MXST_ERR_MXMTD(0x00000004U)
#define MXST_ERR_NOT_MATCH			MXST_ERR_MXMTD(0x00000005U)
#define MXST_ERR_TIMEOUT			MXST_ERR_MXMTD(0x00000006U)
#define MXST_ERR_PTR_NULL			MXST_ERR_MXMTD(0x00000007U)
#define MXST_ERR_CONT				MXST_ERR_MXMTD(0x00000008U)
#define MXST_ERR_CMP				MXST_ERR_MXMTD(0x00000009U)
#define MXST_ERR_ALLOC				MXST_ERR_MXMTD(0x0000000aU)
#define MXST_ERR_NOT_ALIGN			MXST_ERR_MXMTD(0x0000000bU)
#define MXST_ERR_ECC				MXST_ERR_MXMTD(0x0000000cU)
#define MXST_ERR_PARAM				MXST_ERR_MXMTD(0x0000000dU)
#define MXST_ERR_ECC_CORR			MXST_ERR_MXMTD(0x0000000eU)
#define MXST_ERR_SYSTEM				MXST_ERR_MXMTD(0x0000000fU)
#define MXST_ERR_CALI				MXST_ERR_MXMTD(0x00000010U)
#define MXST_ERR_MEM_OP				MXST_ERR_MXMTD(0x00000011U)
#define MXST_ERR_PLATFORM			MXST_ERR_MXMTD(0x00000012U)
#define MXST_ERR_NO_GB				MXST_ERR_MXMTD(0x00000013U)
#define MXST_ERR_BUF_MISMATCH		MXST_ERR_MXMTD(0x00000014U)
#define MXST_ERR_LOCK				MXST_ERR_MXMTD(0x00000016U)
#define MXST_ERR_UNLOCK				MXST_ERR_MXMTD(0x00000017U)

/* Range of SPINOR specific error code : [0x00001fff: 0x00001000]*/
/* Range of SPINAND specific error code : [0x00002fff: 0x00002000]*/
/* Range of ONFINAND specific error code : [0x00003fff: 0x00003000] */

#endif
