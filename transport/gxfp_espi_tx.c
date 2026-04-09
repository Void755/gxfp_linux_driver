#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "gxfp_espi_tx.h"
#include "gxfp_espi_rx_sync.h"
#include "gxfp_espi_common.h"
#include "gxfp_priv.h"
#include "../hw/gxfp_delay.h"
#include "../hw/gxfp_gpio.h"
#include "../hw/gxfp_mmio.h"
#include "gxfp_constants.h"

int gxfp_espi_xfer(struct gxfp_dev *gdev,
		   const __u8 *mp_frame,
		   size_t mp_frame_len,
		   __u8 *rx_buf,
		   size_t rx_cap,
		   size_t *out_rx_len)
{
	int ret;

	ret = gxfp_espi_write(gdev, mp_frame, mp_frame_len);
	if (ret)
		return ret;

	gxfp_busy_wait_us(1000);

	return gxfp_espi_read(gdev, rx_buf, rx_cap, out_rx_len);
}

int gxfp_espi_write(struct gxfp_dev *gdev,
		    const __u8 *mp_frame,
		    size_t mp_frame_len)
{
	__u8 *tx;
	__u16 seq16;
	size_t pad;
	size_t tx_len;
	int ret;

	if (!gdev || !mp_frame || mp_frame_len == 0)
		return -EINVAL;
	if (!gdev->hw.mailbox_mmio)
		return -ENODEV;

	pad = (GXFP_ESPI_ALIGN - (mp_frame_len & (GXFP_ESPI_ALIGN - 1))) &
	      (GXFP_ESPI_ALIGN - 1);
	tx_len = 8 + mp_frame_len + pad;
	if (tx_len > GXFP_TX_BUFFER_SIZE)
		return -EOVERFLOW;

	tx = kmalloc(tx_len, GFP_KERNEL);
	if (!tx)
		return -ENOMEM;

	memset(tx, 0, tx_len);
	tx[0] = GXFP_ESPI_WRAPPER_MAGIC;
	tx[1] = (__u8)(mp_frame_len & 0xff);
	tx[2] = (__u8)((mp_frame_len >> 8) & 0xff);
	tx[3] = (__u8)((tx[0] + tx[1] + tx[2]) & 0xff);

	seq16 = (__u16)(atomic_inc_return(&gdev->mailbox_seq16) & 0xFFFF);
	tx[4] = (__u8)(seq16 & 0xff);
	tx[5] = (__u8)((seq16 >> 8) & 0xff);

	memcpy(&tx[8], mp_frame, mp_frame_len);

	ret = gxfp_mmio_write_qword_aligned(gdev->hw.mailbox_mmio, tx,
					 ALIGN(tx_len, GXFP_ESPI_ALIGN));
	if (ret) {
		dev_err_ratelimited(gdev->dev,
			"ESPI TX: mmio_write failed rc=%d seq16=0x%04x payload_len=%zu\n",
			ret, seq16, mp_frame_len);
		kfree(tx);
		return -EIO;
	}

	kfree(tx);
	gxfp_gpio_pulse_write_done(gdev);
	return 0;
}
