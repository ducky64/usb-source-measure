#pragma once

#include <sstream>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "Fusb302.h"
#include "UsbPdStateMachine.h"


namespace fusb302 {

using namespace esphome;

static const char* TAG = "Fusb302Component";

class Fusb302Component : public Component {
public:
  sensor::Sensor* sensor_cc_ = nullptr;
  void set_cc_sensor(sensor::Sensor* that) { sensor_cc_ = that; }
  sensor::Sensor* sensor_vbus_ = nullptr;
  void set_vbus_sensor(sensor::Sensor* that) { sensor_vbus_ = that; }
  sensor::Sensor* sensor_selected_voltage_ = nullptr;
  void set_selected_voltage_sensor(sensor::Sensor* that) { sensor_selected_voltage_ = that; }
  sensor::Sensor* sensor_selected_current_ = nullptr;
  void set_selected_current_sensor(sensor::Sensor* that) { sensor_selected_current_ = that; }

  text_sensor::TextSensor* sensor_status_ = nullptr;
  void set_status_text_sensor(text_sensor::TextSensor* that) { sensor_status_ = that; }
  text_sensor::TextSensor* sensor_capabilities_ = nullptr;
  void set_capabilities_text_sensor(text_sensor::TextSensor* that) { sensor_capabilities_ = that; }

  void set_target(float target) { targetMv_ = target * 1000; }

