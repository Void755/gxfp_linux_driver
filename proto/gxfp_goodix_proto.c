#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "gxfp_goodix_proto.h"

bool gxfp_parse_goodix_body(const __u8 *buf, size_t buf_len,
			    struct gxfp_frame_parsed *out)
{
	size_t need;
	__u16 decl_len, payload_len;
	__u8 cmd;
	__u8 proto_ck;

	if (!out || !buf || buf_len < 4)
		return false;
	memset(out, 0, sizeof(*out));

	cmd = buf[0];
	decl_len = (__u16)buf[1] | ((__u16)buf[2] << 8);
	if (decl_len < 1)
		return false;
	payload_len = (__u16)(decl_len - 1);

	need = 3u + (size_t)payload_len + 1u;
	if (need > buf_len)
		return false;

	out->cmd = cmd;
	out->decl_len = decl_len;
	out->payload = &buf[3];
	out->payload_len = payload_len;
	proto_ck = buf[3 + payload_len];
	out->proto_checksum = proto_ck;
	out->proto_checksum_ok =
		(gxfp_goodix_proto_checksum(cmd, decl_len, out->payload, payload_len) == proto_ck);
	out->valid = true;

	return true;
}

__u8 gxfp_goodix_proto_checksum(__u8 cmd, __u16 decl_len,
				const __u8 *payload, size_t payload_len)
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
	size_t total;

	if (!out || out_cap < 4)
		return -EINVAL;
	if (payload_len && !payload)
		return -EINVAL;
	if (payload_len > GXFP_GOODIX_PAYLOAD_U16_MAX)
		return -EOVERFLOW;

	decl_len = (__u16)(1u + payload_len);
	total = 1u + 2u + payload_len + 1u;
	if (total > out_cap)
		return -EMSGSIZE;

	out[0] = cmd;
	out[1] = (__u8)(decl_len & 0xff);
	out[2] = (__u8)((decl_len >> 8) & 0xff);
	if (payload_len)
		memcpy(&out[3], payload, payload_len);
	out[3 + payload_len] = gxfp_goodix_proto_checksum(cmd, decl_len, payload, payload_len);

	return (int)total;
}