#pragma once

#include <map>
#include <utility>

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/controller.h"
#include "esphome/core/entity_base.h"

using namespace esphome;

namespace sample_buffer {

const size_t kBufferSize = 4096;

struct SampleRecord {
  size_t sourceIndex;
  uint32_t millis;
  float value;
  int8_t accuracyDecimals;
};

// Records and timestamps samples from sensors as they come in, and stores them into a circular buffer
// Exposes a HTTP API to access this buffer, by specifying a start sample index
// If no start sample index is specified, returns the next sample index
// If the start sample index is no longer available (buffer underrun), returns empty
// Otherwise, returns all the samples in the buffer (starting with and including start), as lines of
//   (timestamp in millis),(sensor name),(value)
// and ends with the next sample index
// If the start sample is beyond the buffer, returns just the next sample index
class SampleBuffer : public AsyncWebHandler, public Component {
 public:
  SampleBuffer(web_server_base::WebServerBase *base) : base_(base) {}

  // Add a sensor source to this sample buffer
  void add_source(sensor::Sensor *source, const std::string &name);

  bool canHandle(AsyncWebServerRequest *request);

  void handleRequest(AsyncWebServerRequest *req) override;

  void setup() override {
    this->base_->init();
    this->base_->add_handler(this);
  }
  float get_setup_priority() const override {
    // After WiFi
    return setup_priority::WIFI - 1.0f;
  }

 protected:
  web_server_base::WebServerBase *base_;

  void new_value(size_t sourceIndex, uint32_t millis, float value, int8_t accuracyDecimals);

  inline void write_sample(AsyncResponseStream *stream, SampleRecord& sample);

  std::vector<std::string> names_;  // stores a local copy of names

  SampleRecord records_[kBufferSize];
  size_t recordsEnd_ = 0;  // one past the end of records (if recordsOffset_ == 0), or the pointer to the first sample before recordsOffset_
  size_t recordsOffset_ = 0;  // records_[0] is this sample index
  // note: if recordsEnd_ == 0 and recordsOffset_ == 0, this means the buffer is empty
  // if recordsEnd_ != 0 and recordsOffset_ == 0, this means the buffer has recordsEnd_ elements
  // if recordsOffset_ != 0, this means the buffer wraps around at one before recordsEnd_ and the buffer is full
  //   at recordsEnd_ (including 0), the sample index is from the last batch (subtract kBufferSize from the expected sample index)
};

}
