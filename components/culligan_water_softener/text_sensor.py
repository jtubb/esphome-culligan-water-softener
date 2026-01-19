"""Text sensor platform for Culligan Water Softener."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from . import CulliganWaterSoftener, culligan_ns

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Text sensor configuration keys
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_DEVICE_TIME = "device_time"
CONF_REGEN_TIME = "regeneration_time"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CULLIGAN_WATER_SOFTENER_ID): cv.use_id(CulliganWaterSoftener),
        cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
            icon="mdi:information",
        ),
        cv.Optional(CONF_DEVICE_TIME): text_sensor.text_sensor_schema(
            icon="mdi:clock",
        ),
        cv.Optional(CONF_REGEN_TIME): text_sensor.text_sensor_schema(
            icon="mdi:clock",
        ),
    }
)


async def to_code(config):
    """Generate text sensor code."""
    parent = await cg.get_variable(config[CONF_CULLIGAN_WATER_SOFTENER_ID])

    if CONF_FIRMWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FIRMWARE_VERSION])
        cg.add(parent.set_firmware_version_sensor(sens))

    if CONF_DEVICE_TIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_TIME])
        cg.add(parent.set_device_time_sensor(sens))

    if CONF_REGEN_TIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_REGEN_TIME])
        cg.add(parent.set_regen_time_sensor(sens))
