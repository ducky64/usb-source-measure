#include "UsbPd.h"
#include "Fusb302.h"


bool Fusb302::writeRegister(uint8_t addr, uint8_t data) {
  uint8_t payload[1];
  payload[0] = data;
  return writeRegister(addr, 1, payload);
}

bool Fusb302::writeRegister(uint8_t addr, size_t len, uint8_t data[]) {
  return this->i2c_->write_register(addr, data, len) == esphome::i2c::ERROR_OK;
}

bool Fusb302::readRegister(uint8_t addr, size_t len, uint8_t data[]) {
  return this->i2c_->read_register(addr, data, len) == esphome::i2c::ERROR_OK;
}

bool Fusb302::readRegister(uint8_t addr, uint8_t& dataOut) {
  return readRegister(addr, 1, &dataOut);
}

bool Fusb302::readId(uint8_t& idOut) {
  return readRegister(Register::kDeviceId, idOut);
}

bool Fusb302::writeFifoMessage(uint16_t header, uint8_t numDataObjects, uint32_t data[]) {
  uint8_t buffer[38];  // 4 SOP + 1 pack sym + 2 header + 7x4 data + 1 EOP + 1 TxOff + 1 TxOn
  uint8_t bufInd = 0;
  buffer[bufInd++] = kSopSet[0];
  buffer[bufInd++] = kSopSet[1];
  buffer[bufInd++] = kSopSet[2];
  buffer[bufInd++] = kSopSet[3];
  buffer[bufInd++] = kFifoTokens::kPackSym | (2 + numDataObjects * 4);
  UsbPd::packUint16(header, buffer + bufInd);
  bufInd += 2;
  for (uint8_t i=0; i<numDataObjects; i++) {
    UsbPd::packUint32(data[i], buffer + bufInd);
    bufInd += 4;
  }
  buffer[bufInd++] = Fusb302::kFifoTokens::kJamCrc;
  buffer[bufInd++] = Fusb302::kFifoTokens::kEop;
  buffer[bufInd++] = Fusb302::kFifoTokens::kTxOff;
  buffer[bufInd++] = Fusb302::kFifoTokens::kTxOn;

  return writeRegister(Fusb302::Register::kFifos, 4 + 1 + 2 + (numDataObjects * 4) + 4, buffer);
}

bool Fusb302::readNextRxFifo(uint8_t bufferOut[]) {
  uint8_t buffer[8 * 4 + 4];
  if (this->i2c_->read_register(Register::kFifos, buffer, 3) != esphome::i2c::ERROR_OK) {
    return false;
  }

  if ((buffer[0] & kRxFifoTokenMask) != kRxFifoTokens::kSop) {
    return false;
  }
  bufferOut[0] = buffer[1];
  bufferOut[1] = buffer[2];
  uint16_t header = UsbPd::unpackUint16(bufferOut + 0);
  uint16_t numDataObjects = UsbPd::MessageHeader::unpackNumDataObjects(header);
  uint8_t bufferInd = 0;

  if (this->i2c_->read_register(Register::kFifos, buffer, (uint8_t)(numDataObjects * 4 + 4)) != esphome::i2c::ERROR_OK) {
    return false;
  }
  for (uint8_t i=0; i<numDataObjects * 4; i++) {
    bufferOut[bufferInd + 2] = buffer[bufferInd];
    bufferInd++;
  }
  return true;
}
