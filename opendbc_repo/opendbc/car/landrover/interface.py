from opendbc.car import get_safety_config, structs
from opendbc.car.interfaces import CarInterfaceBase
from opendbc.car.landrover.carcontroller import CarController
from opendbc.car.landrover.carstate import CarState
from opendbc.car.landrover.values import CAR


class CarInterface(CarInterfaceBase):
  CarState = CarState
  CarController = CarController

  @staticmethod
  def _get_params(ret: structs.CarParams, candidate, fingerprint, car_fw, alpha_long, docs) -> structs.CarParams:
    ret.brand = "landrover"


    ret.steerLimitTimer = 0.4
    ret.steerActuatorDelay = 0.1  # Default delay

    ret.radarUnavailable = True

    ret.alphaLongitudinalAvailable = False

    if alpha_long:
      ret.openpilotLongitudinalControl = True

    ret.pcmCruise = True # managed by cruise state manager

    if ret.centerToFront == 0:
      ret.centerToFront = ret.wheelbase * 0.4

    if candidate in (CAR.RANGEROVER_VOGUE_2017):
      CarInterfaceBase.configure_torque_tune(candidate, ret.lateralTuning)
      ret.steerActuatorDelay = 0.11
      ret.enableBsm = True

    elif candidate in (CAR.LANDROVER_DEFENDER_2023):
      ret.steerControlType = structs.CarParams.SteerControlType.angle
      ret.enableBsm = True

    ret.safetyConfigs = [get_safety_config(structs.CarParams.SafetyModel.landrover, ret.flags)]

    return ret

