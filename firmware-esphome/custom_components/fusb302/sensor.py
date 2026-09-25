import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY, UNIT_VOLT, UNIT_AMPERE
from . import Fusb302Component, CONF_FUSB302_ID

DEPENDENCIES = ['fusb302']

CONF_CC = 'cc'
CONF_VBUS = 'vbus'
CONF_SELECTED_VOLTAGE = 'selected_voltage'
CONF_SELECTED_CURRENT = 'selected_current'

TYPES = [
    CONF_CC,
    CONF_VBUS,
    CONF_SELECTED_VOLTAGE,
    CONF_SELECTED_CURRENT,
]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_FUSB302_ID): cv.use_id(Fusb302Component),
    cv.Optional(CONF_ID): sensor.sensor_schema(
      unit_of_measurement=UNIT_EMPTY
    ),
    cv.Optional(CONF_CC): sensor.sensor_schema(
      unit_of_measurement=UNIT_EMPTY
    ),
    cv.Optional(CONF_VBUS): sensor.sensor_schema(
      unit_of_measurement=UNIT_VOLT,
      accuracy_decimals=1,  # 0.42v resolution in Vbus mode, last digit is uncertain
    ),
    cv.Optional(CONF_SELECTED_VOLTAGE): sensor.sensor_schema(
      unit_of_measurement=UNIT_VOLT,
      accuracy_decimals=0
    ),
    cv.Optional(CONF_SELECTED_CURRENT): sensor.sensor_schema(
      unit_of_measurement=UNIT_AMPERE,
      accuracy_decimals=1
    ),
})


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))

async def to_code(config):
    hub = await cg.get_variable(config[CONF_FUSB302_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
