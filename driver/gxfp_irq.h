#ifndef __GXFP_IRQ_H
#define __GXFP_IRQ_H

#include <linux/interrupt.h>

irqreturn_t gxfp_irq_thread(int irq, void *data);

#endif /* __GXFP_IRQ_H */
