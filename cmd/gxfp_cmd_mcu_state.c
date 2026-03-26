#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/timekeeping.h>

#include "gxfp_priv.h"
#include "../proto/gxfp_proto.h"
#include "gxfp_cmd.h"
#include "gxfp_constants.h"
#include "gxfp_cmd_mcu_state.h"

int gxfp_cmd_parse_mcu_state_payload(const __u8 *buf, size_t len,
					 struct gxfp_mcu_state *out)
{
	if (!buf || !out)
		return -EINVAL;

	memset(out, 0, sizeof(*out));
	if (len < GXFP_MCU_STATE_LEN)
		return -EBADMSG;

	out->version = buf[0];

	out->b1_raw = buf[1];
	out->is_pov_image_valid = (buf[1] >> 0) & 0x1;
	out->is_tls_connected = (buf[1] >> 1) & 0x1;
	out->is_tls_used = (buf[1] >> 2) & 0x1;
	out->is_locked = (buf[1] >> 3) & 0x1;

	memcpy(out->reserved, buf + 2, sizeof(out->reserved));

	return 0;
}

static int gxfp_do_query_mcu_state(struct gxfp_dev *gdev,
				  struct gxfp_mcu_state *out)
{
	__u8 payload[GXFP_MAX_MCU_STATE_PAYLOAD];
	__u8 trigger_payload[5];
	size_t payload_len = 0;
	int ret;
	struct timespec64 ts;
	__u16 ts_u16;

	if (!out)
		return -EINVAL;

	memset(out, 0, sizeof(*out));

	if (!gdev->hw.mailbox_mmio || gdev->hw.mailbox_size < GXFP_MAILBOX_MIN_SIZE) {
		return -ENODEV;
	}

	ktime_get_real_ts64(&ts);
	ts_u16 = (__u16)(((__u64)(ts.tv_sec % 60) * 1000ull) + ((__u64)ts.tv_nsec / 1000000ull));
	trigger_payload[0] = 0x55;
	trigger_payload[1] = (__u8)(ts_u16 & 0xff);
	trigger_payload[2] = (__u8)((ts_u16 >> 8) & 0xff);
	trigger_payload[3] = 0x00;
	trigger_payload[4] = 0x00;
	dev_dbg(gdev->dev, "MCU_STATE: trigger ts=0x%04x payload=%*ph\n",
		 ts_u16, (int)sizeof(trigger_payload), trigger_payload);

	ret = gxfp_cmd_xfer(gdev,
			   GXFP_CMD_TRIGGER_MCU_STATE, trigger_payload, sizeof(trigger_payload),
			   GXFP_CMD_QUERY_MCU_STATE,
			   payload, sizeof(payload), &payload_len,
			   GXFP_DEFAULT_RETRY_COUNT, GXFP_DEFAULT_TIMEOUT_US);
	if (ret) {
		return ret;
	}

	ret = gxfp_cmd_parse_mcu_state_payload(payload, payload_len, out);
	if (ret)
		return ret;

	dev_dbg(gdev->dev,
		"MCU_STATE: version=%u pov_valid=%u tls=%u locked=%u\n",
		out->version,
		out->is_pov_image_valid,
		out->is_tls_connected,
		out->is_locked);
	return 0;
}

int gxfp_cmd_query_mcu_state(struct gxfp_dev *gdev, struct gxfp_mcu_state *out)
{
	return gxfp_do_query_mcu_state(gdev, out);
}
