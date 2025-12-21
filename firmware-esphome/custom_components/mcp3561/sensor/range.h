#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensor/filter.h"
#include "esphome/core/component.h"

using namespace esphome;
namespace range_filter {

class RangeFilter : public sensor::Filter {
 public:
  RangeFilter(size_t window_size, size_t send_every);

  optional<float> new_value(float value) override;

protected:
  size_t window_size_;
  size_t send_every_;

  size_t send_at_ = 0;

  std::deque<float> queue_;
};

}
