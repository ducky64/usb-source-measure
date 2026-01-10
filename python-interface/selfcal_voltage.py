import argparse
import sys
import time
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface


kLimitCurrentMax = 0.05
kLimitCurrentMin = -0.05
kVoltageCalPoints = [1, 2, 4, 8, 12, 16, 20]
kVoltageCalFineRatios = [-0.4, -0.25, 0, 0.25, 0.4]
# LV short cal option
# kVoltageCalPoints = [1, 5]
# kVoltageCalFineRatios = [-0.4, 0, 0.4]

kSetReadDelay = 0.3  # seconds


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='SmuCal')
    parser.add_argument('addr', type=str)
    args = parser.parse_args()

    smu = SmuInterface(args.addr)

    prev_factor = smu.cal_get(smu.kNameCalVoltageSetFactor)
    prev_offset = smu.cal_get(smu.kNameCalVoltageSetOffset)
    prev_factor_fine = smu.cal_get(smu.kNameCalVoltageFineSetFactor)
    print(f'Current voltage set cal: {prev_factor}x coarse, {prev_factor_fine}x fine, {prev_offset} offset')

    while True:
        print('Clear and re-run calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set(smu.kNameCalVoltageSetFactor, 1)
    smu.cal_set(smu.kNameCalVoltageFineSetFactor, 1)
    smu.cal_set(smu.kNameCalVoltageSetOffset, 0)

    set_data: List[Tuple[float, float]] = []  # quantized readback setpoints ratios
    meas_data: List[float] = []  # ADC-measured 'ground-truth' ratios

    try:
        smu.set_current_limits(kLimitCurrentMin, kLimitCurrentMax)
        smu.enable(True, "3A")
        for set_voltage in kVoltageCalPoints:
            smu.set_voltage(set_voltage)

            for voltage_fine_ratio in kVoltageCalFineRatios:
                smu._set('number', smu.kNameSetRatioVoltageFine, voltage_fine_ratio)
                time.sleep(kSetReadDelay)
                set_voltage_readback = float(smu._get('sensor', smu.kNameSetRatioVoltage))
                set_voltage_fine_readback = float(smu._get('number', smu.kNameSetRatioVoltageFine))
                set_data.append((set_voltage_readback, set_voltage_fine_readback))

                meas_voltage = float(smu._get('sensor', smu.kNameMeasRatioVoltage))
                meas_data.append(meas_voltage)

                print(f"SP={set_voltage_readback} + {set_voltage_fine_readback}, MV={meas_voltage}")
    finally:
        smu.enable(False)

    """Create a matrix A x = b for multivariate least squares
    where
    A is the setpoints with constant 1 for offset
    x is the coefficients (coarse factor, fine factor, offset)
    b is the resulting voltages
    """
    a = np.stack([list(set_voltages) + [1.0]
                  for set_voltages in set_data])
    b = -np.array(meas_data)  # set data is inverted from measurement data
    x, residuals, rank, s = np.linalg.lstsq(a, b, rcond=None)
    factor, factor_fine, offset = x
    factor = 1/factor
    factor_fine = 1/factor_fine
    offset = -offset

    print(f'New voltage set cal: {factor}x coarse, {factor_fine}x fine, {offset}')
    print(f'  residuals: {residuals}')
    while True:
        print('Update calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set(smu.kNameCalVoltageSetFactor, factor)
    smu.cal_set(smu.kNameCalVoltageFineSetFactor, factor_fine)
    smu.cal_set(smu.kNameCalVoltageSetOffset, offset)
