CC= gcc
CFLAGS= -g -Wall -I. -I./libftdi -I/usr/include/libusb-1.0


LIBOBJS = libftdi/ftdispi.o libftdi/ftdi.o
LIBSTATIC = libftdi/libftdispi.a

LIBS=-L./libftdi -L/aarch64-linux-gnu -lusb-1.0


TEST_SPI= test_spi
TEST_GET_LOG = test_get_log
FLEXRAY_LOGD = flexray_logd

OBJS= $(LIBOBJS) test_spi.o
OBJS2= $(LIBOBJS) flexray_reader.o flexray_decoder.o flexray_logd.o
OBJS3= test_get_log.o


$(LIBSTATIC): $(LIBOBJS)
	ar rcs $@ $^

$(TEST_SPI): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(FLEXRAY_LOGD): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(LIBS)

$(GET_LOG): $(OBJS3)
	$(CC) $(CFLAGS) -o $@ $(OBJS3)


libftdi/ftdispi.o: libftdi/ftdispi.h libftdi/ftdispi.c
libftdi/ftdi.o: libftdi/ftdi.h libftdi/ftdi.c
flexray_logd.o: flexray_logd.c flexray_decoder.h
flexray_reader.o: flexray_reader.c flexray_reader.h
flexray_decoder.o: flexray_decoder.c flexray_decoder.h
test_get_log.o: test_get_log.c
test_spi.o: test_spi.c

all: $(LIBSTATIC) $(FLEXRAY_LOGD)

test: $(TEST_SPI) $(TEST_GET_LOG)

clean:
	rm -f *.o libftdi/*.o
	rm -f $(FLEXRAY_LOGD)
	rm -f $(TEST_GET_LOG)
	rm -f $(TEST_SPI)
