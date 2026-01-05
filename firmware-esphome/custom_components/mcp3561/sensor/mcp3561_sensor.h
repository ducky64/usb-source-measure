#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/core/component.h"

#include "../mcp3561.h"

using namespace esphome;
namespace mcp3561 {

class MCP3561Sensor : public PollingComponent,
                      public sensor::Sensor,
                      public Parented<MCP3561> {
 public:
  MCP3561Sensor(MCP3561::Mux channel, MCP3561::Mux channel_neg = MCP3561::kAGnd, 
    MCP3561::Gain gain = MCP3561::kX1);

  void update() override;  // requests conversions regularly
  void dump_config() override;
  float get_setup_priority() const override;

  void conversion_result(int32_t adcCounts);  // called by the parent on a conversion result for this sensor

  int32_t rawValue;

  MCP3561::Mux channel_;
  MCP3561::Mux channel_neg_;
  MCP3561::Gain gain_;
};

}
