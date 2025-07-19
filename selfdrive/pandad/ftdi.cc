
#include "selfdrive/pandad/panda.h"  // for  _USE_FLEXRAY_HARNESS_

#ifdef _USE_FLEXRAY_HARNESS_

#include <sys/file.h>
#include <sys/ioctl.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "common/util.h"
#include "common/timing.h"
#include "common/swaglog.h"
#include "panda/board/comms_definitions.h"
#include "selfdrive/pandad/panda_comms.h"

#define FTDI_DEVICE_ID  0x0403
#if 0
#define FTDI_PRODUCT_ID 0x6010
#else
#define FTDI_PRODUCT_ID 0x6011
#endif

#define FLEXRAY_SOCKET_PATH   "/tmp/flexraylogd_unix_socket"

int connect_to_server(struct sockaddr_un *addr) {
    int sfd;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return -1;
    }
    if (connect(sfd, (struct sockaddr*)addr, sizeof(struct sockaddr)) < 0) {
        perror("connect");
        close(sfd);
        return -1;
    }
    return sfd;
}

PandaFtdiHandle::PandaFtdiHandle(std::string serial) : PandaCommsHandle(serial) {
  char serial_no[128];

  sprintf(serial_no, "%04x%04x", FTDI_DEVICE_ID, FTDI_PRODUCT_ID);

  hw_serial =  serial_no;
  sockfd = -1;

  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  strncpy(sock_addr.sun_path, FLEXRAY_SOCKET_PATH, sizeof(sock_addr.sun_path) - 1);

  return;
}


PandaFtdiHandle::~PandaFtdiHandle() {
  std::lock_guard lk(hw_lock);
  cleanup();
  connected = false;
}

void PandaFtdiHandle::cleanup() {
  if(sockfd < 0) close (sockfd);
}



int PandaFtdiHandle::control_write(uint8_t request, uint16_t param1, uint16_t param2, unsigned int timeout) {

  return 0;
}

int PandaFtdiHandle::control_read(uint8_t request, uint16_t param1, uint16_t param2, unsigned char *data, uint16_t length, unsigned int timeout) {

  switch(request) {
    case 0xc1 :
      *data = ((int)cereal::PandaState::PandaType::FLEXRAY_PANDA);
      break;

    // state health
    case 0xd2 :
      break;

    // can health
    case 0xc2 :
      break;

    // firmware
    case 0xd3 :
      break;
    case 0xd4:
      break;

    default:
      break;
  }

  return 0;
}

int PandaFtdiHandle::bulk_write(unsigned char endpoint, unsigned char* data, int length, unsigned int timeout) {

  return 0;
}

int PandaFtdiHandle::bulk_read(unsigned char endpoint, unsigned char* data, int length, unsigned int timeout) {
  int recv = -1;
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = timeout;


  sockfd = connect_to_server(&sock_addr);

  if(sockfd >= 0) {
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    recv = read(sockfd, data, length);
  }

  if(recv < 0)
  {
    LOGW("FTDI : fail read_data %d", recv);
    comms_healthy = false;
  }

  close(sockfd);
  sockfd = -1;

  return recv;
}


std::vector<std::string> PandaFtdiHandle::list() {
  std::vector<std::string> serials;

  int sfd;
  char serial_no[128];

  struct sockaddr_un saddr;


  memset(&saddr, 0, sizeof(saddr));
  saddr.sun_family = AF_UNIX;
  strncpy(saddr.sun_path, FLEXRAY_SOCKET_PATH, sizeof(saddr.sun_path) - 1);

  sprintf(serial_no, "%04x%04x", FTDI_DEVICE_ID, FTDI_PRODUCT_ID);



  sfd = connect_to_server(&saddr);

  if(sfd >= 0) {
    serials.push_back(serial_no);
    close(sfd);
  }

  return serials;
}


#endif
