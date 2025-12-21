#include "UsbPd.h"
#include "Fusb302.h"


bool Fusb302::writeRegister(uint8_t addr, uint8_t data) {
  uint8_t payload[1];
  payload[0] = data;
  return writeRegister(addr, 1, payload);
}

bool Fusb302::writeRegister(uint8_t addr, size_t len, uint8_t data[]) {
  wire_.beginTransmission(kI2cAddr);
  wire_.write(addr);
  for (size_t i=0; i<len; i++) {
    wire_.write(data[i]);
  }
  return !wire_.endTransmission();
}

bool Fusb302::readRegister(uint8_t addr, size_t len, uint8_t data[]) {
  if (!writeRegister(addr, 0, NULL)) {
    return false;
  }

  uint8_t reqd = wire_.requestFrom(kI2cAddr, len);
  if (reqd != len) {
    return false;
  }
  for (size_t i=0; i<len; i++) {
    data[i] = wire_.read();
  }
  return true;
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
  if (!writeRegister(Register::kFifos, 0, NULL)) {
    return false;
  }

  wire_.requestFrom(kI2cAddr, (uint8_t)3);  // read and parse the header
  uint8_t sofByte = wire_.read();
  if ((sofByte & kRxFifoTokenMask) != kRxFifoTokens::kSop) {
    wire_.endTransmission();
    return false;
  }
  bufferOut[0] = wire_.read();
  bufferOut[1] = wire_.read();
  uint16_t header = UsbPd::unpackUint16(bufferOut + 0);
  uint16_t numDataObjects = UsbPd::MessageHeader::unpackNumDataObjects(header);
  uint8_t bufferInd = 2;
  if (wire_.endTransmission()) {
    return false;
  }

  wire_.requestFrom(kI2cAddr, (uint8_t)(numDataObjects * 4 + 4));  // read out additional data objects
  for (uint8_t i=0; i<numDataObjects * 4; i++) {
    bufferOut[bufferInd++] = wire_.read();
  }
  for (uint8_t i=0; i<4; i++) {  // drop the CRC bytes, these should be checked by the chip
      wire_.read();
  }
  return !wire_.endTransmission();
}
