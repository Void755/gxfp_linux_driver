#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/unaligned.h>
#include <linux/string.h>

#include "gxfp_priv.h"
#include "gxfp_cmd.h"
#include "../hw/gxfp_delay.h"
#include "gxfp_constants.h"
#include "gxfp_cmd_version.h"

static bool gxfp_extract_ascii_version(const __u8 *data, char *out, size_t out_len)
{
	size_t j;

	if (!out || out_len == 0)
		return false;
	out[0] = '\0';

	if (!data)
		return false;

	for (j = 0; (j + 1) < out_len; j++) {
		__u8 ch = data[j];
		if (ch == 0 || ch < 0x20 || ch > 0x7e)
			break;
		out[j] = (char)ch;
	}
	out[j] = '\0';

	return j != 0;
}

static int gxfp_do_get_version(struct gxfp_dev *gdev,
                               struct gxfp_get_version_result *out)
{
	static const __u8 req_payload[] = { 0x01 };
	__u8 payload[GXFP_MAX_VERSION_RAW];
	size_t rx_len = 0;
	int ret;

	memset(out, 0, sizeof(*out));
	out->status = (__u32)-EIO;
	out->gpio_status = (__u32)-EIO;
	out->tx_status = (__u32)-EIO;
	out->rx_status = (__u32)-EIO;

	if (!gdev->hw.mailbox_mmio || gdev->hw.mailbox_size < GXFP_MAILBOX_MIN_SIZE) {
		out->status = (__u32)-ENODEV;
		out->gpio_status = (__u32)-ENODEV;
		out->tx_status = (__u32)-ENODEV;
		out->rx_status = (__u32)-ENODEV;
		return -ENODEV;
	}

	ret = gxfp_cmd_xfer(gdev,
			   GXFP_CMD_FIRMWARE_VERSION,
			   req_payload,
			   sizeof(req_payload),
			   GXFP_CMD_FIRMWARE_VERSION,
			   payload,
			   sizeof(payload),
			   &rx_len,
			   GXFP_DEFAULT_RETRY_COUNT,
		   GXFP_DEFAULT_TIMEOUT_US);
	out->tx_status = 0;
	out->gpio_status = 0;
	out->rx_status = (__u32)(ret ? ret : 0);
	if (ret) {
		out->status = (__u32)ret;
		return ret;
	}

	out->bytes_read = (__u32)min_t(size_t, rx_len, sizeof(out->raw));
	memset(out->raw, 0, sizeof(out->raw));
	memcpy(out->raw, payload, out->bytes_read);
	dev_dbg(gdev->dev,
		 "GET_VERSION: payload_len=%zu head16=%*ph\n",
		 rx_len, 16, out->raw);

	(void)gxfp_extract_ascii_version(payload,
					 out->version_ascii,
					 sizeof(out->version_ascii));
	if (out->version_ascii[0] == '\0') {
		out->status = (__u32)-ENODATA;
		return 0;
	}

	out->status = 0;
	return 0;
}

int gxfp_cmd_get_version(struct gxfp_dev *gdev, struct gxfp_get_version_result *out)
{
	return gxfp_do_get_version(gdev, out);
}