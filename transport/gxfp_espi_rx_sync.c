#include <linux/device.h>
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include "gxfp_espi_common.h"
#include "gxfp_espi_rx_sync.h"
#include "gxfp_priv.h"
#include "../hw/gxfp_delay.h"
#include "../hw/gxfp_gpio.h"
#include "gxfp_constants.h"

static int gxfp_espi_read_assemble(struct gxfp_dev *gdev,
				    __u8 *dst,
				    size_t dst_cap,
				    size_t *out_len)
{
	size_t total;
	size_t first_chunk;
	size_t off;
	int ret;

	if (!gdev || !dst || dst_cap < GXFP_ESPI_CHUNK_MAX)
		return -EINVAL;
	if (!gdev->hw.mailbox_mmio)
		return -ENODEV;
	if (gdev->hw.mailbox_size < GXFP_ESPI_RX_OFF + GXFP_ESPI_CHUNK_MAX)
		return -ENODEV;

	memset(dst, 0, dst_cap);

	ret = gxfp_espi_rx_start_packet(gdev, dst, dst_cap, false,
				      &total, &first_chunk);
	if (ret)
		return ret;

	dev_dbg(gdev->dev, "ESPI RX: first=%u total=%zu (mp_len=%u) head16=%*ph\n",
		 GXFP_ESPI_CHUNK_MAX, total,
		 (unsigned int)get_unaligned_le16(dst + 1), 16, dst);
	off = first_chunk;

	while (off < total) {
		size_t remain = total - off;
		size_t chunk;
		size_t pad;
		__u8 *tmp = gdev->irq.rx_tmp;

		gxfp_espi_rx_chunk_calc(remain, &chunk, &pad, NULL);

		dev_dbg(gdev->dev,
			"ESPI RX: off=%zu remain=%zu chunk=%zu pad=%zu\n",
			off, remain, chunk, pad);

		if (!tmp)
			return -ENODEV;

		if (off + chunk > dst_cap)
			return -EMSGSIZE;

		ret = gxfp_espi_rx_read_window(gdev, tmp);
		if (ret)
			return ret;

		memcpy(dst + off, tmp, chunk);

		off += chunk;

		if (off < total) {
			gxfp_gpio_pulse_write_done(gdev);
			gxfp_busy_wait_us(GXFP_DEFAULT_TIMEOUT_US);
		}
	}

	gxfp_gpio_pulse_read_done(gdev);
	dev_dbg(gdev->dev, "ESPI RX: done total=%zu\n", total);

	if (out_len)
		*out_len = total;

	return 0;
}

int gxfp_espi_read(struct gxfp_dev *gdev,
		   __u8 *rx_buf,
		   size_t rx_cap,
		   size_t *out_rx_len)
{
	if (!gdev || !rx_buf || rx_cap == 0)
		return -EINVAL;
	if (!gdev->hw.mailbox_mmio)
		return -ENODEV;

	return gxfp_espi_read_assemble(gdev, rx_buf, rx_cap, out_rx_len);
}
