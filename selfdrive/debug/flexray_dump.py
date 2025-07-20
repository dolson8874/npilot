#!/usr/bin/env python3
import os
import time

import argparse
import binascii
from collections import defaultdict

import cereal.messaging as messaging
#from common.realtime import sec_since_boot

def sec_since_boot():
  return time.time()



def flexray_printer(bus, max_msg, addr, ascii_decode, canaddr):
  logcan = messaging.sub_sock('can', addr=addr)

  start = sec_since_boot()
  lp = sec_since_boot()
  msgs = defaultdict(list)
  while 1:
    can_recv = messaging.drain_sock(logcan, wait_for_one=True)
    for x in can_recv:
      for y in x.can:
        if y.src == bus:
          x = binascii.hexlify(y.dat).decode('ascii')
          ll = len(y.dat);
          if canaddr == 0:
            msgs[y.address].append(y.dat)
            print( "%04X(%6d)(%6d)[%d] %s" % (y.address, y.address, len(y.dat), ll, x))
          else:
            if y.address == canaddr:
              msgs[y.address].append(y.dat)
              print( "%04X(%6d)(%6d)[%d] %s" % (y.address, y.address, len(y.dat), ll, x))

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="simple CAN data viewer",
                                   formatter_class=argparse.ArgumentDefaultsHelpFormatter)

  parser.add_argument("--bus", type=int, help="CAN bus to print out", default=6)
  parser.add_argument("--max_msg", type=int, help="max addr")
  parser.add_argument("--ascii", action='store_true', help="decode as ascii")
  parser.add_argument("--addr", default="127.0.0.1")
  parser.add_argument("--caddr", type=int, help="ETH address", default=0)

  args = parser.parse_args()
  flexray_printer(args.bus, args.max_msg, args.addr, args.ascii, args.caddr)
