from opendbc.car.landrover.values import CanBus
from opendbc.car.landrover.lkas_crc_table import find_steer_torque

# crc8 poly=0x1d, xor=0xcc , 32bit
def defender_crc(data):
   crc = 0
   poly = 0x1d

   for byte in data:
      crc = crc ^ byte
      for _i in range(8):
          if crc & 0x80:
              crc = (crc << 1) ^ poly
          else:
              crc = (crc << 1)
      crc &= 0xFF

   return crc ^ 0xcc


# LKAS_COMMAND 0x28F (655) Lane-keeping signal to turn the wheel.
def create_lkas_command(packer, lkas_run, frame, apply_steer):
  counter = frame % 0x10
  torque , crc = find_steer_torque(counter, apply_steer)

  values = {
    "CHECKSUM": crc,
    "ALLFFFF" : 0xffff,
    "A1" : 1,
    "HIGH_TORQ": 0,
    "ALL11" : 3,
    "COUNTER" : counter,
    "STEER_TORQ": torque,
    "LKAS_GREEN" : 1
  }

  return packer.make_can_msg("LKAS_RUN", CanBus.UNDERBODY, values)



def create_lkas_command_defender(packer, enable, latActive, apply_angle, cnt):

  values = {
    "Lkas_checksum" : 0,
    "counter"       : cnt,
    "ReqAngleTorque": apply_angle,  # todo check
    "EnAngle"       : latActive,
    "Engaged"       : enable,
  }

  dat = packer.make_can_msg("LKAS_OP_TO_FLEXRAY", CanBus.CAN2FLEXRAY, values)[1]
  values["Lkas_checksum"] =  defender_crc(dat[1:5])

  return packer.make_can_msg("LKAS_OP_TO_FLEXRAY", CanBus.CAN2FLEXRAY, values)

def create_hud_command_defender(packer, enable, latActive, cnt, left_lane, right_lane):

  values = {
    "Lkas_checksum" : 0,
    "counter"       : cnt,
    "EnAngle"       : latActive,
    "lane_left"     : left_lane,
    "lane_right"    : right_lane,
    "Engaged"       : enable,
  }

  dat = packer.make_can_msg("HUD_OP_TO_FLEXRAY", CanBus.CAN2FLEXRAY, values)[1]
  values["Lkas_checksum"] =  defender_crc(dat[1:5])

  return packer.make_can_msg("HUD_OP_TO_FLEXRAY", CanBus.CAN2FLEXRAY, values)

