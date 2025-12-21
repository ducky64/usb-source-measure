from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
from esphome.components.adc import sensor
import esphome.codegen as cg
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_PIN,
)

# the default cv time processing only supports down to us, so this custom cv is needed to go down to ns
cv_time_ns = cv.float_with_unit("second", "s")

DEPENDENCIES = ["esp32"]

CONF_PIN_COMP = 'pin_comp'  # complementary pin
CONF_DEADTIME_RISING = 'deadtime_rising'
CONF_DEADTIME_FALLING = 'deadtime_falling'
CONF_INVERTED = 'inverted'
CONF_MAX_DUTY = 'max_duty'

CONF_SYNC_ADC = 'sample_adc'
CONF_SAMPLE_FREQUENCY = 'blank_frequency'
CONF_BLANK_TIME = 'blank_time'


mcpwm_sync_ns = cg.esphome_ns.namespace("mcpwm_sync")
McpwmSyncOutput = mcpwm_sync_ns.class_("McpwmSyncComponent", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(McpwmSyncOutput),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_PIN_COMP): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): cv.frequency,
        cv.Optional(CONF_DEADTIME_RISING, default="1us"): cv_time_ns,
        cv.Optional(CONF_DEADTIME_FALLING, default="1us"): cv_time_ns,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_MAX_DUTY, default=1.0): cv.float_range(min=0, max=1),
        cv.Optional(CONF_SYNC_ADC, default=None): cv.use_id(sensor.ADCSensor),
        cv.Optional(CONF_SAMPLE_FREQUENCY, default="1Hz"): cv.frequency,
        cv.Optional(CONF_BLANK_TIME, default="0s"): cv_time_ns,  # 0 to disable
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    gpio = await cg.gpio_pin_expression(config[CONF_PIN])
    sync_adc = await cg.get_variable(config[CONF_SYNC_ADC])
    var = cg.new_Pvariable(config[CONF_ID], gpio,
      config[CONF_FREQUENCY], config[CONF_DEADTIME_RISING], config[CONF_DEADTIME_FALLING],
      config[CONF_INVERTED], config[CONF_MAX_DUTY],
      sync_adc, config[CONF_SAMPLE_FREQUENCY], config[CONF_BLANK_TIME])

    if CONF_PIN_COMP in config:
        gpio_comp = await cg.gpio_pin_expression(config[CONF_PIN_COMP])
        cg.add(var.set_pin_comp(gpio_comp))

    await cg.register_component(var, config)
    await output.register_output(var, config)
