#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stdarg.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "gxfp_trace.h"

#define GXFP_TRACE_RING_SIZE 1024u
#define GXFP_TRACE_LINE_MAX 192u
#define GXFP_TRACE_DEFAULT_ENABLE 0

static struct {
	spinlock_t lock;
	bool inited;
	bool enabled;
	u32 seq;
	u32 head;
	u32 count;
	char lines[GXFP_TRACE_RING_SIZE][GXFP_TRACE_LINE_MAX];
	struct dentry *dir;
} gxfp_trace;

static void gxfp_trace_push_locked(const char *line)
{
	u32 idx;

	if (!line)
		return;

	idx = gxfp_trace.head;
	strscpy(gxfp_trace.lines[idx], line, GXFP_TRACE_LINE_MAX);
	gxfp_trace.head = (idx + 1u) % GXFP_TRACE_RING_SIZE;
	if (gxfp_trace.count < GXFP_TRACE_RING_SIZE)
		gxfp_trace.count++;
}

void gxfp_trace_logf(const char *fmt, ...)
{
	unsigned long flags;
	char msg[128];
	char line[GXFP_TRACE_LINE_MAX];
	va_list ap;
	u64 ts;
	u32 seq;
	int n;

	if (!fmt || !READ_ONCE(gxfp_trace.inited) || !READ_ONCE(gxfp_trace.enabled))
		return;

	va_start(ap, fmt);
	vscnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	ts = ktime_get_ns();
	spin_lock_irqsave(&gxfp_trace.lock, flags);
	seq = ++gxfp_trace.seq;
	n = scnprintf(line, sizeof(line), "%u %llu %s", seq, ts, msg);
	if (n >= 0)
		gxfp_trace_push_locked(line);
	spin_unlock_irqrestore(&gxfp_trace.lock, flags);
}

void gxfp_trace_clear(void)
{
	unsigned long flags;

	spin_lock_irqsave(&gxfp_trace.lock, flags);
	gxfp_trace.head = 0;
	gxfp_trace.count = 0;
	spin_unlock_irqrestore(&gxfp_trace.lock, flags);
}

static int gxfp_trace_dump_show(struct seq_file *s, void *unused)
{
	char (*snapshot)[GXFP_TRACE_LINE_MAX];
	unsigned long flags;
	u32 count;
	u32 start;
	u32 i;

	snapshot = kcalloc(GXFP_TRACE_RING_SIZE, sizeof(*snapshot), GFP_KERNEL);
	if (!snapshot)
		return -ENOMEM;

	spin_lock_irqsave(&gxfp_trace.lock, flags);
	count = gxfp_trace.count;
	start = (gxfp_trace.head + GXFP_TRACE_RING_SIZE - count) % GXFP_TRACE_RING_SIZE;
	for (i = 0; i < count; i++) {
		u32 idx = (start + i) % GXFP_TRACE_RING_SIZE;
		strscpy(snapshot[i], gxfp_trace.lines[idx], GXFP_TRACE_LINE_MAX);
	}
	spin_unlock_irqrestore(&gxfp_trace.lock, flags);

	seq_printf(s, "enabled=%u ring_size=%u count=%u\n",
		   READ_ONCE(gxfp_trace.enabled) ? 1 : 0,
		   GXFP_TRACE_RING_SIZE,
		   count);
	seq_puts(s, "# seq ts_ns message\n");
	for (i = 0; i < count; i++)
		seq_printf(s, "%s\n", snapshot[i]);

	kfree(snapshot);
	return 0;
}

static int gxfp_trace_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, gxfp_trace_dump_show, inode->i_private);
}

static const struct file_operations gxfp_trace_dump_fops = {
	.owner = THIS_MODULE,
	.open = gxfp_trace_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t gxfp_trace_clear_write(struct file *file,
				      const char __user *ubuf,
				      size_t count,
				      loff_t *ppos)
{
	(void)file;
	(void)ubuf;
	(void)ppos;
	gxfp_trace_clear();
	return count;
}

static const struct file_operations gxfp_trace_clear_fops = {
	.owner = THIS_MODULE,
	.write = gxfp_trace_clear_write,
	.llseek = noop_llseek,
};

int gxfp_trace_init(struct device *dev)
{
	(void)dev;

	if (READ_ONCE(gxfp_trace.inited))
		return 0;

	memset(&gxfp_trace, 0, sizeof(gxfp_trace));
	spin_lock_init(&gxfp_trace.lock);
	gxfp_trace.enabled = GXFP_TRACE_DEFAULT_ENABLE;

	gxfp_trace.dir = debugfs_create_dir("gxfp", NULL);
	if (!IS_ERR_OR_NULL(gxfp_trace.dir)) {
		debugfs_create_file("trace_dump", 0400, gxfp_trace.dir, NULL, &gxfp_trace_dump_fops);
		debugfs_create_file("trace_clear", 0200, gxfp_trace.dir, NULL, &gxfp_trace_clear_fops);
		debugfs_create_bool("trace_enable", 0600, gxfp_trace.dir, &gxfp_trace.enabled);
	}

	gxfp_trace.inited = true;
	return 0;
}

void gxfp_trace_exit(void)
{
	if (!READ_ONCE(gxfp_trace.inited))
		return;

	if (gxfp_trace.dir)
		debugfs_remove_recursive(gxfp_trace.dir);
	gxfp_trace.dir = NULL;
	gxfp_trace.inited = false;
}
