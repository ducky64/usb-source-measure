from typing import List
import numpy as np


def regress(xs: List[float], ys: List[float]) -> (float, float):
  """runs the regression, returning (slope, intercept) and printing stats"""
  (slope, intercept), (sse, ), *_ = np.polyfit(xs, ys, deg=1, full=True)
  print(f"  y = {slope}x + {intercept}, sse={sse}")
  for (x, y) in zip(xs, ys):
    predict = slope * float(x) + intercept
    print(f"  {y} => {predict:.4f} ({predict - y:.4f}, {(predict - y) / predict * 100:.2f}%)")
  return slope, intercept
