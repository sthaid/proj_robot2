SUBDIRS = relay \
          i2c \
          gpio \
          realtime/user_mode \
          realtime/module 
  
.PHONY: build clean

build:
	for d in $(SUBDIRS) ; do echo; make -C $$d || exit 1; done

clean:
	for d in $(SUBDIRS) ; do echo; make -C $$d clean || exit 1; done

