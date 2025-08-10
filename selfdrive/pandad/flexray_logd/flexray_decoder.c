#include <stdio.h>
#include <string.h>
#include "flexray_decoder.h"

//#define _USE_CRC_TABLE_ 1


#ifdef _USE_CRC_TABLE_

static const uint16_t flexray_crc11_table[256] = {
0x000, 0x385, 0x70A, 0x48F, 0x1B4, 0x231, 0x6BE, 0x53B,
0x368, 0x0ED, 0x465, 0x7E0, 0x2DB, 0x15E, 0x5D1, 0x653,
0x1C5, 0x240, 0x6CF, 0x540, 0x07B, 0x3FE, 0x771, 0x4F4,
0x2A7, 0x123, 0x5AC, 0x620, 0x31B, 0x09E, 0x411, 0x794,
0x389, 0x081, 0x409, 0x781, 0x2BA, 0x133, 0x5BB, 0x633,
0x06C, 0x3E5, 0x76D, 0x4E5, 0x1DE, 0x257, 0x6DF, 0x555,
0x2D1, 0x159, 0x5D1, 0x659, 0x36E, 0x0E7, 0x46F, 0x7E7,
0x1B8, 0x230, 0x6B8, 0x530, 0x00B, 0x383, 0x70B, 0x483,
0x3B1, 0x039, 0x439, 0x7B1, 0x28A, 0x103, 0x583, 0x60B,
0x05C, 0x3D4, 0x754, 0x4DC, 0x1E7, 0x26F, 0x6E7, 0x56F,
0x2E3, 0x16B, 0x5EB, 0x663, 0x35C, 0x0D4, 0x454, 0x7DC,
0x181, 0x209, 0x689, 0x501, 0x03A, 0x3B2, 0x732, 0x4BB,
0x081, 0x389, 0x709, 0x481, 0x1BA, 0x233, 0x6BB, 0x533,
0x06C, 0x3E4, 0x764, 0x4EC, 0x1D7, 0x25F, 0x6DF, 0x557,
0x2D3, 0x15B, 0x5DB, 0x653, 0x36C, 0x0E4, 0x464, 0x7EC,
0x1B1, 0x239, 0x6B9, 0x531, 0x00A, 0x382, 0x70A, 0x482,
0x321, 0x0A9, 0x429, 0x7A1, 0x29A, 0x112, 0x5B2, 0x632,
0x06D, 0x3E5, 0x76D, 0x4E5, 0x1DE, 0x256, 0x6DE, 0x556,
0x2D2, 0x15A, 0x5DA, 0x652, 0x369, 0x0E1, 0x461, 0x7E9,
0x1B2, 0x23A, 0x6BA, 0x532, 0x009, 0x381, 0x701, 0x489,
0x3A9, 0x021, 0x429, 0x7A1, 0x29A, 0x112, 0x5B2, 0x632,
0x065, 0x3ED, 0x76D, 0x4ED, 0x1D6, 0x25E, 0x6DE, 0x556,
0x2DA, 0x152, 0x5DA, 0x652, 0x369, 0x0E1, 0x461, 0x7E9,
0x1BA, 0x232, 0x6B2, 0x53A, 0x00B, 0x383, 0x703, 0x48B,
0x321, 0x0A9, 0x429, 0x7A1, 0x29A, 0x112, 0x5B2, 0x632,
0x069, 0x3E1, 0x769, 0x4E1, 0x1DA, 0x252, 0x6DA, 0x552,
0x2D1, 0x159, 0x5D1, 0x651, 0x36A, 0x0E2, 0x462, 0x7EA,
0x1B3, 0x23B, 0x6BB, 0x533, 0x008, 0x380, 0x700, 0x488,
0x3B9, 0x031, 0x439, 0x7B1, 0x28A, 0x102, 0x582, 0x60A,
0x05D, 0x3D5, 0x755, 0x4DD, 0x1E6, 0x26E, 0x6E6, 0x56E,
0x2E2, 0x16A, 0x5EA, 0x662, 0x359, 0x0D1, 0x451, 0x7D9,
0x180, 0x208, 0x688, 0x500, 0x03B, 0x3B3, 0x733, 0x4BB,
};


