#embedded linux course

KERNEL_MODULE_DIR=kernel_module
APP_DIR=mailbox

all: kernel app

kernel:
	$(MAKE) --directory=$(KERNEL_MODULE_DIR)

app:
	$(MAKE) --directory=$(APP_DIR)

clean:
	$(MAKE) clean --directory=$(KERNEL_MODULE_DIR)
	$(MAKE) clean --directory=$(APP_DIR)

.PHONY: all clean

