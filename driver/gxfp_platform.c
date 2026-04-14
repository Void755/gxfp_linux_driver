#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include "gxfp_priv.h"
#include "../hw/gxfp_acpi.h"
#include "../hw/gxfp_gpio.h"
#include "gxfp_constants.h"
#include "gxfp_uapi.h"
#include "gxfp_irq.h"
#include "gxfp_platform.h"
#include "gxfp_trace.h"

#define GXFP_IRQ_RX_CAP (128u * 1024u)


static int gxfp_platform_add_acpi_driver_gpios(struct platform_device *pdev)
{
	struct gxfp_dev *gdev = platform_get_drvdata(pdev);
	struct acpi_gpio_params *params;
	struct acpi_gpio_mapping *map;
	int irq_idx;
	int done_idx[2];
	bool done_active_low;
	int rc;

	if (!pdev || !gdev)
		return -EINVAL;

	rc = gxfp_acpi_get_gpio_crs_indexes(&pdev->dev, &irq_idx, done_idx, &done_active_low);
	if (rc) {
		dev_warn(&pdev->dev, "ACPI _CRS: cannot build driver GPIO mapping (rc=%d)\n", rc);
		return rc;
	}

	params = devm_kcalloc(&pdev->dev, 3, sizeof(*params), GFP_KERNEL);
	map = devm_kcalloc(&pdev->dev, 4, sizeof(*map), GFP_KERNEL);
	if (!params || !map)
		return -ENOMEM;

	params[0].crs_entry_index = (unsigned int)irq_idx;
	params[0].line_index = 0;
	params[0].active_low = false;
	map[0].name = "irq-gpios";
	map[0].data = &params[0];
	map[0].size = 1;

	params[1].crs_entry_index = (unsigned int)done_idx[0];
	params[1].line_index = 0;
	params[1].active_low = done_active_low;
	map[1].name = "write-done-gpios";
	map[1].data = &params[1];
	map[1].size = 1;
	map[1].quirks = 0;

	params[2].crs_entry_index = (unsigned int)done_idx[1];
	params[2].line_index = 0;
	params[2].active_low = done_active_low;
	map[2].name = "read-done-gpios";
	map[2].data = &params[2];
	map[2].size = 1;
	map[2].quirks = 0;

	/* map[3] is terminator */

	rc = devm_acpi_dev_add_driver_gpios(&pdev->dev, map);
	if (rc) {
		dev_warn(&pdev->dev, "ACPI: dev_add_driver_gpios failed (rc=%d)\n", rc);
		return rc;
	}

	dev_dbg(&pdev->dev,
		"ACPI: installed driver GPIO mapping (irq_idx=%d done_idx=%d,%d done_active_low=%u)\n",
		irq_idx, done_idx[0], done_idx[1], done_active_low ? 1 : 0);
	return 0;
}

int gxfp_platform_probe(struct platform_device *pdev)
{
	struct gxfp_dev *gdev;
	struct resource *res;
	unsigned long irq_flags;
	unsigned int irq_type;
	int ret;

	gdev = devm_kzalloc(&pdev->dev, sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return -ENOMEM;
	gdev->dev = &pdev->dev;
	mutex_init(&gdev->lock);
	atomic_set(&gdev->mailbox_seq16, GXFP_ESPI_SEQ16_INIT);
	gdev->irq.assem_active = false;
	gdev->irq.assem_total = 0;
	gdev->irq.assem_copied = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing MMIO resource (mailbox window)\n");
		return -ENODEV;
	}

	gdev->hw.mailbox_size = resource_size(res);
	gdev->hw.mailbox_mmio = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gdev->hw.mailbox_mmio))
		return PTR_ERR(gdev->hw.mailbox_mmio);

	platform_set_drvdata(pdev, gdev);
	(void)gxfp_trace_init(&pdev->dev);

	/* GPIOs */
	{
		int gpio_rc = gxfp_platform_init_gpios(pdev);
		if (gpio_rc) {
			if (gpio_rc == -EPROBE_DEFER)
				dev_dbg(&pdev->dev, "GPIO not ready, deferring probe\n");
			return gpio_rc;
		}
	}

	/* misc device */
	ret = gxfp_uapi_register(gdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register misc device (/dev/gxfp) rc=%d\n", ret);
		return ret;
	}

	/* IRQ */
	gdev->irq.rx_cap = GXFP_IRQ_RX_CAP;
	gdev->irq.rx_buf = devm_kmalloc(&pdev->dev, gdev->irq.rx_cap, GFP_KERNEL);
	if (!gdev->irq.rx_buf) {
		dev_err(&pdev->dev, "IRQ: kmalloc(%zu) failed\n", gdev->irq.rx_cap);
		ret = -ENOMEM;
		goto err_ioctl;
	}

	gdev->irq.rx_tmp = devm_kmalloc(&pdev->dev, (size_t)GXFP_READ_CHUNK_MAX, GFP_KERNEL);
	if (!gdev->irq.rx_tmp) {
		dev_err(&pdev->dev, "IRQ: kmalloc(rx_tmp) failed\n");
		ret = -ENOMEM;
		goto err_ioctl;
	}

	gdev->hw.irq = gpiod_to_irq(gdev->hw.gpio_irq);
	if (gdev->hw.irq < 0) {
		dev_err(&pdev->dev, "IRQ: gpiod_to_irq failed rc=%d\n", gdev->hw.irq);
		ret = gdev->hw.irq;
		goto err_ioctl;
	}

	irq_type = irq_get_trigger_type(gdev->hw.irq);
	if (irq_type == IRQ_TYPE_NONE) {
		/*
		 * ACPI DSDT for SPBA reports:
		 *   GpioInt(Level, ActiveHigh, ExclusiveAndWake, ...)
		 */
		dev_info(&pdev->dev, "IRQ: trigger type unknown; defaulting to LEVEL_HIGH\n");
		(void)irq_set_irq_type(gdev->hw.irq, IRQ_TYPE_LEVEL_HIGH);
		irq_type = irq_get_trigger_type(gdev->hw.irq);
	}

	irq_flags = IRQF_ONESHOT | IRQF_NO_AUTOEN;
	ret = devm_request_threaded_irq(&pdev->dev,
				     gdev->hw.irq,
				     NULL,
				     gxfp_irq_thread,
				     irq_flags,
				     "gxfp-irq",
				     gdev);
	if (ret) {
		dev_err(&pdev->dev, "IRQ: request_threaded_irq failed (irq=%d, rc=%d)\n",
			gdev->hw.irq, ret);
		gdev->hw.irq = 0;
		goto err_ioctl;
	}
	dev_dbg(&pdev->dev, "IRQ: registered (irq=%d, flags=0x%lx, type=0x%x)\n",
		 gdev->hw.irq, irq_flags, irq_type);

	dev_dbg(&pdev->dev, "gxfp probed (mailbox=%pa len=%pa)\n", &res->start, &gdev->hw.mailbox_size);
	return 0;

