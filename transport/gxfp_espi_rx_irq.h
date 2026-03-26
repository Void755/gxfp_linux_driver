#ifndef __GXFP_ESPI_RX_IRQ_H
#define __GXFP_ESPI_RX_IRQ_H

#include <linux/types.h>

struct gxfp_dev;

int gxfp_espi_irq_read_step(struct gxfp_dev *gdev,
                            __u8 *rx_buf,
                            size_t rx_cap,
                            size_t *out_rx_len,
                            bool *complete);

#endif /* __GXFP_ESPI_RX_IRQ_H */
