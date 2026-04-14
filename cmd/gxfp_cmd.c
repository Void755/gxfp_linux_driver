#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "gxfp_cmd.h"
#include "../proto/gxfp_goodix_proto.h"
#include "../proto/gxfp_mp_proto.h"
#include "../transport/gxfp_espi_tx.h"
#include "../hw/gxfp_delay.h"
#include "gxfp_priv.h"
#include "gxfp_constants.h"
#include "../driver/gxfp_trace.h"

int gxfp_xfer(struct gxfp_dev *gdev, const struct gxfp_xfer_req *req)
{
	__u8 *tx;
	__u8 *body;
	__u8 *rx;
	size_t rx_len = 0;
	unsigned int t;
	int body_len;
	int frame_len;
	int ret;

	if (!gdev || !req)
		return -EINVAL;
	if (req->req_payload_len && !req->req_payload)
		return -EINVAL;
	if (req->resp_payload && req->resp_payload_cap == 0)
		return -EINVAL;
	if (!req->resp_payload && req->resp_payload_cap != 0)
		return -EINVAL;

	tx = kmalloc(GXFP_TX_BUFFER_SIZE, GFP_KERNEL);
	if (!tx)
		return -ENOMEM;
	body = kmalloc(GXFP_TX_BUFFER_SIZE, GFP_KERNEL);
	if (!body) {
		ret = -ENOMEM;
		goto out_free_tx;
	}

	rx = kmalloc(GXFP_MAILBOX_MIN_SIZE, GFP_KERNEL);
	if (!rx) {
		ret = -ENOMEM;
		goto out_free_body;
	}

	body_len = gxfp_goodix_build_frame(req->req_cmd,
				   req->req_payload, req->req_payload_len,
				   body, GXFP_TX_BUFFER_SIZE);
	if (body_len < 0) {
		ret = body_len;
		goto out_free_rx;
	}

	frame_len = gxfp_mp_build_frame(GXFP_MP_TYPE_A,
				 body, (size_t)body_len,
				 tx, GXFP_TX_BUFFER_SIZE);
	if (frame_len < 0) {
		ret = frame_len;
		goto out_free_rx;
	}
	gxfp_trace_logf("cmd_req req=0x%02x expect=0x%02x tries=%u tx_mp_len=%d",
		req->req_cmd, req->expect_cmd, req->tries ? req->tries : 1, frame_len);

	for (t = 0; t < (req->tries ? req->tries : 1); t++) {
		struct gxfp_mp_frame_parsed mp;
		struct gxfp_frame_parsed p;

		ret = gxfp_espi_xfer(gdev, tx, (size_t)frame_len,
				    rx, GXFP_MAILBOX_MIN_SIZE, &rx_len);
		if (ret) {
			gxfp_trace_logf("cmd_retry req=0x%02x expect=0x%02x try=%u rc=%d",
				req->req_cmd, req->expect_cmd, t + 1, ret);
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}

		if (!gxfp_parse_mp_frame(rx,
					 min_t(size_t, rx_len, (size_t)GXFP_MAILBOX_MIN_SIZE),
					 &mp)) {
			gxfp_trace_logf("cmd_bad_mp req=0x%02x expect=0x%02x try=%u rx_len=%zu",
				req->req_cmd, req->expect_cmd, t + 1, rx_len);
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}
		if (!gxfp_parse_goodix_body(mp.payload, mp.payload_len, &p) || !p.valid) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}
		if (req->require_proto_ck_ok && !p.proto_checksum_ok) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}
		if (p.cmd != req->expect_cmd) {
			gxfp_trace_logf("cmd_unexpected req=0x%02x expect=0x%02x got=0x%02x try=%u",
				req->req_cmd, req->expect_cmd, p.cmd, t + 1);
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}

		if (req->resp_payload) {
			if (p.payload_len > req->resp_payload_cap) {
				ret = -EMSGSIZE;
				goto out_free_rx;
			}
			memcpy(req->resp_payload, p.payload, p.payload_len);
		}

		if (req->out_resp_payload_len)
			*req->out_resp_payload_len = p.payload_len;
		if (req->out_frame) {
			*req->out_frame = p;
			req->out_frame->payload = req->resp_payload;
		}
		ret = 0;
		gxfp_trace_logf("cmd_ok req=0x%02x expect=0x%02x try=%u payload=%u",
			req->req_cmd, req->expect_cmd, t + 1, p.payload_len);
		goto out_free_rx;
	}

	ret = -ETIMEDOUT;
	gxfp_trace_logf("cmd_timeout req=0x%02x expect=0x%02x tries=%u",
		req->req_cmd, req->expect_cmd, req->tries ? req->tries : 1);

out_free_rx:
	kfree(rx);
out_free_body:
	kfree(body);
out_free_tx:
	kfree(tx);
	return ret;
}

int gxfp_cmd_xfer(struct gxfp_dev *gdev,
			 __u8 req_cmd,
			 const __u8 *req_payload,
			 size_t req_payload_len,
			 __u8 expect_cmd,
			 __u8 *resp_payload,
			 size_t resp_payload_cap,
			 size_t *out_resp_payload_len,
			 unsigned int tries,
			 unsigned int retry_delay_us)
{
	struct gxfp_frame_parsed p;
	int ret;

	if (!resp_payload || resp_payload_cap == 0)
		return -EINVAL;

	memset(&p, 0, sizeof(p));
	ret = gxfp_xfer(gdev, &(const struct gxfp_xfer_req){
		.req_cmd		= req_cmd,
		.req_payload		= req_payload,
		.req_payload_len	= req_payload_len,
		.expect_cmd		= expect_cmd,
		.require_proto_ck_ok	= true,
		.out_frame		= &p,
		.resp_payload		= resp_payload,
		.resp_payload_cap	= resp_payload_cap,
		.out_resp_payload_len	= out_resp_payload_len,
		.tries			= tries,
		.retry_delay_us		= retry_delay_us,
	});
	if (ret)
		return ret;

	dev_dbg(gdev->dev,
		 "CMD_XFER: req=0x%02x expect=0x%02x payload_len=%u\n",
		 req_cmd, expect_cmd, p.payload_len);
	return 0;
}
