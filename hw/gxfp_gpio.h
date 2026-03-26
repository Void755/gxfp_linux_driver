#ifndef __GXFP_GPIO_H
#define __GXFP_GPIO_H

#include <linux/device.h>

struct gxfp_dev;

void gxfp_gpio_pulse_write_done(struct gxfp_dev *gdev);

void gxfp_gpio_set_read_done(struct gxfp_dev *gdev, int value);

void gxfp_gpio_pulse_read_done(struct gxfp_dev *gdev);

void gxfp_gpio_force_done_deassert(struct device *dev, struct gxfp_dev *gdev);

#endif /* __GXFP_GPIO_H */