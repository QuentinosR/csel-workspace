# Part executed when called from kernel build system
ifneq ($(KERNELRELEASE),)
obj-m += up_info.o          ## name of the generated module
mymodule-objs := up_info.o  ## list of objets needed by that module
CFLAGS_skeleton := -DDEBUG   ## to enable printing of debugging messages

# Part executed when called from standard make in the module source directory
else
CPU := arm64
KDIR := /buildroot/output/build/linux-5.15.21/
TOOLS := /buildroot/output/host/usr/bin/aarch64-buildroot-linux-gnu-
MODPATH := /buildroot/output/target # production mode install:
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(CPU) CROSS_COMPILE=$(TOOLS) modules
	$(MAKE) -C $(KDIR) M=$(PWD) INSTALL_MOD_PATH=$(MODPATH) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) 

endif
