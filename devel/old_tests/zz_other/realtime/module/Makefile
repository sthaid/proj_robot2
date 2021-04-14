EXTRA_CFLAGS += -g -I$(PWD)/../../../../../common/include/

KERNEL_ROOT=/lib/modules/$(shell uname -r)/build

rt_gpio_test_module-y := rt_gpio_test.o 

obj-m := rt_gpio_test_module.o

default:
	make -C $(KERNEL_ROOT) M=$(PWD)

install:
	sudo make -C $(KERNEL_ROOT) M=$(PWD) modules_install
	sudo depmod

clean:
	make -C $(KERNEL_ROOT) M=$(PWD) clean

load:
	sudo insmod rt_gpio_test_module.ko
	
unload:
	sudo rmmod rt_gpio_test_module
