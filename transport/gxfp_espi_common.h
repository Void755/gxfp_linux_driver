#ifndef __GXFP_ESPI_COMMON_H
#define __GXFP_ESPI_COMMON_H

#include <linux/types.h>
#include "../include/gxfp_constants.h"

struct gxfp_dev;

#define GXFP_ESPI_RX_OFF         0x200
#define GXFP_ESPI_CHUNK_MAX      GXFP_READ_CHUNK_MAX
#define GXFP_ESPI_ALIGN          GXFP_FRAME_ALIGNMENT

void gxfp_espi_irq_assem_reset(struct gxfp_dev *gdev);
int gxfp_espi_rx_read_window(struct gxfp_dev *gdev, __u8 *dst);
void gxfp_espi_rx_chunk_calc(size_t remain,
                             size_t *chunk,
                             size_t *pad,
                             size_t *do_read);
int gxfp_espi_rx_start_packet(struct gxfp_dev *gdev,
                              __u8 *rx_buf,
                              size_t rx_cap,
                              bool validate_head,
                              size_t *out_total,
                              size_t *out_first_chunk);

#endif /* __GXFP_ESPI_COMMON_H */
