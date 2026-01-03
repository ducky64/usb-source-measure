#include "mcp4728.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp4728 {

static const char *const TAG = "mcp4728";

MCP4728::MCP4728() {
}

float MCP4728::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP4728::setup() {
  uint8_t buf[1];
  if (!this->read_bytes_raw(buf, 1)) {
    ESP_LOGE(TAG, "device read failed");
    this->mark_failed();
  }
}

void MCP4728::dump_config() {
  // no config to be dumped
}

void MCP4728::writeChannel(uint8_t channel, uint16_t data, bool upload, Reference ref, bool gain, PowerDown power) {
  if (channel > 3) {
    ESP_LOGE(TAG, "invalid channel %i", channel);
    return;
  }
  if (data > 4095) {
    ESP_LOGE(TAG, "data out of range, clamping: %u", data);
    data = 4095;
  }
  this->write_byte_16((kMultiWriteDac << 3) | (channel << 1) | !upload,
    (ref << 15) | (power << 13) | (gain << 12) | data);
}

MCP4728Output::MCP4728Output(uint8_t channel) : channel_(channel) {};

void MCP4728Output::write_state(float state) {
  uint16_t data = state * 4095 + 0.5;
  if (data > 4095) {
    ESP_LOGE(TAG, "data out of range, clamping: %f", state);
    data = 4095;
  }
  parent_->writeChannel(channel_, data, true);
  this->rawValue = data;
}

}
