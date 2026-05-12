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

#ifndef _MXIC_UEFC_ERR_H_
#define _MXIC_UEFC_ERR_H_

/* Range of UEFC specific error code : [0x00010fff: 0x00010000]*/

#include "mxmtd_err.h"

#define _SPINOR_ERR_H_

#define MXST_ERR_UEFC(x)        (0x00010000U | x)
#define MXST_ERR_HC_BUSY        MXST_ERR_UEFC(0x00000000U)

#endif
