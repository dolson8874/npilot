import math
from collections import deque
from opendbc.car.common.conversions import Conversions as CV
from opendbc.can.can_define import CANDefine
from opendbc.can.parser import CANParser
from opendbc.car import Bus, structs, create_button_events
from opendbc.car.interfaces import CarStateBase
from opendbc.car.landrover.values import DBC, Buttons, CanBus, CarControllerParams, LandroverFlags

ButtonType = structs.CarState.ButtonEvent.Type

PREV_BUTTON_SAMPLES = 8

BUTTONS_DICT = {Buttons.RES_ACCEL: ButtonType.accelCruise, Buttons.SET_DECEL: ButtonType.decelCruise,
                Buttons.GAP_DIST: ButtonType.gapAdjustCruise, Buttons.CANCEL: ButtonType.cancel}


class CarState(CarStateBase):
  def __init__(self, CP):
    super().__init__(CP)
    self.can_define = CANDefine(DBC[CP.carFingerprint]["pt"])

    if CP.flags & LandroverFlags.FLEXRAY_HARNESS:
      self.shifter_values = self.can_define.dv["GearPRND"]["PRND"]
    else:
      self.shifter_values = self.can_define.dv["GEAR_PRND"]["GEAR_SHIFT"]

    self.cruise_buttons: deque = deque([Buttons.NONE] * PREV_BUTTON_SAMPLES, maxlen=PREV_BUTTON_SAMPLES)
    self.main_buttons: deque = deque([Buttons.NONE] * PREV_BUTTON_SAMPLES, maxlen=PREV_BUTTON_SAMPLES)
    self.lda_button = 0

    self.is_metric = True
    self.params = CarControllerParams(CP)
    self.wheelbase = CP.wheelbase

    self.lfa_btn = 0
    self.lfa_enabled = False


  def update(self, can_parsers) -> structs.CarState:
    if self.CP.flags & LandroverFlags.FLEXRAY_HARNESS:
      return self.update_can_defender(can_parsers)

    cp = can_parsers[Bus.pt]

    ret = structs.CarState()

    self.is_metric = True

    ret.seatbeltUnlatched = (cp.vl["SEAT_BELT"]["SEAT_BELT_DRIVER"]  == 0)
    ret.doorOpen = False

    ret.wheelSpeeds = self.get_wheel_speeds(
      cp.vl["SPEED_01"]["SPEED01"],
      cp.vl["SPEED_02"]["SPEED02"],
      cp.vl["SPEED_01"]["SPEED01"],
      cp.vl["SPEED_02"]["SPEED02"],
    )

    ret.vEgoRaw = (ret.wheelSpeeds.fl + ret.wheelSpeeds.fr + ret.wheelSpeeds.rl + ret.wheelSpeeds.rr) / 4.
    ret.vEgo, ret.aEgo = self.update_speed_kf(ret.vEgoRaw)
    ret.standstill = ret.vEgoRaw < 0.01


    ret.steeringAngleDeg = cp.vl["EPS_01"]["STEER_ANGLE01"]
    ret.steeringRateDeg = cp.vl["EPS_01"]["STEER_SPEED01"]

    steer_rad = math.radians(ret.steeringAngleDeg)
    yawrate_rad_s = math.tan(steer_rad) * ret.vEgo / self.wheelbase

    ret.yawRate = math.degrees(yawrate_rad_s)

    # TODO torq TorqEPS Pressed
    ret.steeringTorque = cp.vl["EPS_00"]["STEER_RATE00"]
    ret.steeringTorqueEps = cp.vl["EPS_02"]["STEER_TORQUE_DRIVER02"]
    ret.steeringPressed = self.update_steering_pressed(abs(ret.steeringTorque) > self.params.STEER_THRESHOLD, 5)

    ret.leftBlinker, ret.rightBlinker = self.update_blinker_from_lamp(50, cp.vl["TURN_SIGNAL"]['LEFT_BLINK'],cp.vl["TURN_SIGNAL"]['RIGHT_BLINK'])


    gear = cp.vl["GEAR_PRND"]["GEAR_SHIFT"]
    ret.gearShifter = self.parse_gear_shifter(self.shifter_values.get(gear))

    ret.brake = 0
    ret.brakePressed = (cp.vl["CRUISE_CONTROL"]['DRIVER_BRAKE'] == 1)

    ret.gas = (cp.vl["ACCELATOR_DRIVER"]["ACCELATOR_DRIVER"])
    ret.gasPressed = ret.gas >= 1


    if self.CP.enableBsm:
      ret.leftBlindspot  = cp.vl["LEFT_ALERT"]["LEFT_ALERT_1"] != 0
      ret.rightBlindspot = cp.vl["RIGHT_ALERT"]["RIGHT_ALERT_1"] != 0
    else:
      ret.leftBlindspot = False
      ret.rightBlindspot = False


    ret.stockAeb = False

    ret.cruiseState.available = cp.vl["CRUISE_CONTROL"]["CRUISE_ON"] == 1
    ret.cruiseState.enabled =  cp.vl["CRUISE_CONTROL"]["CRUISE_ON"] == 1
    ret.cruiseState.speed = ret.vEgoRaw
    ret.cruiseState.standstill = False

    return ret


  def update_can_defender(self, can_parsers) -> structs.CarState:
    cp = can_parsers[Bus.pt]

    ret = structs.CarState()

    self.is_metric = True
    speed_factor = CV.KPH_TO_MS if self.is_metric else CV.MPH_TO_MS

    ret.seatbeltUnlatched = (cp.vl["SeatBelt"]["SeatBelt_Driver"]  == 0)
    ret.doorOpen = not any([cp.vl["DoorStatus"]["FrontLeftDoor"], \
         cp.vl["DoorStatus"]["FrontRightDoor"], \
         cp.vl["DoorStatus"]["RearLeftDoor"], \
         cp.vl["DoorStatus"]["RearRightDoor"]])


    ret.wheelSpeeds = self.get_wheel_speeds(
      cp.vl["WheelSpeedFront"]["SpeedLeft"],
      cp.vl["WheelSpeedFront"]["SpeedRight"],
      cp.vl["WheelSpeedRear"]["SpeedLeft"],
      cp.vl["WheelSpeedRear"]["SpeedRight"],
    )

    ret.vEgoRaw = (ret.wheelSpeeds.fl + ret.wheelSpeeds.fr + ret.wheelSpeeds.rl + ret.wheelSpeeds.rr) / 4.
    ret.vEgo, ret.aEgo = self.update_speed_kf(ret.vEgoRaw)
    ret.standstill = ret.vEgoRaw < 0.01


    ret.steeringAngleDeg = cp.vl["SWM_Angle"]["SteerAngle"]
    ret.steeringRateDeg = cp.vl["SWM_Angle"]["SteerRate"]  # TODO

    steer_rad = math.radians(ret.steeringAngleDeg)
    yawrate_rad_s = math.tan(steer_rad) * ret.vEgo / self.wheelbase

    ret.yawRate = math.degrees(yawrate_rad_s)

    # TODO torq TorqEPS Pressed
    ret.steeringTorque = cp.vl["SWM_Torque"]["TorqueDriver"]
    ret.steeringTorqueEps = cp.vl["PSCM_Out"]["AngleTorque"]
    ret.steeringPressed = self.update_steering_pressed(abs(ret.steeringTorque) > self.params.STEER_THRESHOLD, 5)

    ret.leftBlinker, ret.rightBlinker = self.update_blinker_from_lamp(50, cp.vl["HandleSignal"]['TurnLeft'],cp.vl["HandleSignal"]['TurnRight'])


    gear = cp.vl["GearPRND"]["PRND"]
    ret.gearShifter = self.parse_gear_shifter(self.shifter_values.get(gear))

    ret.brake = cp.vl["Info02"]["BrakePedalPos"]
    ret.brakePressed = cp.vl["StopAndGo"]["BrakeDriver"] == 1
    ret.brakeLights = cp.vl["HandleSignal"]["BrakeLed"] == 1

    ret.gas = cp.vl["GasPedal"]["GasPedalPos"]
    ret.gasPressed = cp.vl["GasPedal_ON"]["GasPedalDriver"] == 1


    if self.CP.enableBsm:
      ret.leftBlindspot  = cp.vl["BlindSpot"]["LeftBS"] == 4
      ret.rightBlindspot = cp.vl["BlindSpot"]["RightBS"] == 4

    ret.stockAeb = False

    ret.cruiseState.available = cp.vl["CruiseInfo"]["CruiseOn"] == 1
    ret.cruiseState.enabled =  cp.vl["CruiseInfo"]["CruiseOn"] == 1
    #ret.cruiseState.speed = ret.vEgoRaw
    ret.cruiseState.speed = ret.vEgoRaw * speed_factor
    ret.cruiseState.standstill = False

    prev_cruise_buttons = self.cruise_buttons[-1]
    prev_main_buttons = self.main_buttons[-1]
    prev_lda_button = self.lda_button

    self.cruise_buttons.extend(cp.vl_all["CruiseInfo"]["CruiseOn"])
    self.main_buttons.extend(cp.vl_all["CruiseInfo"]["CruiseOn"])
    self.lda_button = cp.vl["LKAS_BTN"]["LKAS_Btn_on"]


    ret.buttonEvents = [*create_button_events(self.cruise_buttons[-1], prev_cruise_buttons, BUTTONS_DICT),
                        *create_button_events(self.main_buttons[-1], prev_main_buttons, {1: ButtonType.mainCruise}),
                        *create_button_events(self.lda_button, prev_lda_button, {1: ButtonType.lkas})]


    # ------------------------------------------------------------------------
    # custom messages

    prev_lfa_btn = self.lfa_btn
    self.lfa_btn = cp.vl["LKAS_BTN"]["LKAS_Btn_on"]
    #if prev_lfa_btn != 1 and self.lfa_btn == 1:
    #  self.lfa_enabled = not self.lfa_enabled
    self.lfa_enabled = self.lfa_btn == 1

    #ret.cruiseState.available = self.lfa_enabled

    return ret


  def get_can_parsers(self, CP):
    if self.CP.flags & LandroverFlags.FLEXRAY_HARNESS:
      return self.get_can_parser_defender(CP)

    pt_messages = [
      # sig_name, freq
      ("EPS_00", 50),
      ("EPS_01", 50),
      ("EPS_02", 50),
      ("EPS_03", 50),
      ("EPS_04",  50),
      ("SPEED_01", 50),
      ("SPEED_02", 50),
      ("SPEED_03", 50),
      ("SPEED_04", 50),
      ("CRUISE_CONTROL", 10),
      ("SEAT_BELT", 5),
      ("LKAS_HUD_STAT", 50),
      ("HEAD_LIGHT", 5),
      ("TURN_SIGNAL", 10),
      ("GEAR_PRND", 10),
      ("LEFT_ALERT", 10),
      ("RIGHT_ALERT", 10),
      ("CR00", 50),
      ("ACCELATOR_DRIVER", 100),
    ]

    cam_messages = [
      ("LKAS_RUN", 25),
      ("LKAS_STATUS", 25),
    ]

    return {
      Bus.pt: CANParser(DBC[CP.carFingerprint][Bus.pt], pt_messages, CanBus.UNDERBODY),

      Bus.cam: CANParser(DBC[CP.carFingerprint][Bus.pt], cam_messages, CanBus.CAM)
    }



  def get_can_parser_defender(self, CP):
    pt_messages = [
      # sig_name, freq
      ("SWM_Angle", 100),
      ("SWM_Torque", 50),
      ("PSCM_Out", 50),
      ("WheelSpeedFront", 25),
      ("WheelSpeedRear", 25),
      ("BlindSpot", 6),
      ("GearPRND", 10),
      ("HandleSignal", 100),
      ("SeatBelt", 100),
      ("LKAS_BTN", 16),
      ("CruiseInfo", 25),
      ("StopAndGo", 50),
      ("GasPedal", 50),
      ("GasPedal_ON", 10),
      ("Info02", 25),
      ("DoorStatus", 4),
    ]

    return {
      Bus.pt: CANParser(DBC[CP.carFingerprint][Bus.pt], pt_messages, CanBus.UNDERBODY),
    }

