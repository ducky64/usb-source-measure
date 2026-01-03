import argparse
import sys
import time
import csv
from decimal import Decimal
from typing import Tuple, List

from SmuInterface import SmuInterface
from cal_util import regress


kLimitCurrentMax = 0.05
kLimitCurrentMin = -0.05
kVoltageCalPoints = [1, 2, 4, 8, 12, 16, 20]
kVoltageCalFinePoints = [-0.025, -0.015, 0, 0.015, 0.025]

kSetReadDelay = 0.5  # seconds


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='SmuCal')
    parser.add_argument('addr', type=str)
    args = parser.parse_args()

    smu = SmuInterface(args.addr)

    prev_factor, prev_offset = smu.cal_get_voltage_set()
    prev_factor_fine, prev_offset_fine = smu.cal_get_voltage_fine_set()
    print(f'Current voltage set cal: {prev_factor} x + {prev_offset}, '
          f'fine: {prev_factor_fine} x + {prev_offset_fine}')

    while True:
        print('Clear and re-run calibration? [y/n]: ', end='')
        user_data = input()
        if user_data.lower() == 'y':
            break
        elif user_data.lower() == 'n':
            sys.exit()

    smu.cal_set_voltage_set(1, 0)
    smu.cal_set_voltage_fine_set(1, 0)

    smu.set_current_limits(kLimitCurrentMin, kLimitCurrentMax)

    set_voltage_data: List[Tuple[Decimal, Decimal]] = []  # quantized readback setpoints
    meas_voltage_data: List[Decimal] = []  # ADC-measured 'ground-truth' values

    try:
        smu.enable(True, "3A")
        for set_voltage in kVoltageCalPoints:
            smu.set_voltage(set_voltage)

            for set_voltage_fine in kVoltageCalFinePoints:
                smu.set_voltage_fine(set_voltage_fine)
                set_voltage_readback, set_voltage_fine_readback, _, _ = smu.get_voltage_current_set()
                time.sleep(kSetReadDelay)

                set_voltage_data.append((set_voltage_readback, set_voltage_fine_readback))
                meas_voltage, meas_current = smu.get_voltage_current()
                meas_voltage_data.append(meas_voltage)

                print(f"SP={set_voltage_readback} + {set_voltage_fine_readback}, MV={meas_voltage}")
    finally:
        smu.enable(False)
