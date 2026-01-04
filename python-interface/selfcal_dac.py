import argparse
import sys
import time
from decimal import Decimal
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface


kLimitCurrentMax = 0.05
kLimitCurrentMin = -0.05
kVoltageCalPoints = [1, 2, 4, 8, 12, 16, 20]
# kVoltageCalPoints = [0.5, 1, 2, 4, 6]
kVoltageCalFinePoints = [-0.025, -0.015, 0, 0.015, 0.025]
# kVoltageCalFinePoints = [-0.025, 0, 0.025]

kSetReadDelay = 0.3  # seconds


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='SmuCal')
    parser.add_argument('addr', type=str)
    args = parser.parse_args()

    smu = SmuInterface(args.addr)

    prev_factor, prev_offset = smu.cal_get_voltage_set()
    prev_factor_fine = smu.cal_get_voltage_fine_set()
    print(f'Current voltage set cal: {prev_factor} coarse + {prev_factor_fine} fine + {prev_offset}')

    while True:
        print('Clear and re-run calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set_voltage_set(1, 0)
    smu.cal_set_voltage_fine_set(1)

    smu.set_current_limits(kLimitCurrentMin, kLimitCurrentMax)

    set_voltage_data: List[Tuple[Decimal, Decimal]] = []  # quantized readback setpoints
    meas_voltage_data: List[Decimal] = []  # ADC-measured 'ground-truth' values

    try:
        smu.enable(True, "3A")
        for set_voltage in kVoltageCalPoints:
            smu.set_voltage(set_voltage)

            for set_voltage_fine in kVoltageCalFinePoints:
                smu.set_voltage_fine(set_voltage_fine)
                time.sleep(kSetReadDelay)
                set_voltage_readback, set_voltage_fine_readback, _, _ = smu.get_voltage_current_set()
                set_voltage_data.append((set_voltage_readback, set_voltage_fine_readback))
                meas_voltage, meas_current = smu.get_voltage_current()
                meas_voltage_data.append(meas_voltage)

                print(f"SP={set_voltage_readback} + {set_voltage_fine_readback}, MV={meas_voltage}")
    finally:
        smu.enable(False)

    """Create a matrix A x = b for multivariate least squares
    where
    A is the setpoints with constant 1 for offset
    x is the coefficients (coarse factor, fine factor, offset)
    b is the resulting voltages
    """
    a = np.stack([[float(value) for value in set_voltages] + [1.0]
                  for set_voltages in set_voltage_data])
    b = np.array([float(value) for value in meas_voltage_data])
    x, residuals, rank, s = np.linalg.lstsq(a, b, rcond=None)
    factor, factor_fine, offset = x
    print(f'New voltage set cal: {factor} coarse + {factor_fine} fine + {offset}')
    print(f'  residuals: {residuals}')
    while True:
        print('Update calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set_voltage_set(factor, offset)
    smu.cal_set_voltage_fine_set(factor_fine)
