#include <linux/io.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "gxfp_mmio.h"

int gxfp_mmio_write_qword_aligned(void __iomem *mmio, const __u8 *buf, size_t len)
{
	size_t i, qwords;

	if (!mmio || !buf)
		return -EINVAL;
	if (len % 8)
		return -EINVAL;

	qwords = len / 8;
	for (i = 0; i < qwords; i++) {
		__u64 v;
		memcpy(&v, buf + i * 8, 8);
		writeq(v, mmio + i * 8);
	}
	mb();
	return 0;
}

int gxfp_mmio_read_qword_aligned(__u8 *buf, void __iomem *mmio, size_t len)
{
	size_t i, qwords;

	if (!mmio || !buf)
		return -EINVAL;
	if (len % 8)
		return -EINVAL;

	qwords = len / 8;
	for (i = 0; i < qwords; i++) {
		__u64 v = readq(mmio + i * 8);
		memcpy(buf + i * 8, &v, 8);
	}
	mb();
	return 0;
}
