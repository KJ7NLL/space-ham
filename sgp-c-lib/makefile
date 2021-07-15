CFLAGS = -Wall -g -DINLINE
MFLAGS = $(CFLAGS)
CC = gcc $(CFLAGS)
MCC = gcc $(MFLAGS) -lm



# all

all            : sgp-c-lib.o test_sgp-c-lib


# test_sgp-c-lib

test_sgp-c-lib : sgp-c-lib.o testing.o
	$(MCC) -o test_sgp-c-lib sgp-c-lib.o testing.o

testing.o      : testing.c sgp.h
	$(CC) -c testing.c


# sgp-c-library

sgp-c-lib.o    :  sgp0.o sgp4sdp4.o sgp8sdp8.o sgp_conv.o sgp_deep.o \
                    sgp_math.o sgp_time.o
	$(LD) -r -o sgp-c-lib.o sgp0.o sgp4sdp4.o sgp8sdp8.o sgp_conv.o \
                    sgp_deep.o sgp_math.o sgp_time.o


sgp_time.o    :   sgp_time.c sgp.h sgp_int.h
	$(CC) -c sgp_time.c

sgp_math.o    :   sgp_math.c sgp.h sgp_int.h
	$(CC) -c sgp_math.c

sgp_deep.o    :   sgp_deep.c sgp.h sgp_int.h
	$(CC) -c sgp_deep.c

sgp_conv.o    :   sgp_conv.c sgp.h sgp_int.h
	$(CC) -c sgp_conv.c

sgp8sdp8.o    :   sgp8sdp8.c sgp.h sgp_int.h
	$(CC) -c sgp8sdp8.c

sgp4sdp4.o    :   sgp4sdp4.c sgp.h sgp_int.h
	$(CC) -c sgp4sdp4.c

sgp0.o        :   sgp0.c sgp.h sgp_int.h
	$(CC) -c sgp0.c


# CLEAN et al.

CLEANFILES = test_sgp-c-lib testing.o sgp-c-lib.o sgp0.o sgp4sdp4.o \
             sgp8sdp8.o sgp_conv.o sgp_deep.o sgp_math.o sgp_time.o

clean          :
	rm -rf  $(CLEANFILES)


