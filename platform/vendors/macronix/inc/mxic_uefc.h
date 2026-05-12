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

#ifndef _MXIC_UEFC_H_
#define _MXIC_UEFC_H_

typedef struct _xfer_info xfer_info_t;// forward declaration

int setup_mxic_uefc(xfer_info_t *xfer, int ch_type, int port);
void release_mxic_uefc(xfer_info_t *xfer);

#endif /* SRC_PLATFORM_VENDORS_XILINX_ZYNQ7000_INC_MXIC_UEFC_H_ */
