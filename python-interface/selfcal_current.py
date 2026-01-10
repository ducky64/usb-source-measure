import argparse
import sys
import time
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface


kVoltage = 2.0
kCurrentCalPoints = [0.1, 0.5, 1, 1.5, 2]

kSetReadDelay = 0.3  # seconds


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='SmuCal')
    parser.add_argument('addr', type=str)
    args = parser.parse_args()

    smu = SmuInterface(args.addr)

    prev_factor = smu.cal_get(smu.kNameCalCurrentSetFactor)
    prev_offset = smu.cal_get(smu.kNameCalCurrentSetOffset)
    print(f'Current voltage set cal: {prev_factor}x, {prev_offset} offset')

    while True:
        print('Clear and re-run calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set(smu.kNameCalCurrentSetFactor, 1)
    smu.cal_set(smu.kNameCalCurrentSetOffset, 0)

    set_data: List[float] = []  # quantized readback setpoints ratios
    meas_data: List[float] = []  # ADC-measured 'ground-truth' ratios

    try:
        smu.set_voltage(kVoltage)
        smu.enable(True, "3A")
        for set_current in kCurrentCalPoints:
            smu.set_current_limits(-0.1, set_current)

            time.sleep(kSetReadDelay)
            set_current_readback = float(smu._get('sensor', smu.kNameSetRatioCurrentMax))
            set_data.append(set_current_readback)

            meas_current = float(smu._get('sensor', smu.kNameMeasRatioCurrent))
            meas_data.append(meas_current)

            print(f"SP={set_current_readback}, MV={meas_current}")
    finally:
        smu.enable(False)

    """Create a matrix A x = b for multivariate least squares
    where
    A is the setpoints with constant 1 for offset
    x is the coefficients (factor, offset)
    b is the resulting voltages
    """
    a = np.stack([[set_point, 1.0]
                  for set_point in set_data])
    b = -np.array(meas_data)  # set data is inverted from measurement data
    x, residuals, rank, s = np.linalg.lstsq(a, b, rcond=None)
    factor, offset = x
    factor = 1/factor
    offset = -offset

    print(f'New current set cal: {factor}x, {offset} offset')
    print(f'  residuals: {residuals}')
    while True:
        print('Update calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set(smu.kNameCalCurrentSetFactor, factor)
    smu.cal_set(smu.kNameCalCurrentSetOffset, offset)
