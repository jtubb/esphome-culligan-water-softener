"""Switch platform for Culligan Water Softener."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import CulliganWaterSoftener, culligan_ns, DisplaySwitch

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Switch configuration keys
CONF_DISPLAY = "display"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CULLIGAN_WATER_SOFTENER_ID): cv.use_id(CulliganWaterSoftener),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            DisplaySwitch,
            icon="mdi:monitor",
        ),
    }
)


async def to_code(config):
    """Generate switch code."""
    parent = await cg.get_variable(config[CONF_CULLIGAN_WATER_SOFTENER_ID])

    if CONF_DISPLAY in config:
        sw = await switch.new_switch(config[CONF_DISPLAY])
        await cg.register_parented(sw, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_display_switch(sw))
