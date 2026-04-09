#ifndef __GXFP_MP_PROTO_H
#define __GXFP_MP_PROTO_H

#include <linux/types.h>

#define GXFP_GOODIX_MP_LEN_MAX 0x6c00u

struct gxfp_mp_frame_parsed {
	bool valid;
	__u8 flags;
	__u16 payload_len;
	const __u8 *payload;
};

bool gxfp_parse_mp_frame(const __u8 *buf, size_t buf_len,
			 struct gxfp_mp_frame_parsed *out);

bool gxfp_check_mp_head(const __u8 *buf, size_t buf_len, __u16 *out_mp_len);

int gxfp_mp_build_frame(__u8 mp_type,
			const __u8 *payload,
			size_t payload_len,
			__u8 *out,
			size_t out_cap);

#endif /* __GXFP_MP_PROTO_H */