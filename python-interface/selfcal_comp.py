import argparse
import sys
import time
from typing import Tuple, List

import numpy as np

from SmuInterface import SmuInterface


kCalPoints = [  # as voltage target, source limit, sink limit
    (0, -0.1, 0.1),
    (0, -0.1, 0.5),
    (0, -0.1, 1),
    (0, -0.1, 2),
    (0, -0.5, 0.1),
    (0, -1, 0.1),
    (0, -2, 0.1),
    (0, -1, 1),
    (0, -2, 1),
    (0, -1, 2),

    # common mode cal
    (2, -1, 1),
    (2, -2, 1),
    (2, -1, 2),
    (6, -1, 1),
    (6, -2, 1),
    (6, -1, 2),
]

kSetReadDelay = 0.3  # seconds


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='SmuCal')
    parser.add_argument('addr', type=str)
    args = parser.parse_args()

    smu = SmuInterface(args.addr)

    source_factor = smu.cal_get(smu.kNameCalCurrentSetSourceFactor)
    sink_factor = smu.cal_get(smu.kNameCalCurrentSetSinkFactor)
    common_factor = smu.cal_get(smu.kNameCalCurrentCommonFactor)
    print(f'Current comp cal: {source_factor}x source + {sink_factor}x sink + {common_factor}x common-mode')

    while True:
        print('Clear and re-run calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    currents: List[float] = []
    data: List[Tuple[float, float, float]] = []  # imin, imax, dv
    try:
        smu.enable(True, "3A")
        for set_voltage, set_limit_source, set_limit_sink in kCalPoints:
            smu.set_voltage(set_voltage)
            smu.set_current_limits(set_limit_source, set_limit_sink)
            time.sleep(kSetReadDelay)

            adc_ratio_current = float(smu._get('sensor', smu.kNameMeasRatioCurrent)) - 0.5
            delta_current_min = adc_ratio_current - float(smu._get('sensor', smu.kNameSetRatioCurrentMin))
            delta_current_max = adc_ratio_current - float(smu._get('sensor', smu.kNameSetRatioCurrentMax))
            delta_voltage = float(smu._get('sensor', smu.kNameMeasRatioVoltage)) - 0.5
            currents.append(-adc_ratio_current)
            data.append((delta_current_min,
                         delta_current_max,
                         delta_voltage))

            print(f"Iadc={adc_ratio_current}, dImin={delta_current_min}, dImax={delta_current_max}, dV={delta_voltage}")
    finally:
        smu.enable(False)

    """Create a matrix A x = b for multivariate least squares
    where
    A is the setpoints with constant 1 for offset
    x is the calibration coefficients
    b is the measured currents
    """
    a = np.stack([list(row) + [1.0] for row in data])
    b = np.array(currents)
    x, residuals, rank, s = np.linalg.lstsq(a, b, rcond=None)

    sink_factor, source_factor, common_factor, offset = x
    print(f'New comp cal: {source_factor}x source + {sink_factor}x sink + {common_factor}x common')
    print(f'  + {offset} offset')
    print(f'  residuals: {residuals}')
    while True:
        print('Update calibration? [y/n]: ', end='')
        user_input = input()
        if user_input.lower() == 'y':
            break
        elif user_input.lower() == 'n':
            sys.exit()

    smu.cal_set(smu.kNameCalCurrentSetSourceFactor, source_factor)
    smu.cal_set(smu.kNameCalCurrentSetSinkFactor, sink_factor)
    smu.cal_set(smu.kNameCalCurrentCommonFactor, common_factor)
