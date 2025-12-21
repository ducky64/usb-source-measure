#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"  // needed to enable ESPHome logging
#include "UsbPd.h"
#include "UsbPdStateMachine.h"

using namespace esphome;  // needed to enable ESPHome logging

static const char* TAG = "UsbPdStateMachine";


UsbPdStateMachine::UsbPdState UsbPdStateMachine::update() {
  switch (state_) {
    case kStart:
    default:
      if (init() && setMeasure(1)) {
        measuringCcPin_ = 1;
        savedCcMeasureLevel_ = -1;
        state_ = kDetectCc;
        stateExpire_ = millis() + kMeasureTimeMs;
      } else {
        ESP_LOGW(TAG, "update(): Start init failed");
      }
      break;
    case kDetectCc:
      if (millis() >= stateExpire_) {  // measurement ready
        uint8_t measureLevel;
        if (readMeasure(measureLevel)) {
          if (savedCcMeasureLevel_ != -1 && measureLevel != savedCcMeasureLevel_) {
            // last measurement on other pin was valid, and one is higher
            if (measureLevel > savedCcMeasureLevel_) {  // this measurement higher, use this CC pin
              ccPin_ = measuringCcPin_;
            } else {  // other measurement higher, use other CC pin
              ccPin_ = measuringCcPin_ == 1 ? 2 : 1;
            }
            ESP_LOGI(TAG, "update(): DetectCC cc=%i", ccPin_);
            state_ = kEnableTransceiver;
          } else {  // save this measurement and swap measurement pins
            uint8_t nextMeasureCcPin = measuringCcPin_ == 1 ? 2 : 1;
            if (setMeasure(nextMeasureCcPin)) {
              savedCcMeasureLevel_ = measureLevel;
              measuringCcPin_ = nextMeasureCcPin;
              stateExpire_ = millis() + kMeasureTimeMs;
            } else {
              ESP_LOGW(TAG, "update(): DetectCc setMeasure failed");
            }
          }
        } else {
          ESP_LOGW(TAG, "update(): DetectCc readMeasure failed");
        }
      }
      break;
    case kEnableTransceiver:
      if (enablePdTrasceiver(ccPin_)) {
        state_ = kWaitSourceCapabilities;
        stateExpire_ = millis() + UsbPdTiming::tTypeCSendSourceCapMsMax;
      } else {
        ESP_LOGW(TAG, "update(): EnableTransceiver enablePdTransceiver failed");
      }
      break;
    case kWaitSourceCapabilities:
      processRxMessages();
      if (sourceCapabilitiesLen_ > 0) {
        state_ = kConnected;
      } else if (millis() >= stateExpire_) {
        ESP_LOGD(TAG, "update(): WaitSourceCapabilities timed out");  // happens quite often if non-PD source connected
        state_ = kEnableTransceiver;
      }
      break;
    case kConnected:
      processRxMessages();
      break;
  }

  return state_;
}

uint8_t UsbPdStateMachine::getCapabilities(UsbPd::Capability::Unpacked capabilities[]) {
  for (uint8_t i=0; i<sourceCapabilitiesLen_; i++) {
    capabilities[i] = UsbPd::Capability::unpack(sourceCapabilitiesObjects_[i]);
  }
  return sourceCapabilitiesLen_;
}

bool UsbPdStateMachine::requestCapability(uint8_t capability, uint16_t currentMa) {
  return sendRequestCapability(capability, currentMa);
}

// runs an iterative binary search with increasing and decreasing levels
bool UsbPdStateMachine::updateVbus(uint16_t& vbusOutMv) {
  uint8_t newMdacValue = 32;  // init
  int8_t convergedMdac = -1;  // if >=0, means converged

  if (lastMdacValue_ >= 0) {  // if previous valid mdac setting
    bool comp;
    if (!readComp(comp)) {
      ESP_LOGW(TAG, "updateVbus(): readComp failed");
      return false;
    }

    if (lastMdacDelta_ == -1 && !lastComp_ && comp) {
      convergedMdac = lastMdacValue_;
      deltaWidening_ = true;  // allow widening after converging
    } else if (lastMdacDelta_ == 1 && lastComp_ && !comp) {
      convergedMdac = lastMdacValue_ - 1;
      deltaWidening_ = true;
    } else if (lastComp_ != comp) {  // crossover point, from widening to narrowing
      deltaWidening_ = false;
    }

    // adjust magnitude value of delta
    lastMdacDelta_ = abs(lastMdacDelta_);
    if (deltaWidening_) {
      if (convergedMdac >= 0) {  // converged, stay at single step
      } else {
        lastMdacDelta_ = min(32, lastMdacDelta_ * 2);
      }
    } else {
      lastMdacDelta_ = max(1, lastMdacDelta_ / 2);
    }

    if (!comp) {  // compare false, search downwards
      lastMdacDelta_ = -lastMdacDelta_;
    }
    newMdacValue = max(0, min(fusb_.kMdacCounts - 1, lastMdacValue_ + lastMdacDelta_));

    lastComp_ = comp;
  }

  if (!setMdac(newMdacValue)) {
    ESP_LOGW(TAG, "updateVbus(): setMdac failed");
  } else {
    lastMdacValue_ = newMdacValue;
  }

  if (convergedMdac >= 0) {
    vbusOutMv = (convergedMdac + 1) * fusb_.kMdacVbusCountMv;
    return true;
  } else {
    return false;
  }
}


