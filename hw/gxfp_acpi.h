#ifndef __GXFP_ACPI_H
#define __GXFP_ACPI_H

#include <linux/device.h>
#include <linux/acpi.h>

int gxfp_acpi_get_gpio_crs_indexes(struct device *dev,
				  int *irq_gpio_crs_index,
				  int done_gpio_crs_index[2],
				  bool *done_active_low);

int gxfp_acpi_get_hid(struct device *dev, char *buf, size_t bufsz);

#endif /* __GXFP_ACPI_H */
