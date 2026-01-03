import argparse
import sys
import time
import csv
from decimal import Decimal
from typing import Tuple, List

from SmuInterface import SmuInterface
from cal_util import regress


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

    try:
        smu.enable(True, "30mA")
        # for calibration_point in kVoltageCalPoints:
        #     if isinstance(calibration_point, str):
        #         print(calibration_point, end='')
        #         input()
        #     else:
        #         (set_voltage, set_current_min, set_current_max) = calibration_point
        #         smu.set_current_limits(set_current_min, set_current_max)
        #         smu.set_voltage(set_voltage)
        #         if not enabled:
        #             smu.enable(True)
        #             enabled = True
        time.sleep(1)
    finally:
        smu.enable(False)
