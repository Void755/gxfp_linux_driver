#include <linux/errno.h>
#include <linux/string.h>

#include "gxfp_espi_common.h"
#include "gxfp_espi_rx_irq.h"
#include "gxfp_priv.h"
#include "../hw/gxfp_gpio.h"

int gxfp_espi_irq_read_step(struct gxfp_dev *gdev,
			    __u8 *rx_buf,
			    size_t rx_cap,
			    size_t *out_rx_len,
			    bool *complete)
{
	int ret;
	size_t total;
	size_t first_chunk;
	size_t copied;

	if (complete)
		*complete = false;
	if (out_rx_len)
		*out_rx_len = 0;

	if (!gdev || !rx_buf || rx_cap < GXFP_ESPI_CHUNK_MAX)
		return -EINVAL;
	if (!gdev->hw.mailbox_mmio)
		return -ENODEV;
	if (gdev->hw.mailbox_size < GXFP_ESPI_RX_OFF + GXFP_ESPI_CHUNK_MAX)
		return -ENODEV;

	if (!gdev->irq.assem_active) {
		ret = gxfp_espi_rx_start_packet(gdev, rx_buf, rx_cap, true,
					      &total, &first_chunk);
		if (ret) {
			gxfp_espi_irq_assem_reset(gdev);
			return ret;
		}

		gdev->irq.assem_total = total;
		gdev->irq.assem_copied = first_chunk;
		gdev->irq.assem_active = true;

		copied = gdev->irq.assem_copied;
		if (copied >= total) {
			gxfp_espi_irq_assem_reset(gdev);
			if (complete)
				*complete = true;
			if (out_rx_len)
				*out_rx_len = total;
			return 0;
		}

		gxfp_gpio_pulse_write_done(gdev);
		if (out_rx_len)
			*out_rx_len = copied;
		return 0;
	}

	total = gdev->irq.assem_total;
	copied = gdev->irq.assem_copied;
	if (total == 0 || copied == 0 || copied > rx_cap) {
		gxfp_espi_irq_assem_reset(gdev);
		return -EINVAL;
	}

	{
		size_t remain = (copied < total) ? (total - copied) : 0;
		size_t chunk;
		size_t pad;
		__u8 *tmp = gdev->irq.rx_tmp;

		gxfp_espi_rx_chunk_calc(remain, &chunk, &pad, NULL);
		if (chunk == 0) {
			gxfp_espi_irq_assem_reset(gdev);
			if (complete)
				*complete = true;
			if (out_rx_len)
				*out_rx_len = total;
			return 0;
		}

		if (copied + chunk > rx_cap) {
			gxfp_espi_irq_assem_reset(gdev);
			return -EMSGSIZE;
		}

		if (!tmp) {
			gxfp_espi_irq_assem_reset(gdev);
			return -ENODEV;
		}

		ret = gxfp_espi_rx_read_window(gdev, tmp);
		if (ret) {
			gxfp_espi_irq_assem_reset(gdev);
			return ret;
		}

		memcpy(rx_buf + copied, tmp, chunk);
		copied += chunk;
		gdev->irq.assem_copied = copied;
	}

	if (copied < total) {
		gxfp_gpio_pulse_write_done(gdev);
		if (out_rx_len)
			*out_rx_len = copied;
		return 0;
	}

	gxfp_espi_irq_assem_reset(gdev);
	if (complete)
		*complete = true;
	if (out_rx_len)
		*out_rx_len = total;
	return 0;
}
