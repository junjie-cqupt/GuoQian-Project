ifneq ($(KERNELRELEASE),)

obj-m := mini2440_buttons_rise.o

else
	
KDIR := /home/guoqian/project/motion/src/kernel/linux-2.6.29
all:
	make -C $(KDIR) M=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-
clean:
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers 

endif
