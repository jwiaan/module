.PHONY: modules clean
ifeq ($(KERNELRELEASE),)
modules:
	$(MAKE) -C/lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
else
obj-m := test.o
endif
clean:
	rm -f *.ko *.o *.order *.symvers *.mod *.mod.c *~
