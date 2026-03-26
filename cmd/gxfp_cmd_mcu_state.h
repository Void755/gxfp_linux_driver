#ifndef __GXFP_CMD_MCU_STATE_H
#define __GXFP_CMD_MCU_STATE_H

#include <linux/types.h>
#include "gxfp_constants.h"

struct gxfp_dev;

#define GXFP_MCU_STATE_LEN 20

struct gxfp_mcu_state {
	/* byte 0 */
	__u8 version;

	/* byte 1 */
	__u8 b1_raw;
	__u8 is_pov_image_valid;
	__u8 is_tls_connected;
	__u8 is_tls_used;
	__u8 is_locked;

	/* byte 2 - 19 */
	__u8 reserved[18];
};

int gxfp_cmd_query_mcu_state(struct gxfp_dev *gdev, struct gxfp_mcu_state *out);
int gxfp_cmd_parse_mcu_state_payload(const __u8 *buf, size_t len, struct gxfp_mcu_state *out);

#endif /* __GXFP_CMD_MCU_STATE_H */
