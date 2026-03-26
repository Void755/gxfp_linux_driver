#ifndef __GXFP_CMD_RESET_H
#define __GXFP_CMD_RESET_H

#include <linux/types.h>

struct gxfp_dev;

int gxfp_cmd_sleep_cleanup(struct gxfp_dev *gdev, unsigned int post_delay_ms);

int gxfp_cmd_notify_power_state(struct gxfp_dev *gdev,
				__u8 state);

int gxfp_cmd_reset_device(struct gxfp_dev *gdev,
			  __u8 reset_flags);

int gxfp_cmd_tls_unlock(struct gxfp_dev *gdev);

int gxfp_cmd_protocol_init(struct gxfp_dev *gdev);

int gxfp_cmd_recover_session(struct gxfp_dev *gdev, bool unstick_tls);

#endif /* __GXFP_CMD_RESET_H */

