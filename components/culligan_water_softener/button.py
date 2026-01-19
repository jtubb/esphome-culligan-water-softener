"""Button platform for Culligan Water Softener."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID
from . import (
    CulliganWaterSoftener,
    culligan_ns,
    RegenNowButton,
    RegenNextButton,
    SyncTimeButton,
    ResetGallonsButton,
    ResetRegensButton,
)

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Button configuration keys
CONF_REGEN_NOW = "regenerate_now"
CONF_REGEN_NEXT = "regenerate_next"
CONF_SYNC_TIME = "sync_time"
CONF_RESET_GALLONS = "reset_gallons"
CONF_RESET_REGENS = "reset_regenerations"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CULLIGAN_WATER_SOFTENER_ID): cv.use_id(CulliganWaterSoftener),
        cv.Optional(CONF_REGEN_NOW): button.button_schema(
            RegenNowButton,
            icon="mdi:refresh",
        ),
        cv.Optional(CONF_REGEN_NEXT): button.button_schema(
            RegenNextButton,
            icon="mdi:refresh-auto",
        ),
        cv.Optional(CONF_SYNC_TIME): button.button_schema(
            SyncTimeButton,
            icon="mdi:clock-sync",
        ),
        cv.Optional(CONF_RESET_GALLONS): button.button_schema(
            ResetGallonsButton,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_RESET_REGENS): button.button_schema(
            ResetRegensButton,
            icon="mdi:counter",
        ),
    }
)


async def to_code(config):
    """Generate button code."""
    parent = await cg.get_variable(config[CONF_CULLIGAN_WATER_SOFTENER_ID])

    if CONF_REGEN_NOW in config:
        btn = await button.new_button(config[CONF_REGEN_NOW])
        await cg.register_parented(btn, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_regen_now_button(btn))

    if CONF_REGEN_NEXT in config:
        btn = await button.new_button(config[CONF_REGEN_NEXT])
        await cg.register_parented(btn, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_regen_next_button(btn))

    if CONF_SYNC_TIME in config:
        btn = await button.new_button(config[CONF_SYNC_TIME])
        await cg.register_parented(btn, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_sync_time_button(btn))

    if CONF_RESET_GALLONS in config:
        btn = await button.new_button(config[CONF_RESET_GALLONS])
        await cg.register_parented(btn, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_reset_gallons_button(btn))

    if CONF_RESET_REGENS in config:
        btn = await button.new_button(config[CONF_RESET_REGENS])
        await cg.register_parented(btn, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_reset_regens_button(btn))
