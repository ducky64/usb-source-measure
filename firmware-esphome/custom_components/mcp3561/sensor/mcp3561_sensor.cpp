#include "mcp3561_sensor.h"

#include "esphome/core/log.h"

using namespace esphome;
namespace mcp3561 {

static const char *const TAG = "mcp3561.sensor";

MCP3561Sensor::MCP3561Sensor(MCP3561::Mux channel, MCP3561::Mux channel_neg, 
    MCP3561::Gain gain) : 
    channel_(channel), channel_neg_(channel_neg), gain_(gain) {}

float MCP3561Sensor::get_setup_priority() const { return setup_priority::DATA; }

void MCP3561Sensor::dump_config() {
  LOG_SENSOR("", "MCP3561Sensor Sensor", this);
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Pin neg: %u", this->channel_neg_);
  LOG_UPDATE_INTERVAL(this);
}

void MCP3561Sensor::update() {
  this->parent_->enqueue(this);
}

void MCP3561Sensor::conversion_result(int32_t adcCounts) {
  rawValue = adcCounts;
  this->publish_state(adcCounts / (float)(1 << 23)); 
}

}
