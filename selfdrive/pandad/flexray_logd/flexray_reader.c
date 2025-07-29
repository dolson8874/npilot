#include <stdio.h>

#include "flexray_common.h"
#include "flexray_reader.h"
#include "ftdispi.h"

struct ftdi_context    fc;
struct ftdispi_context fsc;

int open_ftdi_dev()
{
    int i;

    if (ftdi_init(&fc) < 0)
    {
        fprintf(stderr, "ftdi_init failed\n");
        return -1;
    }

    if(ftdi_set_interface(&fc, INTERFACE_B) < 0)
    {
      fprintf(stderr, "Set Int Fail\n");
      return -1;
    }

    i = ftdi_usb_open(&fc, FTDI_PID, FTDI_VID);
    if (i < 0 && i != -5)
    {
        fprintf(stderr,
                "OPEN: %s\n",
                ftdi_get_error_string(&fc));
        return -1;
    }

    return 0;
}


void *read_ftdi_spi(void *arg)
{
    struct flexray_reader_args *args = (struct flexray_reader_args*)arg;

    char buf[SPI_CHUNK_SIZE];
    char pending_buf[SPI_CHUNK_SIZE];
    int pending_len = 0;

    //if (open_ftdi_dev() < 0) return NULL;

    ftdispi_open(&fsc, &fc, INTERFACE_B);
    ftdispi_setmode(&fsc, 1, 0, 0, 0, 0, 0);
    //ftdispi_setclock(&fsc, 10000000);
    ftdispi_setclock(&fsc, 7500000);
    ftdispi_setloopback(&fsc,  0);

    while(args->running) {
        int status = FTDISPI_ERROR_NONE;
        size_t bytes_read = 0;

        if(pending_len > 0) {
            bytes_read = pending_len;
            memcpy(buf, pending_buf, pending_len);
            pending_len = 0;
        } else {
          memset(buf, 0, SPI_CHUNK_SIZE);
          status = ftdispi_read(&fsc, buf, SPI_CHUNK_SIZE, 0);
          if (status != FTDISPI_ERROR_NONE)
             continue;

          bytes_read = SPI_CHUNK_SIZE;
        }

				{
					pthread_mutex_lock(args->raw_mutex);

          size_t free_space = BUFFER_FREE(*args->raw_wpos, *args->raw_rpos, SPI_BUFFER_SIZE);
          if (bytes_read > free_space) {
            memcpy(pending_buf, buf, bytes_read);
            pending_len = bytes_read;

            fprintf(stderr, "[ftOVERFLOW]  %zu, free %zu, write %zu, read %zu\n", (size_t)bytes_read, free_space, *args->raw_wpos, *args->raw_rpos);
            pthread_mutex_unlock(args->raw_mutex);
            continue;
          }

          size_t end_space = SPI_BUFFER_SIZE - *args->raw_wpos;

          if ((size_t)bytes_read <= end_space) {
            memcpy(args->raw_buffer + *args->raw_wpos, buf, bytes_read);
            *args->raw_wpos = (*args->raw_wpos + bytes_read) % SPI_BUFFER_SIZE;
          } else {
            memcpy(args->raw_buffer + *args->raw_wpos, buf, end_space);
            memcpy(args->raw_buffer, buf + end_space, bytes_read - end_space);
            *args->raw_wpos = bytes_read - end_space;
          }

					pthread_mutex_unlock(args->raw_mutex);
				}
    }

    ftdispi_close(&fsc, 1);
    return NULL;
}
