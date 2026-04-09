#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "gxfp_mp_proto.h"
#include "gxfp_constants.h"

bool gxfp_parse_mp_frame(const __u8 *buf, size_t buf_len,
			 struct gxfp_mp_frame_parsed *out)
{
	__u16 mp_len = 0;
	size_t total;

	if (!out || !buf || buf_len < 4)
		return false;
	memset(out, 0, sizeof(*out));

	if (!gxfp_check_mp_head(buf, 4, &mp_len))
		return false;

	total = 4u + (size_t)mp_len;
	if (total > buf_len)
		return false;

	out->flags = buf[0];
	out->payload_len = mp_len;
	out->payload = &buf[4];
	out->valid = true;
	return true;
}

bool gxfp_check_mp_head(const __u8 *buf, size_t buf_len, __u16 *out_mp_len)
{
	__u8 flags, type, ck_expect;
	__u16 mp_len;

	if (out_mp_len)
		*out_mp_len = 0;
	if (!buf || buf_len < 4)
		return false;

	flags = buf[0];
	type = flags >> 4;
	if (type != GXFP_MP_TYPE_A &&
	    type != GXFP_MP_TYPE_B &&
	    type != GXFP_MP_TYPE_C)
		return false;

	mp_len = (__u16)buf[1] | ((__u16)buf[2] << 8);
	if (mp_len >= (__u16)GXFP_GOODIX_MP_LEN_MAX)
		return false;

	ck_expect = (__u8)(flags + buf[1] + buf[2]);
	if (buf[3] != ck_expect)
		return false;

	if (out_mp_len)
		*out_mp_len = mp_len;
	return true;
}

int gxfp_mp_build_frame(__u8 mp_type,
			const __u8 *payload,
			size_t payload_len,
			__u8 *out,
			size_t out_cap)
{
	__u8 flags;
	__u16 mp_len;
	size_t total;

	if (!out || out_cap < 4)
		return -EINVAL;
	if (payload_len && !payload)
		return -EINVAL;
	if (payload_len > GXFP_GOODIX_MP_LEN_MAX)
		return -EOVERFLOW;
	if (mp_type != GXFP_MP_TYPE_A && mp_type != GXFP_MP_TYPE_B && mp_type != GXFP_MP_TYPE_C)
		return -EINVAL;

	mp_len = (__u16)payload_len;
	total = 4u + payload_len;
	if (total > out_cap)
		return -EMSGSIZE;

	flags = (__u8)(mp_type << 4);
	out[0] = flags;
	out[1] = (__u8)(mp_len & 0xff);
	out[2] = (__u8)((mp_len >> 8) & 0xff);
	out[3] = (__u8)(out[0] + out[1] + out[2]);
	if (payload_len)
		memcpy(&out[4], payload, payload_len);

	return (int)total;
}