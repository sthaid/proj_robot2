EXTRA_CFLAGS += -g -I$(PWD)/../../../include

KERNEL_ROOT=/lib/modules/$(shell uname -r)/build

rt_gpio_test_module-y := rt_gpio_test.o 

obj-m := rt_gpio_test_module.o

default:
	make -C $(KERNEL_ROOT) M=$(PWD)

install:
	sudo make -C $(KERNEL_ROOT) M=`pwd` modules_install
	sudo depmod

clean:
	make -C $(KERNEL_ROOT) M=`pwd` clean

load:
	sudo insmod rt_gpio_test_module.ko
	#sudo modprobe rt_gpio_test_module
	
unload:
	sudo rmmod rt_gpio_test_module
