build:
	make -f Makefile.brain
	echo
	make -f Makefile.audio
	echo
	make -f Makefile.db_dump
	echo
	make -f Makefile.db_rm
	echo

clean:
	make -f Makefile.brain $@
	echo
	make -f Makefile.audio $@
	echo
	make -f Makefile.db_dump $@
	echo
	make -f Makefile.db_rm $@
	echo
