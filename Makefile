#ifneq ($(KERNELRELEASE),)
	obj-m := ywy.o
	ywy-objs := super.o inode.o dir.o file.o namei.o 
#else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -fr *.o modules.order Module.symvers *.mod.c *.ko *.ko.usigned .*.cmd .tmp_versions 
	