void UsbPdStateMachine::reset() {
  state_ = kStart;
  ccPin_ = 0;
  nextMessageId_ = 0;

  sourceCapabilitiesLen_ = 0;

  requestedCapability_ = 0;
  currentCapability_ = 0;
  powerStable_ = false;
}

bool UsbPdStateMachine::init() {
  if (!fusb_.writeRegister(Fusb302::Register::kReset, 0x03)) {  // reset everything
    ESP_LOGW(TAG, "init(): reset failed");
    return false;
  }
  fusb_.startStopDelay();

  if (!fusb_.writeRegister(Fusb302::Register::kPower, 0x0f)) {  // power up everything
    ESP_LOGW(TAG, "init(): power failed");
    return false;
  }
  fusb_.startStopDelay();

  return true;
}

bool UsbPdStateMachine::enablePdTrasceiver(int ccPin) {
  uint8_t switches0Val = 0x03;  // PDWN1/2
  uint8_t switches1Val = 0x24;  // Revision 2.0, auto-CRC
  if (ccPin == 1) {
    switches0Val |= 0x04;
    switches1Val |= 0x01;
  } else if (ccPin == 2) {
    switches0Val |= 0x08;
    switches1Val |= 0x02;
  } else {
    ESP_LOGE(TAG, "invalid ccPin = %i", ccPin);
    return false;
  }

  if (!fusb_.writeRegister(Fusb302::Register::kSwitches0, switches0Val)) {  // PDWN1/2
    ESP_LOGW(TAG, "enablePdTransceiver(): switches0 failed");
    return false;
  }
  fusb_.startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kSwitches1, switches1Val)) {
    ESP_LOGW(TAG, "enablePdTransceiver(): switches1 failed");
    return false;
  }
  fusb_.startStopDelay();
  if (!fusb_.writeRegister(Fusb302::Register::kControl3, 0x07)) {  // enable auto-retry
    ESP_LOGW(TAG, "enablePdTransceiver(): control3 failed");
    return false;
  }
  fusb_.startStopDelay();

  if (!fusb_.writeRegister(Fusb302::Register::kReset, 0x02)) {  // reset PD logic
    ESP_LOGW(TAG, "enablePdTransceiver(): reset failed");
    return false;
  }
  fusb_.startStopDelay();

  return true;
}

bool UsbPdStateMachine::setMeasure(int ccPin) {
  uint8_t switches0Val = 0x03;  // PDWN1/2
  if (ccPin == 1) {
    switches0Val |= 0x04;
  } else if (ccPin == 2) {
    switches0Val |= 0x08;
  } else {
    ESP_LOGW(TAG, "setMeasure(): invalid ccPin arg = %i", ccPin);
    return false;
  }
  if (!fusb_.writeRegister(Fusb302::Register::kSwitches0, switches0Val)) {
    ESP_LOGW(TAG, "setMeasure(): switches0 failed");
    return false;
  }
  fusb_.startStopDelay();

  return true;
}

bool UsbPdStateMachine::readMeasure(uint8_t& result) {
  uint8_t regVal;
  if (!fusb_.readRegister(Fusb302::Register::kStatus0, regVal)) {
    ESP_LOGW(TAG, "readMeasure(): status0 failed");
    return false;
  }
  fusb_.startStopDelay();

  result = regVal & 0x03;  // take BC_LVL only
  return true;
}

bool UsbPdStateMachine::readComp(bool& result) {
  uint8_t regVal;
  if (!fusb_.readRegister(Fusb302::Register::kStatus0, regVal)) {
    ESP_LOGW(TAG, "readMeasure(): status0 failed");
    return false;
  }
  fusb_.startStopDelay();

  result = regVal & 0x20;  // take COMP only
  return true;
}

bool UsbPdStateMachine::setMdac(uint8_t mdacValue) {
  if (!fusb_.writeRegister(Fusb302::Register::kMeasure, 0x40 | (mdacValue & 0x3f))) {  // MEAS_VBUS
    ESP_LOGW(TAG, "setMdac(): failed");
    return false;
  }
  fusb_.startStopDelay();  
  return true;
}

