# Standard kernel module build
KDIR ?= $(KERNELDIR)
KDIR ?= /lib/modules/$(shell uname -r)/build

# Module configuration
obj-m += gxfp.o

ccflags-y += -I$(src)/include

gxfp-y := \
	gxfp_main.o \
	transport/gxfp_espi_common.o \
	transport/gxfp_espi_tx.o \
	transport/gxfp_espi_rx_sync.o \
	transport/gxfp_espi_rx_irq.o \
	hw/gxfp_acpi.o \
	hw/gxfp_delay.o \
	hw/gxfp_mmio.o \
	hw/gxfp_gpio.o \
	proto/gxfp_mp_proto.o \
	proto/gxfp_goodix_proto.o \
	cmd/gxfp_cmd.o \
	driver/gxfp_platform.o \
	driver/gxfp_irq.o \
	driver/gxfp_uapi_fifo.o \
	driver/gxfp_uapi_ctrl.o \
	driver/gxfp_uapi_misc.o \
	cmd/gxfp_cmd_version.o \
	cmd/gxfp_cmd_mcu_state.o \
	cmd/gxfp_cmd_mcu_config.o \
	cmd/gxfp_cmd_reset.o 

# Build targets
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

.PHONY: all clean install
