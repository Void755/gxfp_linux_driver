#ifndef __GXFP_ESPI_TX_H
#define __GXFP_ESPI_TX_H

#include <linux/types.h>

struct gxfp_dev;

int gxfp_espi_xfer(struct gxfp_dev *gdev,
                   const __u8 *mp_frame,
                   size_t mp_frame_len,
                   __u8 *rx_buf,
                   size_t rx_cap,
                   size_t *out_rx_len);

int gxfp_espi_write(struct gxfp_dev *gdev,
                    const __u8 *mp_frame,
                    size_t mp_frame_len);

#endif /* __GXFP_ESPI_TX_H */
