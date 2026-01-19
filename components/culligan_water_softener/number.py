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
    RegenDaysNumber,
    ResinCapacityNumber,
    PrefillDurationNumber,
    BackwashTimeNumber,
    BrineDrawTimeNumber,
    RapidRinseTimeNumber,
    BrineRefillTimeNumber,
    LowSaltAlertNumber,
    UNIT_GPG,
    UNIT_HOURS,
    UNIT_LBS,
    UNIT_MINUTES,
    UNIT_GRAINS,
)

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Number configuration keys
CONF_WATER_HARDNESS = "water_hardness"
CONF_REGEN_TIME_HOUR = "regeneration_time_hour"
CONF_RESERVE_CAPACITY = "reserve_capacity"
CONF_SALT_LEVEL = "salt_level"
CONF_REGEN_DAYS = "regen_days"
CONF_RESIN_CAPACITY = "resin_capacity"
CONF_PREFILL_DURATION = "prefill_duration"
CONF_BACKWASH_TIME = "backwash_time"
CONF_BRINE_DRAW_TIME = "brine_draw_time"
CONF_RAPID_RINSE_TIME = "rapid_rinse_time"
CONF_BRINE_REFILL_TIME = "brine_refill_time"
CONF_LOW_SALT_ALERT = "low_salt_alert"

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
        cv.Optional(CONF_REGEN_DAYS): number.number_schema(
            RegenDaysNumber,
            icon="mdi:calendar-clock",
        ),
        cv.Optional(CONF_RESIN_CAPACITY): number.number_schema(
            ResinCapacityNumber,
            unit_of_measurement=UNIT_GRAINS,
            icon="mdi:database",
        ),
        cv.Optional(CONF_PREFILL_DURATION): number.number_schema(
            PrefillDurationNumber,
            unit_of_measurement=UNIT_HOURS,
            icon="mdi:timer",
        ),
        cv.Optional(CONF_BACKWASH_TIME): number.number_schema(
            BackwashTimeNumber,
            unit_of_measurement=UNIT_MINUTES,
            icon="mdi:rotate-left",
        ),
        cv.Optional(CONF_BRINE_DRAW_TIME): number.number_schema(
            BrineDrawTimeNumber,
            unit_of_measurement=UNIT_MINUTES,
            icon="mdi:pump",
        ),
        cv.Optional(CONF_RAPID_RINSE_TIME): number.number_schema(
            RapidRinseTimeNumber,
            unit_of_measurement=UNIT_MINUTES,
            icon="mdi:waves",
        ),
        cv.Optional(CONF_BRINE_REFILL_TIME): number.number_schema(
            BrineRefillTimeNumber,
            unit_of_measurement=UNIT_MINUTES,
            icon="mdi:water-plus",
        ),
        cv.Optional(CONF_LOW_SALT_ALERT): number.number_schema(
            LowSaltAlertNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:alert",
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
        cg.add(num.set_accuracy_decimals(0))
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
        cg.add(num.set_accuracy_decimals(0))
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
        cg.add(num.set_accuracy_decimals(0))
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
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_salt_level_number(num))

    if CONF_REGEN_DAYS in config:
        num = await number.new_number(
            config[CONF_REGEN_DAYS],
            min_value=0,
            max_value=29,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_regen_days_number(num))

    if CONF_RESIN_CAPACITY in config:
        num = await number.new_number(
            config[CONF_RESIN_CAPACITY],
            min_value=0,
            max_value=399,  # Thousands of grains (0-399 = 0-399,000)
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_resin_capacity_number(num))

    if CONF_PREFILL_DURATION in config:
        num = await number.new_number(
            config[CONF_PREFILL_DURATION],
            min_value=0,  # 0 = disabled
            max_value=4,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_prefill_duration_number(num))

    if CONF_BACKWASH_TIME in config:
        num = await number.new_number(
            config[CONF_BACKWASH_TIME],
            min_value=0,
            max_value=99,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_backwash_time_number(num))

    if CONF_BRINE_DRAW_TIME in config:
        num = await number.new_number(
            config[CONF_BRINE_DRAW_TIME],
            min_value=0,
            max_value=99,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_brine_draw_time_number(num))

    if CONF_RAPID_RINSE_TIME in config:
        num = await number.new_number(
            config[CONF_RAPID_RINSE_TIME],
            min_value=0,
            max_value=99,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_rapid_rinse_time_number(num))

    if CONF_BRINE_REFILL_TIME in config:
        num = await number.new_number(
            config[CONF_BRINE_REFILL_TIME],
            min_value=0,
            max_value=99,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_brine_refill_time_number(num))

    if CONF_LOW_SALT_ALERT in config:
        num = await number.new_number(
            config[CONF_LOW_SALT_ALERT],
            min_value=0,
            max_value=100,
            step=1,
        )
        cg.add(num.set_accuracy_decimals(0))
        cg.add(num.traits.set_mode(NumberMode.NUMBER_MODE_BOX))
        await cg.register_parented(num, config[CONF_CULLIGAN_WATER_SOFTENER_ID])
        cg.add(parent.set_low_salt_alert_number(num))
