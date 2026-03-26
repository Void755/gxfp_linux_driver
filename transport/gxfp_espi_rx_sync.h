#ifndef __GXFP_ESPI_RX_SYNC_H
#define __GXFP_ESPI_RX_SYNC_H

#include <linux/types.h>

struct gxfp_dev;

int gxfp_espi_read(struct gxfp_dev *gdev,
                   __u8 *rx_buf,
                   size_t rx_cap,
                   size_t *out_rx_len);

#endif /* __GXFP_ESPI_RX_SYNC_H */
