import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import Fusb302Component, CONF_FUSB302_ID

DEPENDENCIES = ['fusb302']

CONF_STATUS = 'status'
CONF_CAPABILITIES = 'capabilities'

TYPES = [
    CONF_STATUS,
    CONF_CAPABILITIES,
]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_FUSB302_ID): cv.use_id(Fusb302Component),
    cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
      icon="mdi:checkbox-marked-circle-outline"
    ),
    cv.Optional(CONF_CAPABILITIES): text_sensor.text_sensor_schema(
      icon="mdi:checkbox-marked-circle-outline"
    ),
})


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))

async def to_code(config):
    hub = await cg.get_variable(config[CONF_FUSB302_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