// data20: 상위 20비트 데이터 (flagsid, frame_id, length 합친 값)
uint16_t flexray_crc11_calc(uint32_t data20) {
    const uint16_t init = 0x1A;
    const uint16_t poly = 0x385;
    uint16_t crc = init;

    // 상위 8비트
    uint8_t d8 = (data20 >> 12) & 0xFF;
    uint8_t idx = ((crc >> 3) ^ d8) & 0xFF;
    crc = ((crc << 8) ^ flexray_crc11_table[idx]) & 0x7FF;

    // 중간 8비트
    d8 = (data20 >> 4) & 0xFF;
    idx = ((crc >> 3) ^ d8) & 0xFF;
    crc = ((crc << 8) ^ flexray_crc11_table[idx]) & 0x7FF;

    // 마지막 4비트 (비트단위)
    for (int i = 3; i >= 0; i--) {
        uint8_t bit = (data20 >> i) & 1;
        uint8_t msb = (crc >> 10) & 1;
        crc = ((crc << 1) | bit) & 0x7FF;
        if (msb)
            crc ^= poly;
    }
    return crc;
}


uint8_t calculate_flexray_checksum(uint8_t *header, uint16_t fid) {
    struct flexray_header *hdr = (struct flexray_header *) header;
    uint32_t data = (((hdr->flagsid & 0x7) << 14)
                  | (hdr->frame_id << 7)
                  | (hdr->length));

    uint16_t crc_calc = flexray_crc11_calc(data);
    uint16_t crc_org = ((hdr->crc_msb << 10) | (hdr->crc << 2) | hdr->crc_lsb);

    return (crc_calc != crc_org); // 0이면 CRC OK, 1이면 에러
}


#else
uint8_t calculate_flexray_checksum(uint8_t *header, uint16_t fid) {
  const uint16_t polynom = 0x385;
  const uint16_t iv = 0x01A;
  const uint16_t xorval = 0;
  const uint8_t crc_len_bits = 11;
  const uint8_t data_len_bits = 20; // flags(2) + frame_id(11) + length(7)

  struct flexray_header *hdr = (struct flexray_header *) header;


  uint32_t data =   (((hdr->flagsid & 0x7) << 14)
                  | (hdr->frame_id) << 7
                  | hdr->length);

  uint16_t crc_org =  ((hdr->crc_msb << 10)  | (hdr->crc << 2)| hdr->crc_lsb);


  uint16_t reg = iv ^ xorval;

  for (int i = data_len_bits - 1; i >= 0; i--) {
      uint16_t bit = ((reg >> (crc_len_bits - 1)) & 0x1) ^ ((data >> i) & 0x1);
      reg <<= 1;
      if (bit) {
        reg ^= polynom;
      }
   }

   uint16_t mask = (1 << crc_len_bits) - 1;
   uint16_t crc = reg & mask;

   #if 1
   if ((crc ^ xorval) != crc_org) {
      fprintf(stderr, "flexray_decoder : CRC check data=%x crc=%x\n", data, crc_org);
   }
   #endif

   return (crc ^ xorval) != crc_org;
}
#endif




