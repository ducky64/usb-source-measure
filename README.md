# usb-source-measure
Firmware and other supporting files for the USB-PD powered SMU

Board files are part of the PCB HDL project: https://github.com/BerkeleyHCI/PolymorphicBlocks/tree/master/examples/UsbSourceMeasure

## Pinning

For version 3.1.

- mcu
  - touch_duck=TOUCH13, 21
  - vconv_sense=ADC1_CH7, 12
  - adc_spi=SPI2
  - adc_spi.sck=GPIO9, 17
  - adc_spi.mosi=GPIO10, 18
  - adc_spi.miso=GPIO11, 19
  - int_i2c=I2CEXT0
  - int_i2c.scl=GPIO15, 8
  - int_i2c.sda=GPIO7, 7
  - qwiic=I2CEXT1
  - qwiic.scl=GPIO1, 39
  - qwiic.sda=GPIO2, 38
  - pd_int=GPIO16, 9
  - buck_pwm=GPIO17, 10
  - boost_pwm=GPIO18, 11
  - enc_a=GPIO42, 35
  - enc_b=GPIO41, 34
  - enc_sw=GPIO40, 33
  - dut0=GPIO47, 24
  - dut1=GPIO48, 25
  - rgb=GPIO38, 31
  - fan=GPIO39, 3
  - adc_cs=GPIO3, 15
  - adc_clk=GPIO12, 20
  - limit_source=GPIO14, 22
  - limit_sink=GPIO21, 23
  - speaker=I2S0
  - speaker.sck=GPIO5, 5
  - speaker.ws=GPIO6, 6
  - speaker.sd=GPIO4, 4
  - 0=USB
  - 0.dp=GPIO20, 14
  - 0.dm=GPIO19, 13
- ioe_ctl (addr_lsb=0, addr=d56)
  - high_gate=IO6, 11
  - low_gate=IO7, 12
  - ramp=IO0, 4
  - conv_en=IO2, 6
  - conv_en_sense=IO3, 7
  - off_0=IO4, 9
- ioe_ui (addr_lsb=2, addr=58)
  - dir_a=IO0, 4
  - dir_b=IO7, 12
  - dir_c=IO2, 6
  - dir_d=IO3, 7
  - dir_cen=IO1, 5
  - irange_0=IO6, 11
  - irange_1=IO4, 9
  - irange_2=IO5, 10
- vusb_sense (addr_lsb=0, addr=64)
- convin_sense (addr_lsb=4, addr=68)
