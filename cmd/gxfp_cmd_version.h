#ifndef __GXFP_CMD_VERSION_H
#define __GXFP_CMD_VERSION_H

#include <linux/types.h>
#include "gxfp_constants.h"

struct gxfp_dev;

struct gxfp_get_version_result {
	__u32 status;
	__u32 gpio_status;
	__u32 tx_status;
	__u32 rx_status;
	__u32 bytes_read;
	char version_ascii[GXFP_MAX_VERSION_ASCII];
	__u8 raw[GXFP_MAX_VERSION_RAW];
};

int gxfp_cmd_get_version(struct gxfp_dev *gdev, struct gxfp_get_version_result *out);

#endif /* __GXFP_CMD_VERSION_H */