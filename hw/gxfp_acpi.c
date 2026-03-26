#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "gxfp_acpi.h"

struct gxfp_acpi_res_state {
	struct device *dev;
	int gpio_seen;
	int gpio_io_seen;
	u16 gpio_io_pin[2];
	u8 gpio_io_pol[2];
	int gpio_io_crs_index[2];
	int gpio_int_seen;
	int gpio_int_crs_index;
};

static int gxfp_acpi_res_cb(struct acpi_resource *ares, void *context)
{
	struct gxfp_acpi_res_state *st = context;

	if (!ares || !st || !st->dev)
		return 0;

	switch (ares->type) {
	case ACPI_RESOURCE_TYPE_GPIO: {
		struct acpi_resource_gpio *g = &ares->data.gpio;
		u16 pin0 = 0xffff;
		const char *ctype = "gpio?";
		st->gpio_seen++;

		if (g->pin_table_length < 1)
			break;
		pin0 = g->pin_table[0];

		if (g->connection_type == ACPI_RESOURCE_GPIO_TYPE_INT) {
			ctype = "GpioInt";
			st->gpio_int_seen++;
			if (st->gpio_int_seen == 1)
				st->gpio_int_crs_index = st->gpio_seen - 1;
		} else if (g->connection_type == ACPI_RESOURCE_GPIO_TYPE_IO) {
			ctype = "GpioIo";
			if (st->gpio_io_seen < 2) {
				st->gpio_io_pin[st->gpio_io_seen] = pin0;
				st->gpio_io_pol[st->gpio_io_seen] = g->polarity;
				st->gpio_io_crs_index[st->gpio_io_seen] = st->gpio_seen - 1;
				st->gpio_io_seen++;
			}
		}
		break;
	}
	default:
		break;
	}

	return 0;
}

int gxfp_acpi_get_gpio_crs_indexes(struct device *dev,
				  int *irq_gpio_crs_index,
				  int done_gpio_crs_index[2],
				  bool *done_active_low)
{
	struct acpi_device *adev = ACPI_COMPANION(dev);
	struct gxfp_acpi_res_state st = { .dev = dev };
	LIST_HEAD(res_list);
	int rc;
	bool p0, p1;

	if (!dev || !irq_gpio_crs_index || !done_gpio_crs_index || !done_active_low)
		return -EINVAL;
	if (!adev)
		return -ENODEV;

	st.gpio_int_crs_index = -1;
	st.gpio_io_crs_index[0] = -1;
	st.gpio_io_crs_index[1] = -1;

	rc = acpi_dev_get_resources(adev, &res_list, gxfp_acpi_res_cb, &st);
	acpi_dev_free_resource_list(&res_list);
	if (rc <= 0)
		return -ENOENT;
	if (st.gpio_int_seen < 1 || st.gpio_int_crs_index < 0)
		return -ENOENT;
	if (st.gpio_io_seen < 2 || st.gpio_io_crs_index[0] < 0 || st.gpio_io_crs_index[1] < 0)
		return -ENOENT;

	p0 = st.gpio_io_pol[0] ? true : false;
	p1 = st.gpio_io_pol[1] ? true : false;
	if (p0 != p1) {
		dev_warn(dev, "ACPI _CRS: done GPIO polarities differ (done[0]=%u done[1]=%u)\n",
			 st.gpio_io_pol[0], st.gpio_io_pol[1]);
		return -EINVAL;
	}

	*irq_gpio_crs_index = st.gpio_int_crs_index;
	done_gpio_crs_index[0] = st.gpio_io_crs_index[0];
	done_gpio_crs_index[1] = st.gpio_io_crs_index[1];
	*done_active_low = p0;
	return 0;
}

int gxfp_acpi_get_hid(struct device *dev, char *buf, size_t bufsz)
{
	struct acpi_device *adev;
	const char *hid;
	size_t len;

	if (!dev || !buf || bufsz < 2)
		return -EINVAL;

	adev = ACPI_COMPANION(dev);
	if (!adev)
		return -ENODEV;

	hid = acpi_device_hid(adev);
	if (!hid || hid[0] == '\0')
		return -ENOENT;

	len = strlen(hid);
	if (len >= bufsz)
		return -ENOSPC;

	memcpy(buf, hid, len + 1);
	return 0;
}
