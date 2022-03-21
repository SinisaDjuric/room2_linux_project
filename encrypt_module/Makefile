ifneq ($(KERNELRELEASE),)
obj-m := encrypter.o
else
KDIR := ../../../../../src/linux
all:
	$(MAKE) -C $(KDIR) M=$$PWD
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif
