import argparse
import sys
import time
import csv
from decimal import Decimal
from typing import Tuple, List


from SmuInterface import SmuInterface
from cal_util import regress


kOutputFile = 'calibration.csv'
kSetReadDelay = 0.5  # seconds

kCalPoints = [  # by irange; as voltage, current min, current max
  [  # range 0 (3A)
    "Connect 4-ohm load",
    (1, -0.1, 0.1),
    (3, -0.1, 0.5),
    (5, -0.1, 1),
    (7, -0.1, 1.5),
    (9, -0.1, 2),
  ],
  [  # range 1 (300mA)
    "Connect 50-ohm load",
    (3, -0.1, 0.02),
    (3, -0.1, 0.04),
    (6, -0.1, 0.1),
    (8.5, -0.1, 0.15),
    (11, -0.1, 0.2),
    (13.5, -0.1, 0.25),
  ],
]

if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  parser.add_argument('irange', type=int, nargs='?', default=0)
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  prev_factor, prev_offset = smu.cal_get_current_meas(args.irange)
  print(f'Current voltage meas cal: {prev_factor} x + {prev_offset}')
  prev_factor, prev_offset = smu.cal_get_current_set(args.irange)
  print(f'Current voltage set cal: {prev_factor} x + {prev_offset}')

  while True:
    print('Clear and re-run calibration? [y/n]: ', end='')
    user_data = input()
    if user_data.lower() == 'y':
      break
    elif user_data.lower() == 'n':
      sys.exit()

  smu.cal_set_current_meas(args.irange, 1, 0)
  smu.cal_set_current_set(args.irange, 1, 0)
  time.sleep(kSetReadDelay)

  cal_table = kCalPoints[args.irange]
  with open(kOutputFile, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow([
      'set_voltage', 'set_current_min', 'set_current_max',
      'adc_voltage', 'adc_current', 'meas_voltage', 'meas_current',
      'ref_voltage', 'ref_current'
    ])
    csvfile.flush()

    cal_rows = []
    meas_current_cal_data: List[Tuple[Decimal, Decimal]] = []  # device-measured, external-measured (reference)
    set_current_cal_data: List[Tuple[Decimal, Decimal]] = []  # setpoint, external-measured (reference)

    enabled = False
    for calibration_point in cal_table:
      if isinstance(calibration_point, str):
        print(calibration_point, end='')
        input()
      else:
        (set_voltage, set_current_min, set_current_max) = calibration_point
        if not enabled:
          smu.set_current_limits(set_current_min, set_current_max)
          smu.set_voltage(set_voltage)
          smu.enable(irange=args.irange)
          enabled = True

        smu.set_current_limits(set_current_min, set_current_max)  # TODO this sets it again with the right range factor
        smu.set_voltage(set_voltage)

        time.sleep(kSetReadDelay)

        meas_voltage, meas_current = smu.get_voltage_current()
        adc_voltage, adc_current = smu.get_raw_voltage_current()
        values = [adc_voltage, adc_current, meas_voltage, meas_current]

        print(f"{calibration_point}, MV={meas_voltage}, MI={meas_current}", end='')
        print(': ', end='')
        user_data = input()

        row = list(calibration_point) + values + [str(user_data), '']
        csvwriter.writerow(row)
        cal_rows.append(row)
        csvfile.flush()

        meas_current_cal_data.append((meas_current, Decimal(user_data)))
        set_current_cal_data.append((Decimal(set_current_max), Decimal(user_data)))

    smu.enable(False)

    # linear regression
    print("Current meas calibration")
    meas_cal_factor, meas_cal_offset = regress(
      [float(pt[0]) for pt in meas_current_cal_data], [float(pt[1]) for pt in meas_current_cal_data])

    print("Current set calibration")
    set_cal_factor, set_cal_offset = regress(
      [float(pt[0]) for pt in set_current_cal_data], [float(pt[1]) for pt in set_current_cal_data])

  while True:
    print('Commit to device? [y/n]: ', end='')
    user_data = input()
    if user_data.lower() == 'y':
      break
    elif user_data.lower() == 'n':
      sys.exit()

  smu.cal_set_current_meas(args.irange, meas_cal_factor, meas_cal_offset)
  smu.cal_set_current_set(args.irange, set_cal_factor, set_cal_offset)
  print("Wrote device calibration. Allow 5 seconds to commit to flash before power cycling.")
