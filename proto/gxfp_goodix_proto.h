#ifndef __GXFP_GOODIX_PROTO_H
#define __GXFP_GOODIX_PROTO_H

#include <linux/types.h>

#define GXFP_GOODIX_PAYLOAD_U16_MAX 0xFF00u

struct gxfp_frame_parsed {
	bool valid;
	bool proto_checksum_ok;
	__u8 cmd;
	__u16 decl_len;
	const __u8 *payload;
	__u16 payload_len;
	__u8 proto_checksum;
};

bool gxfp_parse_goodix_body(const __u8 *buf, size_t buf_len,
			    struct gxfp_frame_parsed *out);

__u8 gxfp_goodix_proto_checksum(__u8 cmd, __u16 decl_len,
				const __u8 *payload, size_t payload_len);

int gxfp_goodix_build_frame(__u8 cmd,
			   const __u8 *payload,
			   size_t payload_len,
			   __u8 *out,
			   size_t out_cap);

#endif /* __GXFP_GOODIX_PROTO_H */