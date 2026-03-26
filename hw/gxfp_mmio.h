#ifndef __GXFP_MMIO_H
#define __GXFP_MMIO_H

#include <linux/types.h>

int gxfp_mmio_write_qword_aligned(void __iomem *mmio, const __u8 *buf, size_t len);
int gxfp_mmio_read_qword_aligned(__u8 *buf, void __iomem *mmio, size_t len);

#endif /* __GXFP_MMIO_H */