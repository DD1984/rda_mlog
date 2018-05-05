TARGET = rda_mlog

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld

C_SOURCES =  rda_mlog.c dump.c db.c print.c

OBJECTS = $(C_SOURCES:.c=.o)
vpath %.c $(sort $(dir $(C_SOURCES)))

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

all: $(OBJECTS) traceDb.o
	$(CC) $(OBJECTS) traceDb.o $(LDFLAGS) -o $(TARGET) -lc -static

traceDb.o:
	$(LD) -r -b binary -o traceDb.o traceDb.yaml

clean:
	-rm -fR .dep $(OBJECTS) traceDb.o $(TARGET)
