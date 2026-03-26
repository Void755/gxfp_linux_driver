#ifndef __GXFP_CMD_MCU_CONFIG_H
#define __GXFP_CMD_MCU_CONFIG_H

#include <linux/types.h>

struct gxfp_dev;

/* Not used or tested */
int gxfp_cmd_upload_config_mcu(struct gxfp_dev *gdev, const __u8 *cfg, size_t cfg_len);

#endif /* __GXFP_CMD_MCU_CONFIG_H */