// decode flexray for cabana
size_t decode_flexray_buffer(char *data, size_t *psize, char *out, size_t out_size) {
	int pos = 0;
  int o_size = 0;
  size_t  size = *psize;

  while (pos <= size - sizeof(struct can_header)) {
    struct can_header header, new_header;

    uint16_t data_len;
    struct flexray_header *fheader;
    uint16_t frame_id;


		// find frame start
    if(data[pos] != 0xCA || data[pos+1] != 0xA0) {
      pos++;
      continue;
    }

    pos++;

    memcpy(&header, &data[pos], sizeof(struct can_header));

    fheader =  (struct flexray_header *)(&header);

    // flags(1) + counter (1) + data (len) + CRC (3)
    data_len =  fheader->length * 2 + 5;

    frame_id = fheader->frame_id | ((fheader->flagsid & 0x7) << 8);

    //printf("id=%x cycle=%d\n", frame_id, fheader->counter);

   if (pos + sizeof(struct can_header) + data_len > size) {
      // we don't have all the data for this message yet
      break;
    }

    if (out_size - o_size < sizeof(struct can_header) + data_len) {
      break;
    }

    if (calculate_flexray_checksum((uint8_t *) &header , frame_id) != 0) {
        //fprintf(stderr, "flexray_decoder : err header check_sum\n");
        pos++;
        continue;
    }

    if (fheader->reserved == 1 ){
      unsigned char sync_id;

      switch(frame_id)
      {
        // 200Hz
        case 0x7F:
          sync_id = 0;
          break;

        // 100Hz
        case 0x0f: // 250114 50->100hz
    		case 0x12:
        case 0x17:
        case 0x18:
        case 0x1E:
        case 0x20: // 250114 50->100
        case 0x22:
        case 0x23:
        case 0x29: // 250114 50->100
        case 0x2B:
        case 0x2F:
        case 0x3A:
        case 0x3E: // 250114 50->100
          sync_id = fheader->counter % 2;
          break;

        case 0x3B:
          // 250113
          sync_id = fheader->counter % 2;
          if (sync_id == 0) {
            // 50Hz
            sync_id = fheader->counter % 4;
            sync_id = sync_id + 0x10;
          }
          break;

        case 0x32:
          sync_id = fheader->counter % 2;
          if (sync_id == 1)
          {
            sync_id = fheader->counter % 4;
            sync_id = sync_id + 0x10;
          }
          break;
       // 25Hz
        case 0x10: // 250112
        case 0x6: // 250112
          sync_id = fheader->counter % 8;
          if (fheader->counter == 1 || fheader->counter == 33)
            sync_id = 0x11;
          else if (sync_id == 2 && (fheader->counter % 16) == 2)
            sync_id = 0x22;
          else if (sync_id == 5 && fheader->counter == 5)
            sync_id = 0x55;
          break;

        case 0x30:
        case 0x2E:
        case 0x31: // 250111 50->25
          sync_id = fheader->counter % 8;

          // merge 0,4
          if (sync_id == 4) {
            sync_id = 0;

          // split 5, 13
          }else if (sync_id == 5) {
              sync_id = fheader->counter % 16;
              sync_id = sync_id + 0x50;
          }
          break;

        // 13Hz
        case 0x5:
        case 0x19:  // 250114 50->13hz
          sync_id = fheader->counter % 16;
          // 250112
          if (sync_id == 0xa)
            sync_id = 2;
          break;
        case 0x14: // 250111
          sync_id = fheader->counter % 16;
          break;
        case 0x24: // 250112
          sync_id = fheader->counter % 16;
          if (sync_id == 0 &&  (fheader->counter % 32))
            sync_id = 0x32;
          break;


        case 0xa : // 250114, 100-> 50hz
        case 0xb : // 250111, 100-> 50hz
        default:
          // 50Hz
          sync_id = fheader->counter % 4;

          if (frame_id == 0xa && sync_id == 2) // 250115 50->100
            sync_id = 0;

          else if (frame_id == 0x1A)
          {
            if (sync_id != 0 && sync_id != 3)   // 250115, 250116 add 3
            {
              sync_id = fheader->counter % 16;
              sync_id = sync_id + 0x10;
            }
          }
          else if (frame_id == 0x1B)
          {
            if (sync_id != 0 && sync_id != 2)
            {
              sync_id = fheader->counter % 8;
              // 250112
              if (sync_id == 3 && (fheader->counter % 16) == 3)
                sync_id = 0x33;
            }

            if (fheader->counter == 7)
              sync_id = 0x77;
          }
          break;

      }

      new_header.addr = frame_id << 8 | (sync_id & 0xff);

    }

    new_header.bus = fheader->bus;
    new_header.checksum = fheader->length;


		memcpy(&out[o_size], (char *)&new_header, sizeof(struct can_header));
    memcpy(&out[o_size + sizeof(struct can_header)], (char *)&data[pos + sizeof(struct can_header)], data_len);

    #if 0
    static unsigned long dcount = 0;
    dcount++;
    //if (dcount >= 900 && dcount <= 914) 
    {
      int i;

      printf("%04ld: ", dcount);
      for(i=0; i< sizeof(struct can_header) + data_len; i++) {
        printf("%02X ", out[o_size + i]);
      }

      printf("\n");
    }
    #endif

    pos += sizeof(struct can_header) + data_len;
    o_size += sizeof(struct can_header) + data_len;
  }


  // move the overflowing data to the beginning of the buffer for the next round

  //fprintf (stderr, "size = %ld, osize=%d, pos=%d\n", size, o_size, pos);
  memmove(data, &data[pos], size - pos);
  *psize -= pos;

  return o_size;
}
