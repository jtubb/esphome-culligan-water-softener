"""Sensor platform for Culligan Water Softener."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    UNIT_PERCENT,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_WATER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)
from . import (
    CulliganWaterSoftener,
    culligan_ns,
    UNIT_GPM,
    UNIT_GPG,
    UNIT_LBS,
    UNIT_GALLON,
    UNIT_MINUTES,
    UNIT_HOURS,
    UNIT_GRAINS,
)

# Icon constants
ICON_WATER = "mdi:water"
ICON_GAUGE = "mdi:gauge"
ICON_TIMER = "mdi:timer"
ICON_COG = "mdi:cog"

DEPENDENCIES = ["culligan_water_softener"]

# Parent component ID configuration key
CONF_CULLIGAN_WATER_SOFTENER_ID = "culligan_water_softener_id"

# Sensor configuration keys
CONF_CURRENT_FLOW = "current_flow"
CONF_SOFT_WATER_REMAINING = "soft_water_remaining"
CONF_WATER_USAGE_TODAY = "water_usage_today"
CONF_PEAK_FLOW_TODAY = "peak_flow_today"
CONF_WATER_HARDNESS = "water_hardness"
CONF_BRINE_LEVEL = "brine_level"
CONF_AVG_DAILY_USAGE = "avg_daily_usage"
CONF_DAYS_UNTIL_REGEN = "days_until_regen"
CONF_TOTAL_GALLONS = "total_gallons"
CONF_TOTAL_REGENS = "total_regenerations"
CONF_BATTERY_LEVEL = "battery_level"

# New sensors for Phase 3
CONF_RESERVE_CAPACITY = "reserve_capacity"
CONF_RESIN_CAPACITY = "resin_capacity"
CONF_PREFILL_DURATION = "prefill_duration"
CONF_SOAK_DURATION = "soak_duration"
CONF_BACKWASH_TIME = "backwash_time"
CONF_BRINE_DRAW_TIME = "brine_draw_time"
CONF_RAPID_RINSE_TIME = "rapid_rinse_time"
CONF_BRINE_REFILL_TIME = "brine_refill_time"

# Additional sensors from Python script
CONF_FILTER_BACKWASH_DAYS = "filter_backwash_days"
CONF_AIR_RECHARGE_DAYS = "air_recharge_days"
CONF_LOW_SALT_ALERT = "low_salt_alert"
CONF_BRINE_TANK_CAPACITY = "brine_tank_capacity"
CONF_BRINE_SALT_PERCENT = "brine_salt_percent"
CONF_REGEN_DAY_OVERRIDE = "regen_day_override"
CONF_AIR_RECHARGE_FREQUENCY = "air_recharge_frequency"
CONF_TOTAL_GALLONS_RESETTABLE = "total_gallons_resettable"
CONF_TOTAL_REGENS_RESETTABLE = "total_regenerations_resettable"
CONF_CYCLE_POSITION_5 = "cycle_position_5"
CONF_CYCLE_POSITION_6 = "cycle_position_6"
CONF_CYCLE_POSITION_7 = "cycle_position_7"
CONF_CYCLE_POSITION_8 = "cycle_position_8"
CONF_BRINE_TANK_TYPE = "brine_tank_type"
CONF_BRINE_FILL_HEIGHT = "brine_fill_height"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CULLIGAN_WATER_SOFTENER_ID): cv.use_id(CulliganWaterSoftener),
        cv.Optional(CONF_CURRENT_FLOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_GPM,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_SOFT_WATER_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_GALLON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_WATER_USAGE_TODAY): sensor.sensor_schema(
            unit_of_measurement=UNIT_GALLON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_PEAK_FLOW_TODAY): sensor.sensor_schema(
            unit_of_measurement=UNIT_GPM,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_GAUGE,
        ),
        cv.Optional(CONF_WATER_HARDNESS): sensor.sensor_schema(
            unit_of_measurement=UNIT_GPG,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BRINE_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_LBS,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:shaker",
        ),
        cv.Optional(CONF_AVG_DAILY_USAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_GALLON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_DAYS_UNTIL_REGEN): sensor.sensor_schema(
            unit_of_measurement="days",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:calendar-clock",
        ),
        cv.Optional(CONF_TOTAL_GALLONS): sensor.sensor_schema(
            unit_of_measurement=UNIT_GALLON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_TOTAL_REGENS): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon="mdi:refresh",
        ),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        # New sensors for Phase 3
        cv.Optional(CONF_RESERVE_CAPACITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_COG,
        ),
        cv.Optional(CONF_RESIN_CAPACITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_GRAINS,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_COG,
        ),
        cv.Optional(CONF_PREFILL_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOURS,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_SOAK_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOURS,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_BACKWASH_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_BRINE_DRAW_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_RAPID_RINSE_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_BRINE_REFILL_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        # Additional sensors from Python script
        cv.Optional(CONF_FILTER_BACKWASH_DAYS): sensor.sensor_schema(
            unit_of_measurement="days",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:calendar-clock",
        ),
        cv.Optional(CONF_AIR_RECHARGE_DAYS): sensor.sensor_schema(
            unit_of_measurement="days",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:calendar-clock",
        ),
        cv.Optional(CONF_LOW_SALT_ALERT): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:alert",
        ),
        cv.Optional(CONF_BRINE_TANK_CAPACITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_LBS,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:cup",
        ),
        cv.Optional(CONF_BRINE_SALT_PERCENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:percent",
        ),
        cv.Optional(CONF_REGEN_DAY_OVERRIDE): sensor.sensor_schema(
            unit_of_measurement="days",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:calendar-edit",
        ),
        cv.Optional(CONF_AIR_RECHARGE_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement="days",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:calendar-refresh",
        ),
        cv.Optional(CONF_TOTAL_GALLONS_RESETTABLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_GALLON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_TOTAL_REGENS_RESETTABLE): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon="mdi:refresh",
        ),
        cv.Optional(CONF_CYCLE_POSITION_5): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_CYCLE_POSITION_6): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_CYCLE_POSITION_7): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_CYCLE_POSITION_8): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTES,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
        ),
        cv.Optional(CONF_BRINE_TANK_TYPE): sensor.sensor_schema(
            unit_of_measurement="in",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:diameter",
        ),
        cv.Optional(CONF_BRINE_FILL_HEIGHT): sensor.sensor_schema(
            unit_of_measurement="in",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:arrow-expand-vertical",
        ),
    }
)


async def to_code(config):
    """Generate sensor code."""
    parent = await cg.get_variable(config[CONF_CULLIGAN_WATER_SOFTENER_ID])

    if CONF_CURRENT_FLOW in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_FLOW])
        cg.add(parent.set_current_flow_sensor(sens))

    if CONF_SOFT_WATER_REMAINING in config:
        sens = await sensor.new_sensor(config[CONF_SOFT_WATER_REMAINING])
        cg.add(parent.set_soft_water_remaining_sensor(sens))

    if CONF_WATER_USAGE_TODAY in config:
        sens = await sensor.new_sensor(config[CONF_WATER_USAGE_TODAY])
        cg.add(parent.set_water_usage_today_sensor(sens))

    if CONF_PEAK_FLOW_TODAY in config:
        sens = await sensor.new_sensor(config[CONF_PEAK_FLOW_TODAY])
        cg.add(parent.set_peak_flow_today_sensor(sens))

    if CONF_WATER_HARDNESS in config:
        sens = await sensor.new_sensor(config[CONF_WATER_HARDNESS])
        cg.add(parent.set_water_hardness_sensor(sens))

    if CONF_BRINE_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_LEVEL])
        cg.add(parent.set_brine_level_sensor(sens))

    if CONF_AVG_DAILY_USAGE in config:
        sens = await sensor.new_sensor(config[CONF_AVG_DAILY_USAGE])
        cg.add(parent.set_avg_daily_usage_sensor(sens))

    if CONF_DAYS_UNTIL_REGEN in config:
        sens = await sensor.new_sensor(config[CONF_DAYS_UNTIL_REGEN])
        cg.add(parent.set_days_until_regen_sensor(sens))

    if CONF_TOTAL_GALLONS in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_GALLONS])
        cg.add(parent.set_total_gallons_sensor(sens))

    if CONF_TOTAL_REGENS in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_REGENS])
        cg.add(parent.set_total_regens_sensor(sens))

    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(parent.set_battery_level_sensor(sens))

    # New sensors for Phase 3
    if CONF_RESERVE_CAPACITY in config:
        sens = await sensor.new_sensor(config[CONF_RESERVE_CAPACITY])
        cg.add(parent.set_reserve_capacity_sensor(sens))

    if CONF_RESIN_CAPACITY in config:
        sens = await sensor.new_sensor(config[CONF_RESIN_CAPACITY])
        cg.add(parent.set_resin_capacity_sensor(sens))

    if CONF_PREFILL_DURATION in config:
        sens = await sensor.new_sensor(config[CONF_PREFILL_DURATION])
        cg.add(parent.set_prefill_duration_sensor(sens))

    if CONF_SOAK_DURATION in config:
        sens = await sensor.new_sensor(config[CONF_SOAK_DURATION])
        cg.add(parent.set_soak_duration_sensor(sens))

    if CONF_BACKWASH_TIME in config:
        sens = await sensor.new_sensor(config[CONF_BACKWASH_TIME])
        cg.add(parent.set_backwash_time_sensor(sens))

    if CONF_BRINE_DRAW_TIME in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_DRAW_TIME])
        cg.add(parent.set_brine_draw_time_sensor(sens))

    if CONF_RAPID_RINSE_TIME in config:
        sens = await sensor.new_sensor(config[CONF_RAPID_RINSE_TIME])
        cg.add(parent.set_rapid_rinse_time_sensor(sens))

    if CONF_BRINE_REFILL_TIME in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_REFILL_TIME])
        cg.add(parent.set_brine_refill_time_sensor(sens))

    # Additional sensors from Python script
    if CONF_FILTER_BACKWASH_DAYS in config:
        sens = await sensor.new_sensor(config[CONF_FILTER_BACKWASH_DAYS])
        cg.add(parent.set_filter_backwash_days_sensor(sens))

    if CONF_AIR_RECHARGE_DAYS in config:
        sens = await sensor.new_sensor(config[CONF_AIR_RECHARGE_DAYS])
        cg.add(parent.set_air_recharge_days_sensor(sens))

    if CONF_LOW_SALT_ALERT in config:
        sens = await sensor.new_sensor(config[CONF_LOW_SALT_ALERT])
        cg.add(parent.set_low_salt_alert_sensor(sens))

    if CONF_BRINE_TANK_CAPACITY in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_TANK_CAPACITY])
        cg.add(parent.set_brine_tank_capacity_sensor(sens))

    if CONF_BRINE_SALT_PERCENT in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_SALT_PERCENT])
        cg.add(parent.set_brine_salt_percent_sensor(sens))

    if CONF_REGEN_DAY_OVERRIDE in config:
        sens = await sensor.new_sensor(config[CONF_REGEN_DAY_OVERRIDE])
        cg.add(parent.set_regen_day_override_sensor(sens))

    if CONF_AIR_RECHARGE_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_AIR_RECHARGE_FREQUENCY])
        cg.add(parent.set_air_recharge_frequency_sensor(sens))

    if CONF_TOTAL_GALLONS_RESETTABLE in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_GALLONS_RESETTABLE])
        cg.add(parent.set_total_gallons_resettable_sensor(sens))

    if CONF_TOTAL_REGENS_RESETTABLE in config:
        sens = await sensor.new_sensor(config[CONF_TOTAL_REGENS_RESETTABLE])
        cg.add(parent.set_total_regens_resettable_sensor(sens))

    if CONF_CYCLE_POSITION_5 in config:
        sens = await sensor.new_sensor(config[CONF_CYCLE_POSITION_5])
        cg.add(parent.set_cycle_position_5_sensor(sens))

    if CONF_CYCLE_POSITION_6 in config:
        sens = await sensor.new_sensor(config[CONF_CYCLE_POSITION_6])
        cg.add(parent.set_cycle_position_6_sensor(sens))

    if CONF_CYCLE_POSITION_7 in config:
        sens = await sensor.new_sensor(config[CONF_CYCLE_POSITION_7])
        cg.add(parent.set_cycle_position_7_sensor(sens))

    if CONF_CYCLE_POSITION_8 in config:
        sens = await sensor.new_sensor(config[CONF_CYCLE_POSITION_8])
        cg.add(parent.set_cycle_position_8_sensor(sens))

    if CONF_BRINE_TANK_TYPE in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_TANK_TYPE])
        cg.add(parent.set_brine_tank_type_sensor(sens))

    if CONF_BRINE_FILL_HEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_BRINE_FILL_HEIGHT])
        cg.add(parent.set_brine_fill_height_sensor(sens))
