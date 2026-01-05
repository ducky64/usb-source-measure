#include "sample_buffer.h"
#include "esphome/core/application.h"

namespace sample_buffer {

bool SampleBuffer::canHandle(AsyncWebServerRequest *request) const {
  if (request->method() == HTTP_GET) {
    if (request->url() == "/samples")
      return true;
  }

  return false;
}

void SampleBuffer::handleRequest(AsyncWebServerRequest *req) {
  AsyncResponseStream *stream = req->beginResponseStream("text/plain; version=0.0.4; charset=utf-8");

  if (req->hasArg("start")) {  // only return samples if the start index is provided
    char* endptr;
    long start = std::strtol(req->arg("start").c_str(), &endptr, 10);
    
    if (*endptr != '\0' || start < 0) {  // buffer underrun or invalid conversion
      req->send(stream);
      return;
    }

    // dump samples after (in memory space) recordsEnd_ / before (by sample index) recordsOffset_, if available
    if (recordsOffset_ > 0) {
      int startIndex = (int64_t)start - (int64_t)(recordsOffset_ - kBufferSize);
      if (startIndex < (int)recordsEnd_) {  // buffer underrun
        req->send(stream);
        return;
      }
      for (size_t i=startIndex; i<kBufferSize; i++) {
        write_sample(stream, records_[i]);
      }
    }

    // dump samples past recordsOffset_
    int startIndex = start - recordsOffset_;
    if (startIndex < 0) {
      startIndex = 0;
    }
    for (size_t i=startIndex; i<recordsEnd_; i++) {
      write_sample(stream, records_[i]);
    }
  }

  // dump the next sample index
  stream->print(recordsOffset_ + recordsEnd_);

  req->send(stream);
}

void SampleBuffer::write_sample(AsyncResponseStream *stream, SampleRecord& sample) {
  stream->print(sample.millis);
  stream->print(",");
  stream->print(names_[sample.sourceIndex].c_str());
  stream->print(",");
  stream->print(sample.value);
  stream->print("\n");
}

void SampleBuffer::add_source(sensor::Sensor *source, const std::string &name) {
  size_t sourceIndex = names_.size();
  names_.push_back(name);  // create a local copy

  source->add_on_state_callback(
    [this, source, sourceIndex](float value) -> void { 
      uint32_t timestamp = esphome::millis();
      int8_t accuracyDecimals = source->get_accuracy_decimals();
      this->new_value(sourceIndex, timestamp, value, accuracyDecimals);
    }
  );
}

void SampleBuffer::new_value(size_t sourceIndex, uint32_t millis, float value, int8_t accuracyDecimals) {
  records_[recordsEnd_].millis = millis;
  records_[recordsEnd_].value = value;
  records_[recordsEnd_].sourceIndex = sourceIndex;
  records_[recordsEnd_].accuracyDecimals = accuracyDecimals;
  recordsEnd_++;
  if (recordsEnd_ >= kBufferSize) {
    recordsEnd_ -= kBufferSize;
    recordsOffset_ += kBufferSize;
  }
}

}
