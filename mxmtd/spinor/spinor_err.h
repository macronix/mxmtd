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

#ifndef _SPINOR_ERR_H_

/* Range of SPINOR specific error code : [0x00001fff: 0x00001000]*/

#include "mxmtd_err.h"

#define _SPINOR_ERR_H_

#define MXST_ERR_SPINOR(x)      (0x00001000U | x)
#define MXST_ERR_CONFIG         MXST_ERR_SPINOR(0x00000000U)
#define MXST_ERR_CMD_LIST       MXST_ERR_SPINOR(0x00000001U)
#define MXST_ERR_ID_INFO        MXST_ERR_SPINOR(0x00000002U)
#define MXST_ERR_SCAN           MXST_ERR_SPINOR(0x00000003U)
#define MXST_ERR_SPINOR_MODE    MXST_ERR_SPINOR(0x00000004U)
#define MXST_ERR_QUAD_EN		MXST_ERR_SPINOR(0x00000005U)
#define MXST_ERR_WPSEL			MXST_ERR_SPINOR(0x00000006U)
#define MXST_ERR_IS_LOCK		MXST_ERR_SPINOR(0x00000007U)
#define MXST_ERR_ASP			MXST_ERR_SPINOR(0x00000008U)

#endif
