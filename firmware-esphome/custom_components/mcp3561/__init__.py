import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
MULTI_CONF = True

CONF_OSR = "osr"

mcp3561_ns = cg.esphome_ns.namespace("mcp3561")
MCP3561 = mcp3561_ns.class_("MCP3561", cg.Component, spi.SPIDevice)

MCP3561Mux = MCP3561.enum("Mux")  # Table 5-1
MUX = {
    "CH0": MCP3561Mux.kCh0,
    "CH1": MCP3561Mux.kCh1,
    "CH2": MCP3561Mux.kCh2,
    "CH3": MCP3561Mux.kCh3,
    "CH4": MCP3561Mux.kCh4,
    "CH5": MCP3561Mux.kCh5,
    "CH6": MCP3561Mux.kCh6,
    "CH7": MCP3561Mux.kCh7,
    "AGND": MCP3561Mux.kAGnd,
    "AVDD": MCP3561Mux.kVdd,
    "REFIN+": MCP3561Mux.kRefInP,
    "REFIN-": MCP3561Mux.kRefInN,
    "TEMPP": MCP3561Mux.kTempDiodeP,
    "TEMPM": MCP3561Mux.kTempDiodeM,
    "VCM": MCP3561Mux.kVCm,
}

MCP3561Osr = MCP3561.enum("Osr")  # Table 5-6
OSR = {
    32: MCP3561Osr.k32,  # 16b, 38400-153600 Hz
    64: MCP3561Osr.k64,  # 19b, 19200-76800 Hz
    128: MCP3561Osr.k128,  # 22b, 9600-38400 Hz
    256: MCP3561Osr.k256,  # 24b, default, 4800-19200 Hz
    512: MCP3561Osr.k512,  # 2400-9600 Hz
    1024: MCP3561Osr.k1024,  # 1200-4800 Hz
    2048: MCP3561Osr.k2048,  # 600-2400 Hz
    4096: MCP3561Osr.k4096,  # 300-1200 Hz
    8192: MCP3561Osr.k8192,  # 150-600 Hz
    16384: MCP3561Osr.k16384,  # 75-300 Hz
    20480: MCP3561Osr.k20480,  # 60-240 Hz
    24576: MCP3561Osr.k24576,  # 50-200 Hz
    40960: MCP3561Osr.k40960,  # 30-120 Hz
    49152: MCP3561Osr.k49152,  # 25-100 Hz
    81920: MCP3561Osr.k81920,  # 15-60 Hz
    98304: MCP3561Osr.k98304,  # 12.5-50 Hz
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MCP3561),
    }
).extend(spi.spi_device_schema(cs_pin_required=True)
).extend(
    {
        cv.Optional(CONF_OSR, default='256'): cv.enum(OSR),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(
      config[CONF_ID],
      config[CONF_OSR],
    )
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
