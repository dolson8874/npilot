#ifndef FLEXRAY_READER_H
#define FLEXRAY_READER_H

#include <stddef.h>
#include <pthread.h>

#define FTDI_PID 0x0403
#define FTDI_VID 0x6011


int open_ftdi_dev();
void *read_ftdi_spi(void *arg);

struct flexray_reader_args {
    char *raw_buffer;
    size_t *raw_wpos;
    size_t *raw_rpos;
    pthread_mutex_t *raw_mutex;
    volatile int *running;
};

#endif

