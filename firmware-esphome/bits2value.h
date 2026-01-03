const float kVRef = 3.3;
const float kVCenter = 1.65;

// Given an ADC (or DAC) value and calibrations, returns the value in units (eg, volts).
// adc_ratio is signed and in the range [-0.5, 0.5]
float adcToValue(float adc_ratio, float ratio, float factor, float offset) {
    return adc_ratio * kVRef * ratio * factor + offset;
}

// Inverse of adcToValue.
// Note, the current SMU architecture requires a DAC signal opposite (around the center)
// of the measurement target, this inversion must happen elsewhere.
float valueToAdc(float value, float ratio, float factor, float offset) {
    return (value - offset) / factor / ratio / kVRef;
}
