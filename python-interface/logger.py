import argparse
import csv
import sys
import time
from typing import List, Tuple, Optional
import decimal

from SmuInterface import SmuInterface


kCsvCols = ['s', 'V', 'A', 'Ah', 'J']
kAggregateMillis = 150  # max ms to consider samples part of the same row
kNewRowCols = ['V', 'A']  # when a new sample overwrites one of these cols, starts a new row


if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SMU CSV Logger')
  parser.add_argument('addr', type=str)
  parser.add_argument('name_prefix', type=str)
  parser.add_argument('--delay_on', action='store_true', help="don't store until current is non-NaN")
  parser.add_argument('--auto_stop', action='store_true', help="stop on first current NaN")

  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  mac = smu.get_mac()
  print(f"Connected: {mac}")
  mac_postfix = mac.replace(':', '').lower()

  samplebuf = smu.sample_buffer()
  assert samplebuf.get() == []  # initial get to set the sample index, should be empty

  filename = f"{args.name_prefix}_{mac_postfix}.csv"

  with open(filename, 'w', newline='') as csvfile:
    csvwriter = csv.writer(csvfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
    csvwriter.writerow(kCsvCols)
    csvfile.flush()

    samples_valid: bool = False
    if not args.delay_on:
      samples_valid = True
    start_millis: Optional[int] = None
    last_row: Optional[Tuple[int, List[str]]] = None

    done = False

    while not done:
      samples = samplebuf.get()
      for sample in samples:
        if not samples_valid:
          if sample.source == 'A' and not sample.value.is_nan():
            print("Start recording")
            samples_valid = True
          else:
            continue
        if samples_valid:
          if sample.source == 'A' and sample.value.is_nan() and args.auto_stop:
            print("Done")
            done = True
            sys.exit(0)

        sample_col = kCsvCols.index(sample.source)

        if start_millis is None:
          start_millis = sample.millis
        if last_row is not None and ((last_row[0] + kAggregateMillis < sample.millis)
          or (last_row[1][sample_col] != '' and sample.source in kNewRowCols)):
          csvwriter.writerow([decimal.Decimal(last_row[0] - start_millis) / 1000] + last_row[1][1:])
          last_row = None
        if last_row is None:
          last_row = (sample.millis, [''] * len(kCsvCols))
        last_row[1][sample_col] = sample.value

      csvfile.flush()

      if samples:
        print(f"Got {len(samples)} samples")
      time.sleep(1)
