"""Number platform for Culligan Water Softener."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.components.number import NumberMode
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    UNIT_PERCENT,
)
from . import (
    CulliganWaterSoftener,
    culligan_ns,
    HardnessNumber,
    RegenTimeHourNumber,
    ReserveCapacityNumber,
    SaltLevelNumber,
    UNIT_GPG,
    UNIT_HOURS,
    UNIT_LBS,
)

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Number configuration keys
CONF_WATER_HARDNESS = "water_hardness"
CONF_REGEN_TIME_HOUR = "regeneration_time_hour"
CONF_RESERVE_CAPACITY = "reserve_capacity"
CONF_SALT_LEVEL = "salt_level"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CULLIGAN_WATER_SOFTENER_ID): cv.use_id(CulliganWaterSoftener),
        cv.Optional(CONF_WATER_HARDNESS): number.number_schema(
            HardnessNumber,
            unit_of_measurement=UNIT_GPG,
            icon="mdi:water-opacity",
        ),
        cv.Optional(CONF_REGEN_TIME_HOUR): number.number_schema(
            RegenTimeHourNumber,
            unit_of_measurement=UNIT_HOURS,
            icon="mdi:clock",
        ),
        cv.Optional(CONF_RESERVE_CAPACITY): number.number_schema(
            ReserveCapacityNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:gauge",
        ),
        cv.Optional(CONF_SALT_LEVEL): number.number_schema(
            SaltLevelNumber,
            unit_of_measurement=UNIT_LBS,
            icon="mdi:shaker",
        ),
    }
)


async def to_code(config):
    """Generate number code."""
    parent = await cg.get_variable(config[CONF_CULLIGAN_WATER_SOFTENER_ID])

    if CONF_WATER_HARDNESS in config:
        num = await number.new_number(
            config[CONF_WATER_HARDNESS],
            min_value=0,
            max_value=99,
            step=1,
        )
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_hardness_number(num))

    if CONF_REGEN_TIME_HOUR in config:
        num = await number.new_number(
            config[CONF_REGEN_TIME_HOUR],
            min_value=1,
            max_value=12,
            step=1,
        )
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_regen_time_hour_number(num))

    if CONF_RESERVE_CAPACITY in config:
        num = await number.new_number(
            config[CONF_RESERVE_CAPACITY],
            min_value=0,
            max_value=49,
            step=1,
        )
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_reserve_capacity_number(num))

    if CONF_SALT_LEVEL in config:
        num = await number.new_number(
            config[CONF_SALT_LEVEL],
            min_value=0,
            max_value=500,  # Max based on largest tank size
            step=1,
        )
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_salt_level_number(num))
