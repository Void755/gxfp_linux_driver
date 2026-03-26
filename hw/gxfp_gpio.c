#include <linux/device.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/moduleparam.h>
#include <linux/module.h>

#include "gxfp_gpio.h"
#include "gxfp_delay.h"
#include "gxfp_priv.h"

#define GXFP_GPIO_PULSE_HIGH_US		4000
#define GXFP_GPIO_READ_DONE_CLEAR_DELAY_US	200
#define GXFP_GPIO_READ_DONE_HIGH_US	50
#define GXFP_GPIO_WRITE_DONE_FORCE_DEASSERT	true
#define GXFP_GPIO_WRITE_DONE_PRE_US	50

static inline void gxfp_gpio_pulse_delay_us(unsigned int us)
{
	if (us)
		gxfp_busy_wait_us(us);
}


void gxfp_gpio_pulse_write_done(struct gxfp_dev *gdev)
{
	if (GXFP_GPIO_WRITE_DONE_FORCE_DEASSERT && gdev && gdev->hw.gpio_write_done) {
		gpiod_set_value_cansleep(gdev->hw.gpio_write_done, 0);
		gxfp_gpio_pulse_delay_us(GXFP_GPIO_WRITE_DONE_PRE_US);
	}
	gpiod_set_value_cansleep(gdev->hw.gpio_write_done, 1);
	gxfp_gpio_pulse_delay_us(GXFP_GPIO_PULSE_HIGH_US);
	gpiod_set_value_cansleep(gdev->hw.gpio_write_done, 0);
	
	gxfp_gpio_pulse_delay_us(GXFP_GPIO_READ_DONE_CLEAR_DELAY_US);
}

void gxfp_gpio_pulse_read_done(struct gxfp_dev *gdev)
{
	gxfp_gpio_set_read_done(gdev, 1);
	gxfp_gpio_pulse_delay_us(GXFP_GPIO_READ_DONE_HIGH_US);
	gxfp_gpio_set_read_done(gdev, 0);
}

void gxfp_gpio_set_read_done(struct gxfp_dev *gdev, int value)
{
	if (!gdev || !gdev->hw.gpio_read_done)
		return;

	gpiod_set_value_cansleep(gdev->hw.gpio_read_done, value ? 1 : 0);

	if (!value)
		gxfp_gpio_pulse_delay_us(GXFP_GPIO_READ_DONE_CLEAR_DELAY_US);
}

void gxfp_gpio_force_done_deassert(struct device *dev, struct gxfp_dev *gdev)
{
	if (!dev || !gdev || !gdev->hw.gpio_write_done || !gdev->hw.gpio_read_done)
		return;

	gpiod_set_value_cansleep(gdev->hw.gpio_write_done, 0);
	gpiod_set_value_cansleep(gdev->hw.gpio_read_done, 0);
}