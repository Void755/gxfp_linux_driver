#ifndef __GXFP_UAPI_DRV_H
#define __GXFP_UAPI_DRV_H

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/poll.h>

struct gxfp_dev;

void gxfp_uapi_rxq_push_packet(struct gxfp_dev *gdev, const __u8 *buf, size_t len);

void gxfp_uapi_rxq_flush_locked(struct gxfp_dev *gdev);
ssize_t gxfp_uapi_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
__poll_t gxfp_uapi_poll(struct file *file, poll_table *wait);

int gxfp_uapi_open(struct inode *inode, struct file *file);
int gxfp_uapi_release(struct inode *inode, struct file *file);
ssize_t gxfp_uapi_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);
long gxfp_uapi_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

int gxfp_uapi_register(struct gxfp_dev *gdev);
void gxfp_uapi_unregister(struct gxfp_dev *gdev);

#endif /* __GXFP_UAPI_DRV_H */