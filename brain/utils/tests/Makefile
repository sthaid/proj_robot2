TARGETS = leds_test grammar_test db_test

all: $(TARGETS)

leds_test: leds_test.c ../leds.c ../misc.c
	gcc -g -Wall -O2 -I.. $^ -lm -lwiringPi -o $@

grammar_test: grammar_test.c ../grammar.c ../misc.c
	gcc -g -Wall -O2 -I.. $^ -lm -o $@

db_test: db_test.c ../db.c ../misc.c
	gcc -g -Wall -O2 -I.. -lm $^ -o $@

clean:
	rm -f $(TARGETS) db_test.dat
