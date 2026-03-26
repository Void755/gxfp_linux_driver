#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "gxfp_cmd.h"
#include "../proto/gxfp_proto.h"
#include "../transport/gxfp_espi_tx.h"
#include "../hw/gxfp_delay.h"
#include "gxfp_priv.h"
#include "gxfp_constants.h"

int gxfp_xfer(struct gxfp_dev *gdev, const struct gxfp_xfer_req *req)
{
	__u8 *tx;
	__u8 *rx;
	size_t rx_len = 0;
	unsigned int t;
	int frame_len;

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

	rx = kmalloc(GXFP_MAILBOX_MIN_SIZE, GFP_KERNEL);
	if (!rx) {
		kfree(tx);
		return -ENOMEM;
	}

	frame_len = gxfp_goodix_build_frame(req->req_cmd,
				    req->req_payload, req->req_payload_len,
				    tx, GXFP_TX_BUFFER_SIZE);
	if (frame_len < 0) {
		kfree(tx);
		kfree(rx);
		return frame_len;
	}

	for (t = 0; t < (req->tries ? req->tries : 1); t++) {
		struct gxfp_frame_parsed p;
		int ret;

		ret = gxfp_espi_xfer(gdev, tx, (size_t)frame_len,
				    rx, GXFP_MAILBOX_MIN_SIZE, &rx_len);
		if (ret) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}

		if (!gxfp_parse_goodix_frame(rx,
					    min_t(size_t, rx_len, (size_t)GXFP_MAILBOX_MIN_SIZE),
					    &p)) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}
		if (!p.valid || !p.mp_header_ok) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}
		if (req->require_proto_ck_ok && !p.proto_checksum_ok) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}
		if (p.cmd != req->expect_cmd) {
			gxfp_busy_wait_us(req->retry_delay_us);
			continue;
		}

		if (req->resp_payload) {
			if (p.payload_len > req->resp_payload_cap) {
				kfree(tx);
				kfree(rx);
				return -EMSGSIZE;
			}
			memcpy(req->resp_payload, p.payload, p.payload_len);
		}

		if (req->out_resp_payload_len)
			*req->out_resp_payload_len = p.payload_len;
		if (req->out_frame) {
			*req->out_frame = p;
			req->out_frame->payload = req->resp_payload;
		}

		kfree(tx);
		kfree(rx);
		return 0;
	}

	kfree(tx);
	kfree(rx);
	return -ETIMEDOUT;
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
