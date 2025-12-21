#include "range.h"

using namespace esphome;
namespace range_filter {

RangeFilter::RangeFilter(size_t window_size, size_t send_every)
    : window_size_(window_size), send_every_(send_every) {}

optional<float> RangeFilter::new_value(float value) {
  while (this->queue_.size() >= this->window_size_) {
    this->queue_.pop_front();
  }
  this->queue_.push_back(value);

  this->send_at_++;
  if (this->send_at_ >= this->send_every_) {
    this->send_at_ = 0;

    float min = NAN, max = NAN;
    for (auto v : this->queue_) {
      if (!std::isnan(v)) {
        max = std::isnan(max) ? v : std::max(max, v);
        min = std::isnan(min) ? v : std::min(min, v);
      }
    }

    ESP_LOGVV(TAG, "RangeFilter(%p)::new_value(%f) SENDING %f", this, value, max);
    return max - min;
  } else {
    return {};  
  }
}

}
