#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>

#include "gxfp_priv.h"
#include "gxfp_cmd.h"
#include "../hw/gxfp_delay.h"
#include "gxfp_constants.h"

#include "gxfp_cmd_reset.h"

int gxfp_cmd_notify_power_state(struct gxfp_dev *gdev,
				__u8 state)
{
	__u8 payload[1];
	__u8 ack_payload[8];
	size_t ack_len = 0;
	int rc;

	if (!gdev)
		return -EINVAL;

	payload[0] = state;
	rc = gxfp_cmd_xfer(gdev,
			  GXFP_CMD_NOTIFY_POWER_STATE,
			  payload, sizeof(payload),
			  GXFP_CMD_ACK,
			  ack_payload, sizeof(ack_payload), &ack_len,
			  1,
			  GXFP_DEFAULT_TIMEOUT_US);

	dev_dbg(gdev->dev,
		 "NOTIFY_POWER_STATE: cmd=0x0E state=%u rc=%d ack_len=%zu\n",
		 state, rc, ack_len);
	return rc;
}

int gxfp_cmd_sleep_cleanup(struct gxfp_dev *gdev, unsigned int post_delay_ms)
{
	static const __u8 sleep_payload[2] = { 0x00, 0x00 };
	__u8 ack_payload[8];
	size_t ack_len = 0;
	int rc;

	if (!gdev)
		return -EINVAL;

	rc = gxfp_cmd_xfer(gdev,
			  GXFP_CMD_SLEEP_CLEANUP,
			  sleep_payload, sizeof(sleep_payload),
			  GXFP_CMD_ACK,
			  ack_payload, sizeof(ack_payload), &ack_len,
			  1,
			  GXFP_DEFAULT_TIMEOUT_US);
	dev_dbg(gdev->dev, "SLEEP/CLEANUP: cmd=0xD2 rc=%d ack_len=%zu\n", rc, ack_len);

	if (post_delay_ms)
		msleep(post_delay_ms);

	return rc;
}

int gxfp_cmd_reset_device(struct gxfp_dev *gdev,
			  __u8 reset_flags)
{
	__u8 payload[2];
	__u8 ack_payload[8];
	size_t ack_len = 0;
	int rc;

	if (!gdev)
		return -EINVAL;

	payload[0] = reset_flags;
	payload[1] = 0x14;

	rc = gxfp_cmd_xfer(gdev,
			  GXFP_CMD_RESET_DEVICE,
			  payload, sizeof(payload),
			  GXFP_CMD_ACK,
			  ack_payload, sizeof(ack_payload), &ack_len,
			  1,
			  GXFP_DEFAULT_TIMEOUT_US);
	dev_dbg(gdev->dev,
		 "RESET_DEVICE: cmd=0xA1 flags=0x%02x rc=%d ack_len=%zu\n",
		 reset_flags, rc, ack_len);
	return rc;
}

int gxfp_cmd_recover_session(struct gxfp_dev *gdev, bool unstick_tls)
{
	int rc = 0;
	int r;
	unsigned int tries;

	if (!gdev)
		return -EINVAL;

	/* D0Exit notifies MCU power state */
	for (tries = 0; tries < 3; tries++) {
		r = gxfp_cmd_notify_power_state(gdev, 1);
		if (!r)
			break;
		if ((tries + 1) < 3)
			msleep(10);
	}
	if (r && !rc)
		rc = r;
	gxfp_busy_wait_us(GXFP_RESET_DELAY_US);

	if (unstick_tls) {
		r = gxfp_cmd_protocol_init(gdev);
		if (r && !rc)
			rc = r;
		gxfp_busy_wait_us(1000);

		r = gxfp_cmd_tls_unlock(gdev);
		if (r && !rc)
			rc = r;
		gxfp_busy_wait_us(1000);
	}

	r = gxfp_cmd_sleep_cleanup(gdev, 200);
	if (r && !rc)
		rc = r;

	for (tries = 0; tries < 6; tries++) {
		r = gxfp_cmd_reset_device(gdev, 0x02);
		if (!r)
			break;
		if ((tries + 1) < 6)
			gxfp_busy_wait_us(500000);
	}
	if (r && !rc)
		rc = r;
	gxfp_busy_wait_us(GXFP_RESET_DELAY_US);

	dev_dbg(gdev->dev, "RECOVER_SESSION: unstick_tls=%u rc=%d\n", unstick_tls ? 1 : 0, rc);
	return rc;
}

int gxfp_cmd_tls_unlock(struct gxfp_dev *gdev)
{
	__u8 payload[2] = { 0x00, 0x00 };
	__u8 ack_payload[8];
	size_t ack_len = 0;
	int rc;

	if (!gdev)
		return -EINVAL;

	rc = gxfp_cmd_xfer(gdev,
			  GXFP_CMD_TLS_UNLOCK,
			  payload, sizeof(payload),
			  GXFP_CMD_ACK,
			  ack_payload, sizeof(ack_payload), &ack_len,
			  1,
			  GXFP_DEFAULT_TIMEOUT_US);

	dev_dbg(gdev->dev, "TLS_UNLOCK: cmd=0xD4 rc=%d ack_len=%zu\n", rc, ack_len);
	return rc;
}

int gxfp_cmd_protocol_init(struct gxfp_dev *gdev)
{
	__u8 payload[4] = { 0x00, 0x00, 0x00, 0x00 };
	__u8 ack_payload[8];
	size_t ack_len = 0;
	int rc;

	if (!gdev)
		return -EINVAL;

	rc = gxfp_cmd_xfer(gdev,
			  GXFP_CMD_PROTOCOL_INIT,
			  payload, sizeof(payload),
			  GXFP_CMD_ACK,
			  ack_payload, sizeof(ack_payload), &ack_len,
			  1,
			  GXFP_DEFAULT_TIMEOUT_US);

	dev_dbg(gdev->dev, "PROTO_INIT: cmd=0x01 rc=%d ack_len=%zu\n", rc, ack_len);
	return rc;
}
