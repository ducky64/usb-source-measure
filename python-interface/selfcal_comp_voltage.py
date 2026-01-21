import argparse
import sys
import time
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface


# these are run with the output off,
kVoltageCalPoints = [
  0,
  5,
  10,
  15,
  20,
]

kSetReadDelay = 0.3  # seconds


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='SmuCal')
    parser.add_argument('addr', type=str)
    args = parser.parse_args()

    smu = SmuInterface(args.addr)

    voltage_factor = smu.cal_get(smu.kNameCalVoltageMeasSetFactor)
    print(f'Current voltage comp cal: {voltage_factor}x')

    while True:
        print('Re-run calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    set_data: List[Tuple[float]] = []  # dv
    meas_data: List[float] = []
    try:
        smu.enable(False, "3A")
        for set_voltage in kVoltageCalPoints:
            smu.set_voltage(set_voltage)
            time.sleep(kSetReadDelay)

            adc_ratio_voltage = float(smu._get('sensor', smu.kNameMeasRatioVoltage))
            delta_voltage = adc_ratio_voltage - float(smu._get('sensor', smu.kNameSetRatioVoltage))
            meas_data.append(-adc_ratio_voltage)
            set_data.append((delta_voltage,))

            print(f"Vadc={adc_ratio_voltage}, dV={delta_voltage}")
    finally:
        smu.enable(False)

    """Create a matrix A x = b for multivariate least squares
    where
    A is the setpoints with constant 1 for offset
    x is the calibration coefficients
    b is the measurements to match
    """
    a = np.stack([list(row) + [1.0] for row in set_data])
    b = np.array(meas_data)
    x, residuals, rank, s = np.linalg.lstsq(a, b, rcond=None)

    voltage_factor, offset = x
    print(f'New comp cal: {voltage_factor}x voltage')
    print(f'  + {offset} offset')
    print(f'  residuals: {residuals}')
    while True:
        print('Update calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set(smu.kNameCalVoltageMeasSetFactor, voltage_factor)
