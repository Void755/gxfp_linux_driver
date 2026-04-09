#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/unaligned.h>

#include "gxfp_espi_common.h"
#include "gxfp_priv.h"
#include "../hw/gxfp_mmio.h"
#include "../proto/gxfp_mp_proto.h"

void gxfp_espi_irq_assem_reset(struct gxfp_dev *gdev)
{
	gdev->irq.assem_active = false;
	gdev->irq.assem_total = 0;
	gdev->irq.assem_copied = 0;
}

int gxfp_espi_rx_read_window(struct gxfp_dev *gdev, __u8 *dst)
{
	int ret;

	ret = gxfp_mmio_read_qword_aligned(dst,
				   gdev->hw.mailbox_mmio + GXFP_ESPI_RX_OFF,
				   (size_t)GXFP_ESPI_CHUNK_MAX);
	if (ret)
		return -EIO;

	return 0;
}

void gxfp_espi_rx_chunk_calc(size_t remain,
			     size_t *chunk,
			     size_t *pad,
			     size_t *do_read)
{
	*chunk = min_t(size_t, remain, (size_t)GXFP_ESPI_CHUNK_MAX);
	*pad = (GXFP_ESPI_ALIGN - (*chunk & (GXFP_ESPI_ALIGN - 1))) &
	       (GXFP_ESPI_ALIGN - 1);
	if (do_read)
		*do_read = *chunk + *pad;
}

int gxfp_espi_rx_start_packet(struct gxfp_dev *gdev,
			      __u8 *rx_buf,
			      size_t rx_cap,
			      bool validate_head,
			      size_t *out_total,
			      size_t *out_first_chunk)
{
	size_t total;
	__u16 mp_len = 0;
	int ret;

	ret = gxfp_espi_rx_read_window(gdev, rx_buf);
	if (ret)
		return ret;

	if (validate_head) {
		if (!gxfp_check_mp_head(rx_buf, 4, &mp_len)) {
			dev_warn_ratelimited(gdev->dev,
				"ESPI RX: bad package head (gxfp_check_mp_head failed)\n");
			return -EBADMSG;
		}
	} else {
		mp_len = get_unaligned_le16(rx_buf + 1);
	}

	total = 4u + (size_t)mp_len;
	if (total == 0 || total > rx_cap)
		return -EMSGSIZE;

	if (out_total)
		*out_total = total;
	if (out_first_chunk)
		*out_first_chunk = min_t(size_t, total, (size_t)GXFP_ESPI_CHUNK_MAX);

	return 0;
}
