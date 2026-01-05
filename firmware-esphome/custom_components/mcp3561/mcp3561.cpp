#include "mcp3561.h"
#include "sensor/mcp3561_sensor.h"
#include "esphome/core/log.h"

using namespace esphome;
namespace mcp3561 {

static const char *const TAG = "mcp3561";

MCP3561::MCP3561(Osr osr, uint8_t device_address) :
  osr_(osr), device_address_(device_address) {}

float MCP3561::get_setup_priority() const { return setup_priority::HARDWARE; }

void MCP3561::setup() {
  this->spi_setup();

  uint8_t reservedVal = readReg(Register::RESERVED, 2);
  if (reservedVal == 0x000c) {
    ESP_LOGI(TAG, "Detected MCP3561");
  } else if (reservedVal == 0x000d) {
    ESP_LOGI(TAG, "Detected MCP3562");
  } else if (reservedVal == 0x000f) {
    ESP_LOGI(TAG, "Detected MCP3564");
  } else {
    ESP_LOGW(TAG, "MCP356x unexpected Reserved (device ID) value %04x", reservedVal);
  }

  writeReg8(Register::CONFIG0, 0x22);  // external VREF, internal clock w/ no CLK out, ADC standby
  writeReg8(Register::CONFIG1, (this->osr_ & 0xf) << 2);

  writeReg8(Register::CONFIG3, 0x80);  // one-shot conversion into standby, 24b encoding
  writeReg8(Register::IRQ, 0x07);  // enable fast command and start-conversion IRQ, IRQ logic high)
}

void MCP3561::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3561:");
  LOG_PIN("  CS Pin:", this->cs_);
  ESP_LOGCONFIG(TAG, "  OSR: %u", this->osr_);
}

// sends a fast command, returning the status code
uint8_t MCP3561::fastCommand(FastCommand fastCommandCode) {
  this->enable();
  uint8_t status = this->transfer_byte(
    ((this->device_address_ & 0x3) << 6) | ((fastCommandCode & 0xf) << 2) | CommandType::kFastCommand);
  this->disable();

  return status;
}

// tries to reads the ADC as a signed 24-bit value, returning whether the ADC had new data
// does NOT start a new conversion
bool MCP3561::readRaw24(int32_t* outValue) {
  bool valid = false;

  this->enable();
  uint8_t status = this->transfer_byte(
    ((this->device_address_ & 0x3) << 6) | ((Register::ADCDATA & 0xf) << 2) | CommandType::kStaticRead);
  if ((status & 0x04) == 0) {  // STAT[2] /DataReady
    uint8_t result;
    result = this->transfer_byte(0);
    *outValue = result;
    result = this->transfer_byte(0);
    *outValue = *outValue << 8 | result;
    result = this->transfer_byte(0);
    *outValue = *outValue << 8 | result;
    if (*outValue & (1 << 23)) {  // sign extend
      *outValue |= (int32_t)0xff << 24;
    }
    valid = true;
  }
  this->disable();

  return valid;
}

// writes 8 bits into a single register, returning the status code
uint8_t MCP3561::writeReg8(uint8_t regAddr, uint8_t data) {
  this->enable();
  uint8_t result = this->transfer_byte(((this->device_address_ & 0x3) << 6) | ((regAddr & 0xf) << 2) | CommandType::kIncrementalWrite);
  this->transfer_byte(data);
  this->disable();
  return result;
}

uint32_t MCP3561::readReg(uint8_t regAddr, uint8_t bytes) {
  uint32_t out = 0;
  this->enable();
  this->transfer_byte(((this->device_address_ & 0x3) << 6) | ((regAddr & 0xf) << 2) | CommandType::kStaticRead);
  for (uint8_t i=0; i<bytes; i++) {
    uint8_t result = this->transfer_byte(0);
    out = out << 8;
    out |= result;
  }
  this->disable();
  return out;
}

void MCP3561::start_conversion(MCP3561Sensor* sensor) {
  writeReg8(Register::CONFIG2, 
    (2 << 6) |  // default BOOST
    ((sensor->gain_ & 7) << 3) | 
    (1 << 1) |  // default AZ_REF EN
    (1)  // RESERVED=1
  );
  writeReg8(Register::MUX, ((sensor->channel_ & 0xf) << 4) | (sensor->channel_neg_ & 0xf));
  fastCommand(FastCommand::kStartConversion);
  conversionStartMillis_ = esphome::millis();
}

void MCP3561::enqueue(MCP3561Sensor* sensor) {
  if (((queueWrite_ + 1) % kQueueDepth) == queueRead_) {
    ESP_LOGE(TAG, "queue full, dropping request");
    return;
  }
  // don't allow enqueueing duplicates - silently discard if there is a fundamental rate mismatch
  if (queueRead_ <= queueWrite_) {  // doesn't wraparound
    for (size_t i=queueRead_; i<queueWrite_; i++) {
      if (queue_[i] == sensor) {
        return;
      }
    }
  } else {  // wraps around
    for (size_t i=queueRead_; i<kQueueDepth; i++) {
      if (queue_[i] == sensor) {
        return;
      }
    }
    for (size_t i=0; i<queueWrite_; i++) {
      if (queue_[i] == sensor) {
        return;
      }
    }
  }


  queue_[queueWrite_] = sensor;
  if (queueWrite_ == queueRead_) {  // queue empty, start a conversion
    start_conversion(sensor);
  }
  queueWrite_ = (queueWrite_ + 1) % kQueueDepth;
}

void MCP3561::loop() {
  if (queueWrite_ == queueRead_) {  // queue empty, no conversion in progress
    return;
  }

  size_t prevQueueRead = queueRead_;
  int32_t result;
  if (this->readRaw24(&result)) {
    queue_[queueRead_]->conversion_result(result);
    queueRead_ = (queueRead_ + 1) % kQueueDepth;
  } else {
    if ((esphome::millis() - conversionStartMillis_) >= 1000) {
      ESP_LOGE(TAG, "conversion timed out");
      queueRead_ = (queueRead_ + 1) % kQueueDepth;
    }
  }

  if (queueRead_ != prevQueueRead && queueWrite_ != queueRead_) {  // finished conversion and another is in the queue
    start_conversion(queue_[queueRead_]);
  }
}

}
