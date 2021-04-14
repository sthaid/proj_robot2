SUBDIRS = button \
          current \
          encoder \
          mc \
          proximity \
          relay \
          zz_other/i2c \
          zz_other/gpio \
          zz_other/realtime/user_mode \
          zz_other/realtime/module 
  
.PHONY: build clean

build:
	for d in $(SUBDIRS) ; do echo; PWD=$(PWD)/$$d make -C $$d || exit 1; done

clean:
	for d in $(SUBDIRS) ; do echo; make -C $$d clean || exit 1; done

