import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import MCP4728, mcp4728_ns

AUTO_LOAD = ["output"]
DEPENDENCIES = ["mcp4728"]

MCP4728Output = mcp4728_ns.class_("MCP4728Output", output.FloatOutput)
CONF_MCP4728_ID = "mcp4728_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(MCP4728Output),
        cv.GenerateID(CONF_MCP4728_ID): cv.use_id(MCP4728),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_CHANNEL]
        )
    await cg.register_parented(var, config[CONF_MCP4728_ID])
    await output.register_output(var, config)