  Fusb302Component() : fusb_(Wire), pd_fsm_(fusb_) {
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override {
    if (fusb_.readId(id_)) {
      ESP_LOGI(TAG, "got chip id 0x%02x", id_);
    } else {
      ESP_LOGE(TAG, "failed to read chip id");
      if (sensor_status_ != nullptr) {
        sensor_status_->publish_state("Failed chip ID");
      }
      mark_failed();
      return;
    }

    pd_fsm_.reset();
    pd_fsm_.init();  // initialize the chip, including so it can measure Vbus
  }

  void loop() override {
    if (pd_fsm_.updateVbus(lastVbusMv_)) {
      if (sensor_vbus_ != nullptr) {
        sensor_vbus_->publish_state((float)lastVbusMv_ / 1000);
      }
    }

    UsbPdStateMachine::UsbPdState state = UsbPdStateMachine::kStart;
    if (lastVbusMv_ > 4000) {
      state = pd_fsm_.update();
    } else {  // reset, likely source was disconnected
      pd_fsm_.reset();
      selectedVoltageMv_ = 0;
      if (sensor_selected_voltage_ != nullptr) {
        sensor_selected_voltage_->publish_state(0);
      }
      selectedCurrentMa_ = 0;
      if (sensor_selected_current_ != nullptr) {
        sensor_selected_current_->publish_state(0);
      }
    }

    if (state != last_state_) {
      const char* status;
      switch (state) {
        case UsbPdStateMachine::kStart: status = "Start"; break;
        case UsbPdStateMachine::kDetectCc: status = "Detect CC"; break;
        case UsbPdStateMachine::kEnableTransceiver: status = "Enable Transceiver"; break;
        case UsbPdStateMachine::kWaitSourceCapabilities: status = "Wait Capabilities"; break;
        case UsbPdStateMachine::kConnected: status = "Connected"; break;
      }
      if (sensor_status_ != nullptr) {
        sensor_status_->publish_state(status);
      }
    }

    float cc_state;
    if (last_state_ < UsbPdStateMachine::kEnableTransceiver && state >= UsbPdStateMachine::kEnableTransceiver) {  // cc available
      if (sensor_cc_ != nullptr) {
        sensor_cc_->publish_state(pd_fsm_.getCcPin());
      }
    } else if (last_state_ >= UsbPdStateMachine::kEnableTransceiver && state < UsbPdStateMachine::kEnableTransceiver) {  // disconnected
      if (sensor_cc_ != nullptr) {
        sensor_cc_->publish_state(0);
      }
    }

    if (last_state_ < UsbPdStateMachine::kConnected && state >= UsbPdStateMachine::kConnected) {  // capabilities now available
      std::stringstream ss;
      UsbPd::Capability::Unpacked capabilities[UsbPd::Capability::kMaxCapabilities];
      uint8_t capabilitiesCount = pd_fsm_.getCapabilities(capabilities);
      for (uint8_t capabilityIndex=0; capabilityIndex<capabilitiesCount; capabilityIndex++) {
        UsbPd::Capability::Unpacked& capability = capabilities[capabilityIndex];
        if (capabilityIndex > 0) {
          ss << "; ";
        }
        if (capability.capabilitiesType == UsbPd::Capability::Type::kFixedSupply) {
          ss << capability.voltageMv / 1000 << "." << (capability.voltageMv / 100) % 10 <<  "V " 
            << capability.maxCurrentMa / 1000 << "." << (capability.maxCurrentMa / 100) % 10 << "A" ;
          if (capability.dualRolePower) {
            ss << " DR";
          }
          if (!capability.unconstrainedPower) {
            ss << " C";
          }
        } else if (capability.capabilitiesType == UsbPd::Capability::Type::kVariable) {
          ss << "unk variable";
        } else if (capability.capabilitiesType == UsbPd::Capability::Type::kAugmented) {
          ss << "unk augmented";
        } else if (capability.capabilitiesType == UsbPd::Capability::Type::kBattery) {
          ss << "unk battery";
        }
      }
      if (sensor_capabilities_ != nullptr) {
        sensor_capabilities_->publish_state(ss.str());
      }
    } else if (last_state_ >= UsbPdStateMachine::kConnected && state < UsbPdStateMachine::kConnected) {  // disconnected
      if (sensor_capabilities_ != nullptr) {
        sensor_capabilities_->publish_state("");
      }
    }

    // update current capability if not yet set, once the power is stable
    if (selectedVoltageMv_ == 0 && state == UsbPdStateMachine::kConnected && pd_fsm_.powerStable()) {
      UsbPd::Capability::Unpacked capabilities[UsbPd::Capability::kMaxCapabilities];
      pd_fsm_.getCapabilities(capabilities);
      uint8_t currentCapability = pd_fsm_.currentCapability();
      if (currentCapability > 0) {
        UsbPd::Capability::Unpacked capability = capabilities[currentCapability - 1];
        selectedVoltageMv_ = capability.voltageMv;
        if (sensor_selected_voltage_ != nullptr) {
          sensor_selected_voltage_->publish_state((float)selectedVoltageMv_ / 1000);
        }
        selectedCurrentMa_ = capability.maxCurrentMa;  // TODO not necessarily requested
        if (sensor_selected_current_ != nullptr) {
          sensor_selected_current_->publish_state((float)selectedCurrentMa_ / 1000);
        }
      }
    }

    // request capability a cycle after the connection - otherwise the loop blocks for a bit
    if (last_state_ == UsbPdStateMachine::kConnected && state == UsbPdStateMachine::kConnected 
        && pd_fsm_.currentCapability() == 0) {
      uint8_t selectCapability = 0;
      uint16_t lastBestVoltageMv = 0;
      uint16_t lastBestCurrentMa = 0;

      UsbPd::Capability::Unpacked capabilities[UsbPd::Capability::kMaxCapabilities];
      uint8_t capabilitiesCount = pd_fsm_.getCapabilities(capabilities);
      for (uint8_t capabilityIndex=0; capabilityIndex<capabilitiesCount; capabilityIndex++) {
        UsbPd::Capability::Unpacked& capability = capabilities[capabilityIndex];
        if (capability.voltageMv <= targetMv_ && capability.voltageMv > lastBestVoltageMv) {
          selectCapability = capabilityIndex;
          lastBestVoltageMv = capability.voltageMv;
          lastBestCurrentMa = capability.maxCurrentMa;
        }
      }
      if (selectCapability > 0) {
        if (pd_fsm_.requestCapability(selectCapability + 1, lastBestCurrentMa)) {  // note, 1-indexed
          ESP_LOGI(TAG, "request capability %i at %i mA", selectCapability, lastBestCurrentMa);
        } else {
          ESP_LOGW(TAG, "request capability %i at %i mA failed", selectCapability, lastBestCurrentMa);
        }
        selectedVoltageMv_ = 0;
        if (sensor_selected_voltage_ != nullptr) {
          sensor_selected_voltage_->publish_state(0);
        }
        selectedCurrentMa_ = 0;
        if (sensor_selected_current_ != nullptr) {
          sensor_selected_current_->publish_state(0);
        }
      }
    }

    last_state_ = state;
  }

  UsbPdStateMachine::UsbPdState get_state() { return last_state_; }

protected:
  Fusb302 fusb_;
  UsbPdStateMachine pd_fsm_;
  UsbPdStateMachine::UsbPdState last_state_ = UsbPdStateMachine::kStart;

  uint8_t id_ = 0;  // device id, if read successful
  uint16_t lastVbusMv_ = 0;

  uint16_t selectedVoltageMv_ = 0;  // only set when power is stable, zero when power is unstable or new request being made
  uint16_t selectedCurrentMa_ = 0;  // ditto
  uint16_t targetMv_ = 0;
};

}
