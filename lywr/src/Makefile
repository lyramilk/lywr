KBUILD_CFLAGS += -mhard-float
KBUILD_CFLAGS += -msse -msse2

ifneq ($(KERNELRELEASE),)
	obj-m := lywr_ko.o
	#lywr_ko-cflags += -std=c11
	lywr_ko-objs += lywr_kernel.o lywr_call.o  lywr_data.o  lywr_defs.o  lywr_exec.o  lywr_expr.o  lywr_loader.o  lywr_mempool.o  lywr_opcode.o  lywr_rbtree2.o  lywr_stream.o  lywr_util.o  lywr_wasm_types.o
else
	KDIR:=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)

.PHONY: all clean

all:
	@make -C $(KDIR) M=$(PWD) modules

clean:
	@make -C $(KDIR) M=$(PWD) clean
endif
