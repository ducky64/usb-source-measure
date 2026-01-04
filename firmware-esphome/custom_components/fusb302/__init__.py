import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

AUTO_LOAD = ['sensor', 'text_sensor', 'binary_sensor']
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_FUSB302_ID = 'fusb302_id'
CONF_TARGET_VOLTAGE = 'target'

fusb302_ns = cg.esphome_ns.namespace('fusb302')

Fusb302Component = fusb302_ns.class_('Fusb302Component', cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
  cv.Schema({
    cv.GenerateID(): cv.declare_id(Fusb302Component),
    cv.Optional(CONF_TARGET_VOLTAGE, default="5v"): cv.voltage,
  })
  .extend(i2c.i2c_device_schema(0x22))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_target(config[CONF_TARGET_VOLTAGE]))
