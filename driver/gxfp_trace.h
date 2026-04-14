#ifndef __GXFP_TRACE_H
#define __GXFP_TRACE_H

#include <linux/types.h>

struct device;

int gxfp_trace_init(struct device *dev);
void gxfp_trace_exit(void);
void gxfp_trace_clear(void);

void gxfp_trace_logf(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

#endif /* __GXFP_TRACE_H */
