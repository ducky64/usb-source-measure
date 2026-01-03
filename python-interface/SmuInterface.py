from typing import Tuple, Dict, Union, Optional, List, NamedTuple

import requests
import decimal

class SmuInterface:
  device_prefix = 'UsbSMU '

  kNameMacWifi = 'Mac Wifi'

  kNameMeasCurrent = 'Meas Current'
  kNameMeasVoltage = 'Meas Voltage'
  kNameDerivPower = 'Deriv Power'
  kNameDerivEnergy = 'Deriv Energy'

  kNameSetCurrentMin = 'Set Current Min'
  kNameSetCurrentMax = 'Set Current Max'
  kNameSetVoltage = 'Set Voltage'
  kNameActualSetVoltage = 'Set Voltage Actual'
  kNameActualSetVoltageFine = 'Set Voltage Fine Actual'
  kNameActualSetCurrentMin = 'Set Current Min Actual'
  kNameActualSetCurrentMax = 'Set Current Max Actual'

  kNameEnable = "Enable"
  kNameCurrentRange = "Set Current Range"

  kNameCalVoltageMeasFactor = 'Cal Voltage Meas Factor'
  kNameCalVoltageMeasOffset = 'Cal Voltage Meas Offset'
  kNameCalVoltageSetFactor = 'Cal Voltage Set Factor'
  kNameCalVoltageSetOffset = 'Cal Voltage Set Offset'
  kNameCalVoltageFineSetFactor = 'Cal Voltage Fine Set Factor'

  kNameCalCurrentMeasFactor = ['Cal Current0 Meas Factor', 'Cal Current1 Meas Factor', 'Cal Current2 Meas Factor']
  kNameCalCurrentMeasOffset = ['Cal Current0 Meas Offset', 'Cal Current1 Meas Offset', 'Cal Current2 Meas Offset']
  kNameCalCurrentSetFactor = ['Cal Current0 Set Factor', 'Cal Current1 Set Factor', 'Cal Current2 Set Factor']
  kNameCalCurrentSetOffset = ['Cal Current0 Set Offset', 'Cal Current1 Set Offset', 'Cal Current2 Set Offset']

  kNameAllCal = [
    kNameCalVoltageMeasFactor, kNameCalVoltageMeasOffset,
    kNameCalVoltageSetFactor, kNameCalVoltageSetOffset,
    kNameCalVoltageFineSetFactor,
  ] + kNameCalCurrentMeasFactor + kNameCalCurrentMeasOffset + kNameCalCurrentSetFactor + kNameCalCurrentSetOffset

  def _webapi_name(self, name: str) -> str:
    # TODO should actually replace all non-alphanumeric but this is close enough
    return (self.device_prefix + name).replace(' ', '_').lower()

  def _set(self, service: str, name: str, value: Union[float, decimal.Decimal]) -> None:
    resp = requests.post(f'http://{self.addr}/{service}/{self._webapi_name(name)}/set?value={value}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')

  def _get(self, service: str, name: str, read_value=False) -> decimal.Decimal:
    resp = requests.get(f'http://{self.addr}/{service}/{self._webapi_name(name)}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')
    if read_value:
      return decimal.Decimal(resp.json()['value'])
    else:
      return decimal.Decimal(resp.json()['state'].split(' ')[0])

  def _get_text(self, service: str, name: str) -> str:
    resp = requests.get(f'http://{self.addr}/{service}/{self._webapi_name(name)}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')
    return resp.json()['value']

  def __init__(self, addr: str):
    self.addr = addr

  def get_mac(self):
    return self._get_text('text_sensor', self.kNameMacWifi)

  def set_voltage(self, voltage: float) -> None:
    self._set('number', self.kNameSetVoltage, voltage)

  def set_voltage_fine(self, voltage: float) -> None:
    self._set('number', self.kNameActualSetVoltageFine, voltage)

  def set_current_limits(self, current_min: float, current_max: float) -> None:
    assert current_min < current_max
    self._set('number', self.kNameSetCurrentMin, current_min)
    self._set('number', self.kNameSetCurrentMax, current_max)

  def get_voltage_current(self) -> Tuple[decimal.Decimal, decimal.Decimal]:
    """Returns the measured voltage and current"""
    return (self._get('sensor', self.kNameMeasVoltage),
            self._get('sensor', self.kNameMeasCurrent))

  def get_voltage_current_set(self) -> Tuple[decimal.Decimal, decimal.Decimal, decimal.Decimal, decimal.Decimal]:
      """Returns the voltage (coarse, fine) and current (sink limit, source limit) as a DAC-quantized target value"""
      return (self._get('sensor', self.kNameActualSetVoltage),
              self._get('number', self.kNameActualSetVoltageFine),  # inout typed
              self._get('sensor', self.kNameActualSetCurrentMin),
              self._get('sensor', self.kNameActualSetCurrentMax))

  def get_deriv_power(self) -> decimal.Decimal:
    """Returns the derived power in watts"""
    return self._get('sensor', self.kNameDerivPower)

  def get_deriv_energy(self) -> decimal.Decimal:
    """Returns the derived cumulative energy in joules"""
    return self._get('sensor', self.kNameDerivEnergy)

  def enable(self, on: bool = True, irange: Optional[str] = None) -> None:
    if on and irange is not None:
      resp = requests.post(f'http://{self.addr}/select/{self._webapi_name(self.kNameCurrentRange)}/set?option={irange}')
      if resp.status_code != 200:
        raise Exception(f'Request failed: {resp.status_code}')

    if on:
      action = 'turn_on'
    else:
      action = 'turn_off'
    resp = requests.post(f'http://{self.addr}/switch/{self._webapi_name(self.kNameEnable)}/{action}')
    if resp.status_code != 200:
      raise Exception(f'Request failed: {resp.status_code}')

  def cal_get_voltage_meas(self) -> Tuple[decimal.Decimal, decimal.Decimal]:
    """Returns the voltage measurement calibration, factor and offset terms"""
    return (self._get('number', self.kNameCalVoltageMeasFactor, read_value=True),
            self._get('number', self.kNameCalVoltageMeasOffset, read_value=True))

  def cal_set_voltage_meas(self, factor: float, offset: float) -> None:
    """Sets the voltage measurement calibration, factor and offset terms"""
    self._set('number', self.kNameCalVoltageMeasFactor, factor)
    self._set('number', self.kNameCalVoltageMeasOffset, offset)

  def cal_get_voltage_set(self) -> Tuple[decimal.Decimal, decimal.Decimal]:
    """Returns the voltage set calibration, factor and offset terms"""
    return (self._get('number', self.kNameCalVoltageSetFactor, read_value=True),
            self._get('number', self.kNameCalVoltageSetOffset, read_value=True))

  def cal_set_voltage_set(self, factor: float, offset: float) -> None:
    """Sets the voltage set calibration, factor and offset terms"""
    self._set('number', self.kNameCalVoltageSetFactor, factor)
    self._set('number', self.kNameCalVoltageSetOffset, offset)

  def cal_get_voltage_fine_set(self) -> decimal.Decimal:
    """Returns the voltage fine set calibration, factor term (offset rolled into coarse cal)"""
    return self._get('number', self.kNameCalVoltageFineSetFactor, read_value=True)

  def cal_set_voltage_fine_set(self, factor: float) -> None:
    """Sets the voltage fine set calibration, factor term"""
    self._set('number', self.kNameCalVoltageFineSetFactor, factor)

  def cal_get_current_meas(self, irange: int) -> Tuple[decimal.Decimal, decimal.Decimal]:
    return (self._get('number', self.kNameCalCurrentMeasFactor[irange], read_value=True),
            self._get('number', self.kNameCalCurrentMeasOffset[irange], read_value=True))

  def cal_set_current_meas(self, irange: int, factor: float, offset: float) -> None:
    self._set('number', self.kNameCalCurrentMeasFactor[irange], factor)
    self._set('number', self.kNameCalCurrentMeasOffset[irange], offset)

  def cal_get_current_set(self, irange: int) -> Tuple[decimal.Decimal, decimal.Decimal]:
    return (self._get('number', self.kNameCalCurrentSetFactor[irange], read_value=True),
            self._get('number', self.kNameCalCurrentSetOffset[irange], read_value=True))

  def cal_set_current_set(self, irange: int, factor: float, offset: float) -> None:
    self._set('number', self.kNameCalCurrentSetFactor[irange], factor)
    self._set('number', self.kNameCalCurrentSetOffset[irange], offset)

  def cal_get_all(self) -> Dict[str, decimal.Decimal]:
    out_dict = {}
    for name in self.kNameAllCal:
      value = self._get('number', name, read_value=True)
      out_dict[name] = value
    return out_dict

  def cal_set_all(self, cal_dict: Dict[str, decimal.Decimal]) -> None:
    for name, value in cal_dict.items():
      assert name in self.kNameAllCal, f'invalid calibration parameter: {name}'
    for name, value in cal_dict.items():
      self._set('number', name, value)

  def sample_buffer(self) -> 'SmuSampleBuffer':
    return SmuSampleBuffer(self)


class SmuSampleRecord(NamedTuple):
  millis: int
  source: str
  value: decimal.Decimal


class SmuSampleBuffer:
  def __init__(self, smu: SmuInterface):
    self._smu = smu
    self._last_sample: Optional[int] = None

  def get(self) -> List[SmuSampleRecord]:
    if self._last_sample is None:
      resp = requests.get(f'http://{self._smu.addr}/samples')
      if resp.status_code != 200:
        raise Exception(f'Request failed: {resp.status_code}')
      self._last_sample = int(resp.text)
      return []
    else:
      resp = requests.get(f'http://{self._smu.addr}/samples?start={self._last_sample}')
      if resp.status_code != 200:
        raise Exception(f'Request failed: {resp.status_code}')
      lines = resp.text.split('\n')
      samples = []
      for sample_line in lines[:-1]:
        sample_line_split = sample_line.split(',')
        samples.append(SmuSampleRecord(
          millis=int(sample_line_split[0]),
          source=sample_line_split[1],
          value=decimal.Decimal(sample_line_split[2])
        ))
      self._last_sample = int(lines[-1])
      return samples
