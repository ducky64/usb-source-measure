import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import CONF_ID, CONF_CHANNEL, UNIT_VOLT, STATE_CLASS_MEASUREMENT, DEVICE_CLASS_VOLTAGE

from .. import mcp3561_ns, MCP3561, MUX

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["mcp3561"]

CONF_CHANNEL_NEG = "channel_neg"

MCP3561Sensor = mcp3561_ns.class_(
    "MCP3561Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    voltage_sampler.VoltageSampler,
    cg.Parented.template(MCP3561),
)
CONF_MCP3561_ID = "mcp3561_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
      MCP3561Sensor,
      unit_of_measurement=UNIT_VOLT,
      accuracy_decimals=7,
      state_class=STATE_CLASS_MEASUREMENT,
      device_class=DEVICE_CLASS_VOLTAGE,
    ).extend(
        {
            cv.GenerateID(CONF_MCP3561_ID): cv.use_id(MCP3561),
            cv.Required(CONF_CHANNEL): cv.enum(MUX, upper=True),
            cv.Optional(CONF_CHANNEL_NEG, default="AGND"): cv.enum(MUX, upper=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_CHANNEL],
        config[CONF_CHANNEL_NEG],
    )
    await cg.register_parented(var, config[CONF_MCP3561_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)


from esphome.const import CONF_WINDOW_SIZE, CONF_SEND_EVERY
from esphome.components.sensor import Filter, FILTER_REGISTRY
range_filter_ns = cg.esphome_ns.namespace("range_filter")
RangeFilter = range_filter_ns.class_("RangeFilter", Filter)

RANGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_WINDOW_SIZE, default=5): cv.positive_not_null_int,
            cv.Optional(CONF_SEND_EVERY, default=5): cv.positive_not_null_int,
        }
    ),
)

@FILTER_REGISTRY.register("range", RangeFilter, RANGE_SCHEMA)
async def min_filter_to_code(config, filter_id):
    return cg.new_Pvariable(
        filter_id,
        config[CONF_WINDOW_SIZE],
        config[CONF_SEND_EVERY],
    )
