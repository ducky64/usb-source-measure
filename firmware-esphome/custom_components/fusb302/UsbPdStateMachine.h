#pragma once

#include "UsbPd.h"
#include "Fusb302.h"


/**
 * A USB PD state machine using the FUSB302 chip, designed for use in a cooperatively multitasked (non-RTOS)
 * system and instantiated as an object in top-level code
 */
class UsbPdStateMachine {
public:
  enum UsbPdState {
    kStart,  // FSM starts here, before FUSB302 is initialized
    kDetectCc,  // alternate measuring CC1/CC2
    kEnableTransceiver,  // reset and enable transceiver
    kWaitSourceCapabilities,  // waiting for initial source capabilities message
    kConnected,  // connected, ready to accept commands
  };

  UsbPdStateMachine(Fusb302& fusb) : fusb_(fusb) {
  }

  // Updates the state machine, handling non-time-sensitive operations.
  // This needs to be called regularly.
  // Returns the current state
  UsbPdState update();

  // Gets the capabilities of the source. Returns the total count, and the unpacked capabilities are
  // stored in the input array.
  // Zero means no source is connected, or capabilities are not yet available.
  // Can return up to kMaxCapabilities=8 objects.
  uint8_t getCapabilities(UsbPd::Capability::Unpacked capabilities[]);

  // The currently active capability requested to and confirmed by the source.
  // Zero means the default (none was requested).
  // 1 is the first capability, consistent with the object position field described in the PD spec.
  uint8_t currentCapability() { return currentCapability_; }
  bool powerStable() { return powerStable_; }

  // Requests a capability from the source.
  bool requestCapability(uint8_t capability, uint16_t currentMa);

  uint8_t getCcPin() { return ccPin_; }

  // Updates the Vbus measurement. Can be called independently of update().
  // Returns true if a valid sample was obtained, false if trying to converge.
  // Call regularly.
  bool updateVbus(uint16_t& vbusOutMv);

  void reset();

  // Resets and initializes the FUSB302 from an unknown state, returning true on success
  // Checks here are minimal, the upper layer should read out the device ID
  // TODO: init should probably not be exposed separately? TBD - needed before updateVbus is valid
  bool init();

protected:
  // Enable the transmitter with the given CC pin (1 or 2, anything else is invalid)
  bool enablePdTrasceiver(int ccPin);

  bool setMeasure(int ccPin);

  bool readMeasure(uint8_t& result);

  bool readComp(bool& result);
  bool setMdac(uint8_t mdacValue);

  bool processRxMessages();

  void processRxSourceCapabilities(uint8_t numDataObjects, uint8_t rxData[]);

  // Requests a capability from the source.
  // 1 is the first capability, consistent with the object position field described in the PD spec.
  bool sendRequestCapability(uint8_t capability, uint16_t currentMa);

  UsbPdState state_ = kStart;

  // CC detection state
  int8_t savedCcMeasureLevel_;  // last measured level of the other CC pin, or -1 if not yet measured
  int8_t measuringCcPin_;  // CC pin currently being measured
  uint8_t ccPin_;  // CC pin used for communication, only valid when connected
  uint32_t stateExpire_;  // millis() time at which the next state expires

  // USB PD state
  uint8_t nextMessageId_;

  uint8_t sourceCapabilitiesLen_ = 0;  // written by ISR
  uint32_t sourceCapabilitiesObjects_[UsbPd::MessageHeader::kMaxDataObjects];  // written by ISR

  // 
  uint8_t requestedCapability_;  // currently requested capability
  uint8_t currentCapability_;  // current accepted capability, 0 is default, written by ISR
  bool powerStable_;
  
  Fusb302& fusb_;

  static const int kMeasureTimeMs = 1;  // TODO arbitrary

  // Vbus measurement binary search algorithm
  int8_t lastMdacValue_ = -1;  // -1 is invalid
  int8_t lastMdacDelta_ = 32;  // delta 
  bool deltaWidening_ = true;  // if true, delta is increasing (broadening search), if false is decreasing (refining search)
  bool lastComp_ = false;
};
