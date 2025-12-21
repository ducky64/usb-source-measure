#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

using namespace esphome;
namespace mcp4728 {

class MCP4728 : public Component, public i2c::I2CDevice {
 public:
  enum WriteCommand {  // 5-bit C2:0, W1:0, where LSb is W0
    kFastWrite = 0x00,  // only two MSbits are relevant, rest of the 'command field' is payload
    kMultiWriteDac = 0x08,
    kSingleWriteDacEeprom = 0x0b,
  };

  enum PowerDown {
    kNormal = 0,
    kPowerdown1kGnd = 1,  // powered down with Vout loaded with 1k to GND
    kPowerdown100kGnd = 2,  // above, but 100k to GND
    kPowerdown500kGnd = 3,  // above, but 500k to GND
  };

  enum Reference {
    kExternalVref = 0,  // Vdd
    kInternalVref = 1,  // 2.048v internal (2.007-2.089, +/- 2 %)
  };

  MCP4728();

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  
  void writeChannel(uint8_t channel, uint16_t data, bool upload = true, Reference ref = kExternalVref, bool gain = false, PowerDown power = kNormal);
};

class MCP4728Output : public output::FloatOutput,
                      public Parented<MCP4728> {
 public:
  MCP4728Output(uint8_t channel);

  void write_state(float state) override;

 protected:
  uint8_t channel_;
};

}
