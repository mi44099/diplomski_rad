
PLATFORM = i386

INCLUDES := ../../.. ../../../kernel ../../../arch/$(PLATFORM)

CMACROS := PLATFORM="\"$(PLATFORM)\"" DEBUG MEM_TEST

CC = gcc

CFLAGS = -O -g -lrt
LDFLAGS = -O -g -lrt

ff: test.c test.h ../ff_simple.c ../ff_simple.h
	@$(CC) test.c -c $(CFLAGS) \
		$(foreach MACRO,$(CMACROS),-D $(MACRO)) -D FIRST_FIT
	@$(CC) ../ff_simple.c -c $(CFLAGS) \
		$(foreach INC,$(INCLUDES),-I$(INC)) \
		$(foreach MACRO,$(CMACROS),-D $(MACRO)) -D FIRST_FIT
	@$(CC) ff_simple.o test.o -o $@ $(LDFLAGS)

gma: test.c test.h ../gma.c ../gma.h
	@$(CC) test.c -c $(CFLAGS) \
		$(foreach MACRO,$(CMACROS),-D $(MACRO)) -D GMA
	@$(CC) ../gma.c -c $(CFLAGS) \
		$(foreach INC,$(INCLUDES),-I$(INC)) \
		$(foreach MACRO,$(CMACROS),-D $(MACRO)) -D GMA
	@$(CC) gma.o test.o -o $@ $(LDFLAGS)

clean:
	-rm ff gma *.o