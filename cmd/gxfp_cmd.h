#ifndef __GXFP_CMD_H
#define __GXFP_CMD_H

#include <linux/types.h>

struct gxfp_dev;
struct gxfp_frame_parsed;

/**
 * struct gxfp_xfer_req - Parameters for a single Goodix command exchange.
 * @req_cmd:              Command byte to send.
 * @expect_cmd:           Expected command byte in the response frame.
 * @require_proto_ck_ok:  If true, require protocol checksum to pass.
 * @tries:                Number of send/receive attempts (0 treated as 1).
 * @retry_delay_us:       Microseconds to wait between retries.
 * @req_payload:          Pointer to request payload (may be NULL).
 * @req_payload_len:      Length of request payload in bytes.
 * @out_frame:            If non-NULL, receives parsed frame metadata on success.
 * @resp_payload:         If non-NULL, response payload is copied here.
 * @resp_payload_cap:     Capacity of @resp_payload buffer.
 * @out_resp_payload_len: If non-NULL, receives actual response payload length.
 */
struct gxfp_xfer_req {
	__u8		req_cmd;
	__u8		expect_cmd;
	bool		require_proto_ck_ok;
	unsigned int	tries;
	unsigned int	retry_delay_us;

	const __u8	*req_payload;
	size_t		req_payload_len;

	struct gxfp_frame_parsed *out_frame;

	__u8		*resp_payload;
	size_t		resp_payload_cap;
	size_t		*out_resp_payload_len;
};

int gxfp_xfer(struct gxfp_dev *gdev, const struct gxfp_xfer_req *req);

int gxfp_cmd_xfer(struct gxfp_dev *gdev,
			 __u8 req_cmd,
			 const __u8 *req_payload,
			 size_t req_payload_len,
			 __u8 expect_cmd,
			 __u8 *resp_payload,
			 size_t resp_payload_cap,
			 size_t *out_resp_payload_len,
			 unsigned int tries,
			 unsigned int retry_delay_us);

#endif /* __GXFP_CMD_H */
