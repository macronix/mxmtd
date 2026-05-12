#include "platform_itf.h"
#include "platform_print.h"
#include "mxmtd.h"
#include "mxmtd_err.h"

#define SPINOR_RDID					0x9F		/* Read Identification */
#define RAWNAND_RDID				0x90

/* example for read id of SPINOR and RAWNAND */

static int spinor_read_id(xfer_info_t *xfer, uint8_t *id, uint8_t len_id)
{
	int ret;

	PKTS(xfer->pkts, {
			.cmd = {.val = SPINOR_RDID, .len = 1, .bw = 1},
			.data = {.buf = id, .len = len_id, .bw = 1},
			.dir = DIR_RD
	});
	ret = platform_xfer(xfer);
	mxic_pr_inf("ID: %02X%02X%02X\r\n", id[0], id[1], id[2]);

	return ret;
}

static int rawnand_read_id(xfer_info_t *xfer, uint32_t addr, uint8_t len_addr, uint8_t *id, uint8_t len_id)
{
	int ret;

	PKTS(xfer->pkts, {
			.cmd = {.val = RAWNAND_RDID, .len = len_addr, .bw = 8},
			.addr = {.val = addr, .len = 1, .bw = 8},
			.data = {.buf = id, .len = len_id, .bw = 8},
			.dir = DIR_RD
	});
	ret = platform_xfer(xfer);
	mxic_pr_inf("ID: %02X%02X%02X%02X%02X\r\n", id[0], id[1], id[2], id[3], id[4]);

	return ret;
}

int spinor_pass_through()
{
	int ret;
	xfer_info_t xfer = {.hc_type = HC_MXIC_UEFC, .hc_prot_type = HC_PROT_xSPI};
	uint8_t id[3] = {};

	ret = setup_platform(&xfer, CH_A, 0);
	if (MXST_SUCCESS == ret) {
		ret = spinor_read_id(&xfer, id, sizeof(id));
	}

	release_platform(&xfer);

	return ret;
}

int rawnand_pass_through()
{
	int ret;
	xfer_info_t xfer = {.hc_type = HC_MXIC_UEFC, .hc_prot_type = HC_PROT_RAWNAND};
	uint8_t id[5] = {};

	ret = setup_platform(&xfer, CH_B, 0);
	if (MXST_SUCCESS == ret) {
		ret = rawnand_read_id(&xfer, 0, 1, id, sizeof(id));
	}

	release_platform(&xfer);

	return ret;
}
