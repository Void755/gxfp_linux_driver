#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/minmax.h>
#include <linux/align.h>

#include "gxfp_constants.h"
#include "gxfp_proto.h"

bool gxfp_parse_goodix_frame(const __u8 *buf, size_t buf_len,
				   struct gxfp_frame_parsed *out)
{
	const size_t base = 4;
	size_t total, need;
	__u8 mp_flags, mp_ck, mp_ck_expect;
	__u16 mp_len, decl_len, payload_len;
	__u8 cmd;
	__u8 proto_ck;

	if (!out || !buf || buf_len < 8)
		return false;
	memset(out, 0, sizeof(*out));

	mp_flags = buf[0];
	mp_len = (__u16)buf[1] | ((__u16)buf[2] << 8);
	mp_ck = buf[3];
	mp_ck_expect = (__u8)(mp_flags + buf[1] + buf[2]);

	out->mp_flags = mp_flags;
	out->mp_len = mp_len;
	out->mp_header_ok = (mp_ck == mp_ck_expect);

	total = 4u + (size_t)mp_len;
	if (!out->mp_header_ok || total > buf_len || mp_len < 4)
		return false;

	cmd = buf[base + 0];
	decl_len = (__u16)buf[base + 1] | ((__u16)buf[base + 2] << 8);
	if (decl_len < 1)
		return false;
	payload_len = (__u16)(decl_len - 1);

	need = base + 3u + (size_t)payload_len + 1u;
	if (need > total)
		return false;

	out->cmd = cmd;
	out->decl_len = decl_len;
	out->payload = &buf[base + 3];
	out->payload_len = payload_len;
	proto_ck = buf[base + 3 + payload_len];
	out->proto_checksum = proto_ck;
	out->proto_checksum_ok =
		(gxfp_goodix_proto_checksum(cmd, decl_len, out->payload, payload_len) == proto_ck);

	out->valid = true;
	return true;
}

bool gxfp_goodix_check_package_head(const __u8 *buf, size_t buf_len, __u16 *out_mp_len)
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

__u8 gxfp_goodix_proto_checksum(__u8 cmd, __u16 decl_len, const __u8 *payload, size_t payload_len)
{
	__u32 sum = (__u32)cmd + (__u32)(decl_len & 0xff) + (__u32)((decl_len >> 8) & 0xff);
	size_t i;

	for (i = 0; i < payload_len; i++)
		sum += (__u32)payload[i];

	return (__u8)(0xaa - (__u8)sum);
}

int gxfp_goodix_build_frame(__u8 cmd,
			   const __u8 *payload,
			   size_t payload_len,
			   __u8 *out,
			   size_t out_cap)
{
	__u16 decl_len;
	__u16 mp_len;
	size_t total;

	if (!out || out_cap < 8)
		return -EINVAL;
	if (payload_len && !payload)
		return -EINVAL;
	if (payload_len > GXFP_GOODIX_PAYLOAD_U16_MAX)
		return -EOVERFLOW;

	decl_len = (__u16)(1u + payload_len);
	mp_len = (__u16)(1u + 2u + payload_len + 1u);
	total = 4u + (size_t)mp_len;

	if (total > out_cap)
		return -EMSGSIZE;

	memset(out, 0, total);
	out[0] = (GXFP_MP_TYPE_A << 4);
	out[1] = (__u8)(mp_len & 0xff);
	out[2] = (__u8)((mp_len >> 8) & 0xff);
	out[3] = (__u8)(out[0] + out[1] + out[2]);
	out[4] = cmd;
	out[5] = (__u8)(decl_len & 0xff);
	out[6] = (__u8)((decl_len >> 8) & 0xff);
	if (payload_len)
		memcpy(&out[7], payload, payload_len);
	out[7 + payload_len] = gxfp_goodix_proto_checksum(cmd, decl_len, payload, payload_len);

	return (int)total;
}
