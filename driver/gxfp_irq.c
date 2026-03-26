#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/minmax.h>

#include "gxfp_priv.h"
#include "../transport/gxfp_espi_rx_irq.h"

#include "gxfp_irq.h"
#include "gxfp_uapi.h"

#include "../hw/gxfp_gpio.h"

irqreturn_t gxfp_irq_thread(int irq, void *data)
{
	struct gxfp_dev *gdev = data;
	size_t rx_len = 0;
	int ret;
	bool complete = false;
	bool push = false;

	if (!gdev || !gdev->irq.rx_buf || gdev->irq.rx_cap < 16)
		return IRQ_NONE;

	mutex_lock(&gdev->lock);

	gxfp_gpio_set_read_done(gdev, 1);

	ret = gxfp_espi_irq_read_step(gdev, gdev->irq.rx_buf, gdev->irq.rx_cap,
			     &rx_len, &complete);
	if (ret)
		goto out_clear;

	if (!complete)
		goto out_clear;

	if (rx_len == 0)
		goto out_clear;

	push = true;

out_clear:
	gxfp_gpio_set_read_done(gdev, 0);
	mutex_unlock(&gdev->lock);

	if (push)
		gxfp_uapi_rxq_push_packet(gdev, gdev->irq.rx_buf, rx_len);

	return IRQ_HANDLED;
}
