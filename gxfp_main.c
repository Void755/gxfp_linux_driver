#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include "include/gxfp_priv.h"
#include "cmd/gxfp_cmd.h"
#include "cmd/gxfp_cmd_reset.h"
#include "cmd/gxfp_cmd_version.h"
#include "cmd/gxfp_cmd_mcu_state.h"
#include "driver/gxfp_platform.h"

#define GXFP_DRV_NAME "gxfp"

static int gxfp_startup_sequence(struct platform_device *pdev)
{
	struct gxfp_dev *gdev = platform_get_drvdata(pdev);
	struct gxfp_get_version_result ver;
	struct gxfp_mcu_state mcu;
	int rc;
	int mcu_rc;
	unsigned int try;

	if (!gdev)
		return -ENODEV;

	msleep(150);
	memset(&ver, 0, sizeof(ver));
	memset(&mcu, 0, sizeof(mcu));

	mutex_lock(&gdev->lock);
	rc = 0;
	for (try = 0; try < 3; try++) {
		rc = gxfp_cmd_get_version(gdev, &ver);
		if (!rc)
			break;
		if ((try + 1) < 3)
			msleep(20);
	}
	mcu_rc = gxfp_cmd_query_mcu_state(gdev, &mcu);
	if (ver.status != 0) {
		dev_warn(&pdev->dev,
			 "INIT: get_version failed (%d); attempting recover_session then retry\n",
			 (int)ver.status);
		(void)gxfp_cmd_recover_session(gdev, 1);
		(void)gxfp_cmd_get_version(gdev, &ver);
	}
	mutex_unlock(&gdev->lock);

	if (ver.status == 0 && ver.version_ascii[0] != '\0')
		dev_info(&pdev->dev, "INIT: FW='%s'\n", ver.version_ascii);
	if (ver.status != 0)
		return (int)ver.status;

	return 0;
}

static void gxfp_shutdown_sequence(struct platform_device *pdev)
{
	struct gxfp_dev *gdev;
	int rc;

	if (!pdev)
		return;
	gdev = platform_get_drvdata(pdev);
	if (!gdev)
		return;

	mutex_lock(&gdev->lock);
	(void)gxfp_cmd_notify_power_state(gdev, 1);
	rc = gxfp_cmd_sleep_cleanup(gdev, 0);
	mutex_unlock(&gdev->lock);
	dev_dbg(&pdev->dev, "REMOVE: sent SLEEP/CLEANUP(0xD2) rc=%d\n", rc);
}

static int gxfp_probe(struct platform_device *pdev)
{
	struct gxfp_dev *gdev;
	int ret;

	ret = gxfp_platform_probe(pdev);
	if (ret)
		return ret;

	ret = gxfp_startup_sequence(pdev);
	if (ret) {
		gxfp_platform_remove(pdev);
		return ret;
	}

	gdev = platform_get_drvdata(pdev);
	if (gdev && gdev->hw.irq > 0)
		enable_irq(gdev->hw.irq);

	return 0;
}

static void gxfp_remove(struct platform_device *pdev)
{
	gxfp_shutdown_sequence(pdev);
	gxfp_platform_remove(pdev);
}

static const struct acpi_device_id gxfp_acpi_match[] = {
	{ "GXFP5130", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, gxfp_acpi_match);

static struct platform_driver gxfp_driver = {
	.driver = {
		.name = GXFP_DRV_NAME,
		.acpi_match_table = gxfp_acpi_match,
	},
	.probe = gxfp_probe,
	.remove = gxfp_remove,
};

module_platform_driver(gxfp_driver);

MODULE_DESCRIPTION("eSPI-Based GXFP5130 Fingerprint Sensor Driver");
MODULE_AUTHOR("vindeu");
MODULE_LICENSE("GPL");
