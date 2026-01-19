"""Binary sensor platform for Culligan Water Softener."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import CulliganWaterSoftener, culligan_ns

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Binary sensor configuration keys
CONF_DISPLAY_OFF = "display_off"
CONF_BYPASS_ACTIVE = "bypass_active"
CONF_SHUTOFF_ACTIVE = "shutoff_active"
CONF_REGEN_ACTIVE = "regeneration_active"
CONF_RENTAL_REGEN_DISABLED = "rental_regen_disabled"
CONF_RENTAL_UNIT = "rental_unit"
CONF_PREFILL_ENABLED = "prefill_enabled"
CONF_PREFILL_SOAK_MODE = "prefill_soak_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CULLIGAN_WATER_SOFTENER_ID): cv.use_id(CulliganWaterSoftener),
        cv.Optional(CONF_DISPLAY_OFF): binary_sensor.binary_sensor_schema(
            icon="mdi:monitor-off",
        ),
        cv.Optional(CONF_BYPASS_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:pipe-disconnected",
        ),
        cv.Optional(CONF_SHUTOFF_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:water-off",
        ),
        cv.Optional(CONF_REGEN_ACTIVE): binary_sensor.binary_sensor_schema(
            icon="mdi:refresh",
        ),
        cv.Optional(CONF_RENTAL_REGEN_DISABLED): binary_sensor.binary_sensor_schema(
            icon="mdi:lock",
        ),
        cv.Optional(CONF_RENTAL_UNIT): binary_sensor.binary_sensor_schema(
            icon="mdi:home-group",
        ),
        cv.Optional(CONF_PREFILL_ENABLED): binary_sensor.binary_sensor_schema(
            icon="mdi:water-plus",
        ),
        cv.Optional(CONF_PREFILL_SOAK_MODE): binary_sensor.binary_sensor_schema(
            icon="mdi:timer-sand",
        ),
    }
)


async def to_code(config):
    """Generate binary sensor code."""
    parent = await cg.get_variable(config[CONF_CULLIGAN_WATER_SOFTENER_ID])

    if CONF_DISPLAY_OFF in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DISPLAY_OFF])
        cg.add(parent.set_display_off_sensor(sens))

    if CONF_BYPASS_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BYPASS_ACTIVE])
        cg.add(parent.set_bypass_active_sensor(sens))

    if CONF_SHUTOFF_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_SHUTOFF_ACTIVE])
        cg.add(parent.set_shutoff_active_sensor(sens))

    if CONF_REGEN_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_REGEN_ACTIVE])
        cg.add(parent.set_regen_active_sensor(sens))

    if CONF_RENTAL_REGEN_DISABLED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_RENTAL_REGEN_DISABLED])
        cg.add(parent.set_rental_regen_disabled_sensor(sens))

    if CONF_RENTAL_UNIT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_RENTAL_UNIT])
        cg.add(parent.set_rental_unit_sensor(sens))

    if CONF_PREFILL_ENABLED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_PREFILL_ENABLED])
        cg.add(parent.set_prefill_enabled_sensor(sens))

    if CONF_PREFILL_SOAK_MODE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_PREFILL_SOAK_MODE])
        cg.add(parent.set_prefill_soak_mode_sensor(sens))
