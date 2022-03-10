CC := gcc
LIBRARIES := -lgmp -lreadline
DEFINES := # -D_DEBUG
CFLAGS := -g -Og

calculator: calculator.c
	$(CC) $< $(LIBRARIES) $(CLFAGS) $(DEFINES) -o $@
