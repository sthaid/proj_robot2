ARCH := $(shell arch)

SUBDIRS = body_test \
          brain/devel

ifeq ($(ARCH),armv7l)
SUBDIRS += body body/devel
endif
  
.PHONY: build clean

build:
	for d in $(SUBDIRS) ; do echo; make -C $$d || exit 1; done

clean:
	for d in $(SUBDIRS) ; do echo; make -C $$d clean || exit 1; done

