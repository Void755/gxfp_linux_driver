#include <linux/kernel.h>
#include <linux/errno.h>

#include "gxfp_priv.h"
#include "gxfp_cmd.h"
#include "gxfp_constants.h"
#include "gxfp_cmd_mcu_config.h"

int gxfp_cmd_upload_config_mcu(struct gxfp_dev *gdev, const __u8 *cfg, size_t cfg_len)
{
	__u8 ack_payload[32];
	size_t ack_len = 0;

	if (!gdev)
		return -EINVAL;
	if (!cfg || cfg_len == 0)
		return -EINVAL;

	return gxfp_cmd_xfer(gdev,
				   GXFP_CMD_UPLOAD_CONFIG_MCU,
				   cfg, cfg_len,
				   GXFP_CMD_ACK,
				   ack_payload, sizeof(ack_payload),
				   &ack_len,
				   GXFP_DEFAULT_RETRY_COUNT,
				   GXFP_DEFAULT_TIMEOUT_US);
}
