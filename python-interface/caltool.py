import argparse
import json
from decimal import Decimal

from SmuInterface import SmuInterface


if __name__ == "__main__":
  parser = argparse.ArgumentParser(prog='SmuCal')
  parser.add_argument('addr', type=str)
  parser.add_argument('name_prefix', type=str, nargs='?')
  parser.add_argument('--restore', action='store_true')
  parser.add_argument('--dump', action='store_true')
  args = parser.parse_args()

  smu = SmuInterface(args.addr)

  mac = smu.get_mac()
  print(f"Connected: {mac}")
  mac_postfix = mac.replace(':', '').lower()

  cal_dict = smu.cal_get_all()
  print("Current calibration:")
  for key, value in cal_dict.items():
    print(f"  {key}: {value}")

  assert not args.restore or not args.dump, "cannot simultaneously dump and restore"
  filename = f"{args.name_prefix}_{mac_postfix}.json"

  if args.restore:
    assert args.name_prefix, "name_prefix required"
    with open(filename, 'r') as f:
      cal_str_dict = json.load(f)
    cal_dict = {key: Decimal(value) for key, value in cal_str_dict.items()}
    smu.cal_set_all(cal_dict)

    print(f"Restored from {filename}")

  if args.dump:
    assert args.name_prefix, "name_prefix required"
    cal_str_dict = {key: str(value) for key, value in cal_dict.items()}
    jsonstr = json.dumps(cal_str_dict, indent=2)
    with open(filename, 'w') as f:
      f.write(jsonstr)

    print(f"Wrote to {filename}")
