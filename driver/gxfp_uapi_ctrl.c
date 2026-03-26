#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "gxfp_priv.h"
#include "../transport/gxfp_espi_tx.h"
#include "../include/uapi/linux/gxfp_ioctl.h"
#include "gxfp_uapi.h"

int gxfp_uapi_open(struct inode *inode, struct file *file)
{
	struct miscdevice *m = file->private_data;
	struct gxfp_dev *gdev = container_of(m, struct gxfp_dev, uapi.miscdev);
	unsigned long flags;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (file->f_mode & FMODE_READ) {
		spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
		if (!gdev->uapi.rxq_inited) {
			spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
			return -ENODEV;
		}
		if (gdev->uapi.rxq_reader_open) {
			spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
			return -EBUSY;
		}
		gdev->uapi.rxq_reader_open = true;
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
	}

	file->private_data = gdev;
	return 0;
}

int gxfp_uapi_release(struct inode *inode, struct file *file)
{
	struct gxfp_dev *gdev = file->private_data;
	unsigned long flags;

	if (!gdev)
		return 0;

	if (file->f_mode & FMODE_READ) {
		spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
		gdev->uapi.rxq_reader_open = false;
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
	}

	return 0;
}

ssize_t gxfp_uapi_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct gxfp_dev *gdev = file->private_data;
	struct gxfp_tx_pkt_hdr txh;
	__u8 *mp_local = NULL;
	size_t payload_len;
	size_t need;
	int ret;

	if (!gdev || !ubuf)
		return -ENODEV;
	if (count < sizeof(txh))
		return -EINVAL;

	if (copy_from_user(&txh, ubuf, sizeof(txh)))
		return -EFAULT;

	payload_len = (size_t)txh.payload_len;
	if (payload_len > GXFP_IOCTL_TX_PAYLOAD_MAX)
		return -EINVAL;

	need = sizeof(txh) + payload_len;
	if (count != need)
		return -EINVAL;

	mp_local = kmalloc(4u + payload_len, GFP_KERNEL);
	if (!mp_local)
		return -ENOMEM;

	mp_local[0] = txh.mp_flags;
	mp_local[1] = (__u8)(payload_len & 0xff);
	mp_local[2] = (__u8)((payload_len >> 8) & 0xff);
	mp_local[3] = (__u8)(mp_local[0] + mp_local[1] + mp_local[2]);
	if (payload_len && copy_from_user(mp_local + 4, ubuf + sizeof(txh), payload_len)) {
		kfree(mp_local);
		return -EFAULT;
	}

	mutex_lock(&gdev->lock);
	ret = gxfp_espi_write(gdev, mp_local, 4u + payload_len);
	mutex_unlock(&gdev->lock);

	kfree(mp_local);
	if (ret)
		return ret;

	return (ssize_t)count;
}

long gxfp_uapi_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct gxfp_dev *gdev = file->private_data;
	int ret;

	if (!gdev)
		return -ENODEV;

	switch (cmd) {
	case GXFP_IOCTL_FLUSH_RXQ: {
		unsigned long flush_flags;

		ret = mutex_lock_interruptible(&gdev->uapi.rxq_mutex);
		if (ret)
			return ret;

		spin_lock_irqsave(&gdev->uapi.rxq_lock, flush_flags);
		gxfp_uapi_rxq_flush_locked(gdev);
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flush_flags);
		mutex_unlock(&gdev->uapi.rxq_mutex);
		return 0;
	}
	default:
		return -ENOTTY;
	}
}