bool UsbPdStateMachine::processRxMessages() {
  while (true) {
    uint8_t status1Val;
    if (!fusb_.readRegister(Fusb302::Register::kStatus1, status1Val)) {
      ESP_LOGW(TAG, "processRxMessages(): readRegister(Status1) failed");
      return false;  // exit on error condition
    }
    fusb_.startStopDelay();
    if (status1Val & Fusb302::kStatus1::kRxEmpty) {  // nothing to do
      return true;
    }

    uint8_t rxData[30];
    if (!fusb_.readNextRxFifo(rxData)) {
      ESP_LOGW(TAG, "processRxMessages(): readNextRxFifo failed");
      return false;  // exit on error condition
    }
    fusb_.startStopDelay();

    uint16_t header = UsbPd::unpackUint16(rxData + 0);
    uint8_t messageId = UsbPd::MessageHeader::unpackMessageId(header);
    uint8_t messageType = UsbPd::MessageHeader::unpackMessageType(header);
    uint8_t messageNumDataObjects = UsbPd::MessageHeader::unpackNumDataObjects(header);
    if (messageNumDataObjects > 0) {  // data message
      ESP_LOGI(TAG, "processRxMessages(): data message: id=%i, type=%03x, numData=%i", 
          messageId, messageType, messageNumDataObjects);
      switch (messageType) {
        case UsbPd::MessageHeader::DataType::kSourceCapabilities: {
          ESP_LOGI(TAG, "processRxMessages(): source capabilities");
          bool isFirstMessage = sourceCapabilitiesLen_ == 0;
          processRxSourceCapabilities(messageNumDataObjects, rxData);
          if (isFirstMessage && sourceCapabilitiesLen_ > 0) {
            UsbPd::Capability::Unpacked v5vCapability = UsbPd::Capability::unpack(sourceCapabilitiesObjects_[0]);
            sendRequestCapability(0, v5vCapability.maxCurrentMa);  // request the vSafe5v capability
          } else {
            // TODO this should be an error
          }
        } break;
        default:  // ignore
          break;
      }
    } else {  // command message
      ESP_LOGI(TAG, "processRxMessages(): command message: id=%i, type=%03x", 
          messageId, messageType);
      switch (messageType) {
        case UsbPd::MessageHeader::ControlType::kAccept:
          ESP_LOGI(TAG, "processRxMessages(): accept");
          currentCapability_ = requestedCapability_;
          break;
        case UsbPd::MessageHeader::ControlType::kReject:
          ESP_LOGI(TAG, "processRxMessages(): reject");
          requestedCapability_ = currentCapability_;
          break;
        case UsbPd::MessageHeader::ControlType::kPsRdy:
          ESP_LOGI(TAG, "processRxMessages(): ready");
          powerStable_ = true;
          break;
        case UsbPd::MessageHeader::ControlType::kGoodCrc:
        default:  // ignore
          break;
      }
    }
  }
}

void UsbPdStateMachine::processRxSourceCapabilities(uint8_t numDataObjects, uint8_t rxData[]) {
  for (uint8_t i=0; i<numDataObjects; i++) {
    sourceCapabilitiesObjects_[i] = UsbPd::unpackUint32(rxData + 2 + 4*i);
    UsbPd::Capability::Unpacked capability = UsbPd::Capability::unpack(sourceCapabilitiesObjects_[i]);
  }
  sourceCapabilitiesLen_ = numDataObjects;
}

bool UsbPdStateMachine::sendRequestCapability(uint8_t capability, uint16_t currentMa) {
  uint16_t header = UsbPd::MessageHeader::pack(UsbPd::MessageHeader::DataType::kRequest, 1, nextMessageId_);

  uint32_t requestData = UsbPd::maskAndShift(capability, 3, 28) |
      UsbPd::maskAndShift(1, 10, 24) |  // no USB suspend
      UsbPd::maskAndShift(currentMa / 10, 10, 10) |
      UsbPd::maskAndShift(currentMa / 10, 10, 0);
  if (fusb_.writeFifoMessage(header, 1, &requestData)) {
    requestedCapability_ = capability;
    powerStable_ = false;
    ESP_LOGI(TAG, "requestCapability(): writeFifoMessage(Request(%i), %i)", capability, nextMessageId_);
  } else {
    ESP_LOGW(TAG, "requestCapability(): writeFifoMessage(Request(%i), %i) failed", capability, nextMessageId_);
    return false;
  }
  nextMessageId_ = (nextMessageId_ + 1) % 8;
  return true;
}
