EXTRA_CFLAGS+=	-Wno-declaration-after-statement -std=gnu99

MODULES	= pwm_in

default:	86

obj-m += $(MODULES:%=%.o)

BUILD	= $(MODULES:%=%.ko)

all::	$(BUILD)

clean::
	rm -f $(BUILD) *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions

$(MODULES:%=%.ko):	*.c *.h
	$(MAKE) -C $(KERNEL_SOURCE) ARCH=$(ARCH) \
		SUBDIRS=$(CURDIR) CC=${CC}  modules


KVERSION = $(shell uname -r)
86:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean86:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	
tester: pwmInTester.c
	gcc $^ -o $@ -std=c99 -pedantic -Wall -g
