#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/minmax.h>

#include "gxfp_priv.h"
#include "gxfp_uapi.h"
#include "../include/uapi/linux/gxfp_ioctl.h"
#include "../proto/gxfp_mp_proto.h"

void gxfp_uapi_rxq_flush_locked(struct gxfp_dev *gdev)
{
	if (!gdev || !gdev->uapi.rxq_inited)
		return;
	kfifo_reset(&gdev->uapi.rxq_fifo);
}

static void gxfp_uapi_rxq_push(struct gxfp_dev *gdev, const __u8 *buf, size_t len)
{
	unsigned long flags;
	struct gxfp_tap_hdr hdr;
	struct gxfp_mp_frame_parsed mp;
	size_t need;

	if (!gdev || !gdev->uapi.rxq_inited || !buf || len == 0)
		return;
	if (!gxfp_parse_mp_frame(buf, len, &mp) || !mp.valid)
		return;
	if (mp.payload_len > (size_t)U32_MAX)
		return;

	memset(&hdr, 0, sizeof(hdr));
	hdr.len = (__u32)mp.payload_len;
	hdr.type = (__u32)(mp.flags >> 4);
	hdr.ts_ns = ktime_get_ns();
	memcpy(hdr.head16, mp.payload, min_t(size_t, mp.payload_len, sizeof(hdr.head16)));

	spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
	if (!gdev->uapi.rxq_inited) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return;
	}
	need = sizeof(hdr) + mp.payload_len;
	if (kfifo_avail(&gdev->uapi.rxq_fifo) < need) {
		unsigned int avail = kfifo_avail(&gdev->uapi.rxq_fifo);
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		dev_warn_ratelimited(gdev->dev,
			"rxq: drop need=%zu avail=%u\n",
			need, avail);
		return;
	}
	(void)kfifo_in(&gdev->uapi.rxq_fifo, (const void *)&hdr, sizeof(hdr));
	(void)kfifo_in(&gdev->uapi.rxq_fifo, mp.payload, mp.payload_len);
	spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);

	wake_up_interruptible(&gdev->uapi.rxq_wq);
}

void gxfp_uapi_rxq_push_packet(struct gxfp_dev *gdev, const __u8 *buf, size_t len)
{
	gxfp_uapi_rxq_push(gdev, buf, len);
}

static bool gxfp_uapi_rxq_ready(struct gxfp_dev *gdev)
{
	unsigned long flags;
	bool ready;

	spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
	ready = gdev->uapi.rxq_inited && !kfifo_is_empty(&gdev->uapi.rxq_fifo);
	spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);

	return ready;
}

static int gxfp_uapi_wait_nonempty(struct gxfp_dev *gdev, struct file *file)
{
	unsigned long flags;
	int ret;

	for (;;) {
		spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
		if (!gdev->uapi.rxq_inited) {
			spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
			return -ENODEV;
		}
		if (!kfifo_is_empty(&gdev->uapi.rxq_fifo)) {
			spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
			return 0;
		}
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(gdev->uapi.rxq_wq,
			gxfp_uapi_rxq_ready(gdev) || !READ_ONCE(gdev->uapi.rxq_inited));
		if (ret)
			return ret;
	}
}

static int gxfp_uapi_peek_need(struct gxfp_dev *gdev, size_t count, size_t *out_need)
{
	struct gxfp_tap_hdr hdr;
	unsigned long flags;
	size_t need;

	if (!out_need)
		return -EINVAL;

	spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
	if (!gdev->uapi.rxq_inited) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return -ENODEV;
	}
	if (kfifo_len(&gdev->uapi.rxq_fifo) < sizeof(hdr)) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return -EIO;
	}
	if (kfifo_out_peek(&gdev->uapi.rxq_fifo, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return -EIO;
	}
	need = sizeof(hdr) + (size_t)hdr.len;
	if ((size_t)kfifo_len(&gdev->uapi.rxq_fifo) < need) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return -EIO;
	}
	if (count < need) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return -EMSGSIZE;
	}
	spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);

	*out_need = need;
	return 0;
}

static int gxfp_uapi_pop_record(struct gxfp_dev *gdev, __u8 *tmp, size_t need, unsigned int *out_popped)
{
	unsigned long flags;
	unsigned int popped;

	if (!out_popped)
		return -EINVAL;

	spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
	if (!gdev->uapi.rxq_inited) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return -ENODEV;
	}
	popped = kfifo_out(&gdev->uapi.rxq_fifo, tmp, need);
	spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);

	*out_popped = popped;
	return 0;
}

ssize_t gxfp_uapi_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	struct gxfp_dev *gdev = file->private_data;
	__u8 *tmp;
	size_t need;
	int ret;
	unsigned int popped = 0;

	if (!gdev || !gdev->uapi.rxq_inited)
		return -ENODEV;
	if (count == 0)
		return 0;

	ret = mutex_lock_interruptible(&gdev->uapi.rxq_mutex);
	if (ret)
		return ret;

	ret = gxfp_uapi_wait_nonempty(gdev, file);
	if (ret)
		goto out_unlock;

	ret = gxfp_uapi_peek_need(gdev, count, &need);
	if (ret)
		goto out_unlock;

	tmp = kmalloc(need, GFP_KERNEL);
	if (!tmp)
	{
		ret = -ENOMEM;
		goto out_unlock;
	}

	ret = gxfp_uapi_pop_record(gdev, tmp, need, &popped);
	if (ret) {
		kfree(tmp);
		goto out_unlock;
	}

	if (popped != need) {
		kfree(tmp);
		ret = -EIO;
		goto out_unlock;
	}

	if (copy_to_user(ubuf, tmp, popped)) {
		kfree(tmp);
		ret = -EFAULT;
		goto out_unlock;
	}

	kfree(tmp);
	ret = (int)popped;

out_unlock:
	mutex_unlock(&gdev->uapi.rxq_mutex);
	return ret;
}

__poll_t gxfp_uapi_poll(struct file *file, poll_table *wait)
{
	struct gxfp_dev *gdev = file->private_data;
	__poll_t mask = 0;
	unsigned long flags;

	if (!gdev || !gdev->uapi.rxq_inited)
		return EPOLLERR;

	poll_wait(file, &gdev->uapi.rxq_wq, wait);
	spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
	if (!gdev->uapi.rxq_inited) {
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		return EPOLLERR;
	}
	if (!kfifo_is_empty(&gdev->uapi.rxq_fifo))
		mask |= EPOLLIN | EPOLLRDNORM;
	spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
	return mask;
}