err_ioctl:
	gxfp_uapi_unregister(gdev);
	return ret;
}

int gxfp_platform_init_gpios(struct platform_device *pdev)
{
	struct gxfp_dev *gdev = platform_get_drvdata(pdev);
	struct gpio_desc *d;
	int added_map = 0;

	d = devm_gpiod_get(&pdev->dev, "write-done", GPIOD_OUT_LOW);
	if (IS_ERR(d) && PTR_ERR(d) == -ENOENT) {
		(void)gxfp_platform_add_acpi_driver_gpios(pdev);
		added_map = 1;
		d = devm_gpiod_get(&pdev->dev, "write-done", GPIOD_OUT_LOW);
	}
	gdev->hw.gpio_write_done = d;
	if (IS_ERR(gdev->hw.gpio_write_done))
		return PTR_ERR(gdev->hw.gpio_write_done);

	d = devm_gpiod_get(&pdev->dev, "read-done", GPIOD_OUT_LOW);
	if (!added_map && IS_ERR(d) && PTR_ERR(d) == -ENOENT) {
		(void)gxfp_platform_add_acpi_driver_gpios(pdev);
		added_map = 1;
		d = devm_gpiod_get(&pdev->dev, "read-done", GPIOD_OUT_LOW);
	}
	gdev->hw.gpio_read_done = d;
	if (IS_ERR(gdev->hw.gpio_read_done))
		return PTR_ERR(gdev->hw.gpio_read_done);

	d = devm_gpiod_get(&pdev->dev, "irq", GPIOD_IN);
	if (!added_map && IS_ERR(d) && PTR_ERR(d) == -ENOENT) {
		(void)gxfp_platform_add_acpi_driver_gpios(pdev);
		added_map = 1;
		d = devm_gpiod_get(&pdev->dev, "irq", GPIOD_IN);
	}
	gdev->hw.gpio_irq = d;
	if (IS_ERR(gdev->hw.gpio_irq))
		return PTR_ERR(gdev->hw.gpio_irq);

	if (!gdev->hw.gpio_write_done || !gdev->hw.gpio_read_done || !gdev->hw.gpio_irq) {
		dev_err(&pdev->dev, "missing GPIOs write-done/read-done/irq (write_done=%p, read_done=%p, irq=%p)\n",
			gdev->hw.gpio_write_done, gdev->hw.gpio_read_done, gdev->hw.gpio_irq);
		return -EINVAL;
	}

	gxfp_gpio_force_done_deassert(gdev->dev, gdev);

	return 0;
}

void gxfp_platform_remove(struct platform_device *pdev)
{
	struct gxfp_dev *gdev = platform_get_drvdata(pdev);

	if (gdev && gdev->hw.irq > 0) {
		disable_irq(gdev->hw.irq);
		synchronize_irq(gdev->hw.irq);
	}
	gxfp_trace_exit();
	gxfp_uapi_unregister(gdev);
}