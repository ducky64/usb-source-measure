import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_SOURCE,
)
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.components import web_server_base, sensor
from esphome.cpp_types import EntityBase

CONF_SOURCES = "sources"

AUTO_LOAD = ["web_server_base"]

sample_buffer_ns = cg.esphome_ns.namespace("sample_buffer")
SampleBuffer = sample_buffer_ns.class_("SampleBuffer", cg.Component)

SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_NAME): cv.string_strict,
    },
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SampleBuffer),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
        cv.Required(CONF_SOURCES): cv.ensure_list(SOURCE_SCHEMA),
    },
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    for source_conf in config[CONF_SOURCES]:
        source = await cg.get_variable(source_conf[CONF_SOURCE])
        name = await cg.templatable(
            source_conf[CONF_NAME],
            [(str, "x")],
            cg.float_,
        )
        cg.add(var.add_source(source, name))
