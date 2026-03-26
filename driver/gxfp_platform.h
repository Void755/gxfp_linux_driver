#ifndef __GXFP_PLATFORM_H
#define __GXFP_PLATFORM_H

#include <linux/platform_device.h>

int gxfp_platform_probe(struct platform_device *pdev);
void gxfp_platform_remove(struct platform_device *pdev);

int gxfp_platform_init_gpios(struct platform_device *pdev);

#endif /* __GXFP_PLATFORM_H */