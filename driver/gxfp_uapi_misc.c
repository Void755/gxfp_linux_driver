#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sizes.h>

#include "gxfp_priv.h"
#include "../hw/gxfp_acpi.h"
#include "gxfp_uapi.h"

#define GXFP_RXQ_FIFO_BYTES		SZ_1M

static ssize_t acpi_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct miscdevice *m = dev_get_drvdata(dev);
	struct gxfp_dev *gdev;
	char hid[16];
	int rc;

	if (!m)
		return -ENODEV;
	gdev = container_of(m, struct gxfp_dev, uapi.miscdev);
	if (!gdev || !gdev->dev)
		return -ENODEV;
	rc = gxfp_acpi_get_hid(gdev->dev, hid, sizeof(hid));
	if (rc)
		return sysfs_emit(buf, "\n");
	return sysfs_emit(buf, "%s\n", hid);
}
static DEVICE_ATTR_RO(acpi_id);

static struct attribute *gxfp_misc_attrs[] = {
	&dev_attr_acpi_id.attr,
	NULL,
};
ATTRIBUTE_GROUPS(gxfp_misc);

static const struct file_operations gxfp_uapi_fops = {
	.owner = THIS_MODULE,
	.open = gxfp_uapi_open,
	.release = gxfp_uapi_release,
	.read = gxfp_uapi_read,
	.write = gxfp_uapi_write,
	.poll = gxfp_uapi_poll,
	.unlocked_ioctl = gxfp_uapi_unlocked_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.llseek = noop_llseek,
};

int gxfp_uapi_register(struct gxfp_dev *gdev)
{
	int ret;
	int rc_fifo;
	unsigned int fifo_bytes;

	if (!gdev)
		return -EINVAL;
	if (gdev->uapi.misc_registered)
		return 0;

	gdev->uapi.miscdev.minor = MISC_DYNAMIC_MINOR;
	gdev->uapi.miscdev.name = "gxfp";
	gdev->uapi.miscdev.fops = &gxfp_uapi_fops;
	gdev->uapi.miscdev.parent = gdev->dev;
	gdev->uapi.miscdev.groups = gxfp_misc_groups;

	spin_lock_init(&gdev->uapi.rxq_lock);
	mutex_init(&gdev->uapi.rxq_mutex);
	init_waitqueue_head(&gdev->uapi.rxq_wq);
	gdev->uapi.rxq_inited = false;
	gdev->uapi.rxq_reader_open = false;
	fifo_bytes = GXFP_RXQ_FIFO_BYTES;
	rc_fifo = kfifo_alloc(&gdev->uapi.rxq_fifo, fifo_bytes, GFP_KERNEL);
	if (rc_fifo)
		return rc_fifo;
	gdev->uapi.rxq_inited = true;

	ret = misc_register(&gdev->uapi.miscdev);
	if (ret)
		goto err_fifo;

	gdev->uapi.misc_registered = true;

	dev_dbg(gdev->dev, "misc device registered: /dev/%s\n", gdev->uapi.miscdev.name);
	return 0;

err_fifo:
	if (gdev->uapi.rxq_inited) {
		kfifo_free(&gdev->uapi.rxq_fifo);
		gdev->uapi.rxq_inited = false;
	}
	return ret;
}

void gxfp_uapi_unregister(struct gxfp_dev *gdev)
{
	unsigned long flags;

	if (!gdev)
		return;
	if (!gdev->uapi.misc_registered && !gdev->uapi.rxq_inited)
		return;

	if (gdev->uapi.misc_registered) {
		misc_deregister(&gdev->uapi.miscdev);
		gdev->uapi.misc_registered = false;
	}

	if (gdev->uapi.rxq_inited) {
		mutex_lock(&gdev->uapi.rxq_mutex);
		spin_lock_irqsave(&gdev->uapi.rxq_lock, flags);
		gdev->uapi.rxq_inited = false;
		gdev->uapi.rxq_reader_open = false;
		kfifo_reset(&gdev->uapi.rxq_fifo);
		spin_unlock_irqrestore(&gdev->uapi.rxq_lock, flags);
		wake_up_interruptible_all(&gdev->uapi.rxq_wq);

		kfifo_free(&gdev->uapi.rxq_fifo);
		mutex_unlock(&gdev->uapi.rxq_mutex);
	}
}