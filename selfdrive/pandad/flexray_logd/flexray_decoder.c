#include <stdio.h>
#include <string.h>
#include "flexray_decoder.h"


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

   //LOGW("flexray header crc %x / %x counter=%d", (crc ^ xorval), crc_org, hdr->counter);

   return (crc ^ xorval) != crc_org;
}



// decode flexray for cabana
size_t decode_flexray_buffer(char *data, size_t *psize, char *out) {
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

    #if 0
    fprintf(sterr, "flexray busoffset=%d len=%d res=%x bus=%x rej=%x ret=%x ext=%x fl=%x id=%x len=%0x crc=%x cnt=%x",
        bus_offset,
        data_len, fheader->reserved, fheader->bus,
        fheader->rejected, fheader->returned, fheader->extended,
        (fheader->flagsid & 0xf8)>>3, frame_id,
        fheader->length,
        ((fheader->crc_msb << 10)  | (fheader->crc << 2)| fheader->crc_lsb) , fheader->counter);
    #endif

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

    #if 0
    if (calculate_flexray_checksum((uint8_t *) &header , frame_id) != 0) {
        //LOGE("Panda Flexray header checksum failed" );
        //size = 0;
        //return false;
    }
    #endif

		memcpy(&out[o_size], (char *)&new_header, sizeof(struct can_header));
    memcpy(&out[o_size + sizeof(struct can_header)], (char *)&data[pos + sizeof(struct can_header)], data_len);

    pos += sizeof(struct can_header) + data_len;
    o_size += sizeof(struct can_header) + data_len;
  }


  // move the overflowing data to the beginning of the buffer for the next round

  //fprintf (stderr, "size = %ld, osize=%d, pos=%d\n", size, o_size, pos);
  memmove(data, &data[pos], size - pos);
  *psize -= pos;

  return o_size;
}
