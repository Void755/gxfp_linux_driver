#ifndef __GXFP_PRIV_H
#define __GXFP_PRIV_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

struct gxfp_dev {
	struct device *dev;

	struct mutex lock;
	atomic_t mailbox_seq16;

	struct {
		void __iomem *mailbox_mmio;
		resource_size_t mailbox_size;

		struct gpio_desc *gpio_write_done;
		struct gpio_desc *gpio_read_done;
		struct gpio_desc *gpio_irq;
		int irq;
	} hw;

	struct {
		__u8 *rx_buf;
		size_t rx_cap;
		__u8 *rx_tmp;

		bool assem_active;
		size_t assem_total;
		size_t assem_copied;
	} irq;

	struct {
		struct miscdevice miscdev;
		bool misc_registered;

		struct mutex rxq_mutex;
		spinlock_t rxq_lock;
		wait_queue_head_t rxq_wq;
		struct kfifo rxq_fifo; /* bytes: [tap_hdr][payload][tap_hdr][payload]... */
		bool rxq_inited;
		bool rxq_reader_open;
	} uapi;

};

#endif /* __GXFP_PRIV_H */
