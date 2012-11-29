# Makefile

OBJS	= comlog.o

CC	= gcc
CFLAGS	= -Wl,--no-as-needed -fPIC -Wall
LDFLAGS	= -s
LDLIBS	= -lrt

comlog:		$(OBJS)
		$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(OBJS) -o comlog

comlog.o:	comlog.c
		$(CC) $(CFLAGS) -c comlog.c -o comlog.o

install:
	#echo "Copying headers and lib files..."
	#cp firm-dkim.h /usr/local/include/
	#cp libfirm-dkim.so /usr/local/lib/
	#ldconfig

