"""ESPHome component for Culligan Water Softener BLE integration."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]
CODEOWNERS = ["@your-github-username"]
MULTI_CONF = True

# Component namespace
culligan_ns = cg.esphome_ns.namespace("culligan_water_softener")
CulliganWaterSoftener = culligan_ns.class_(
    "CulliganWaterSoftener", cg.Component, ble_client.BLEClientNode
)

# Button classes
RegenNowButton = culligan_ns.class_("RegenNowButton", cg.Component)
RegenNextButton = culligan_ns.class_("RegenNextButton", cg.Component)
SyncTimeButton = culligan_ns.class_("SyncTimeButton", cg.Component)
ResetGallonsButton = culligan_ns.class_("ResetGallonsButton", cg.Component)
ResetRegensButton = culligan_ns.class_("ResetRegensButton", cg.Component)

# Switch class
DisplaySwitch = culligan_ns.class_("DisplaySwitch", cg.Component)

# Number classes
HardnessNumber = culligan_ns.class_("HardnessNumber", cg.Component)
RegenTimeHourNumber = culligan_ns.class_("RegenTimeHourNumber", cg.Component)
ReserveCapacityNumber = culligan_ns.class_("ReserveCapacityNumber", cg.Component)
SaltLevelNumber = culligan_ns.class_("SaltLevelNumber", cg.Component)
RegenDaysNumber = culligan_ns.class_("RegenDaysNumber", cg.Component)
ResinCapacityNumber = culligan_ns.class_("ResinCapacityNumber", cg.Component)
PrefillDurationNumber = culligan_ns.class_("PrefillDurationNumber", cg.Component)
BackwashTimeNumber = culligan_ns.class_("BackwashTimeNumber", cg.Component)
BrineDrawTimeNumber = culligan_ns.class_("BrineDrawTimeNumber", cg.Component)
RapidRinseTimeNumber = culligan_ns.class_("RapidRinseTimeNumber", cg.Component)
BrineRefillTimeNumber = culligan_ns.class_("BrineRefillTimeNumber", cg.Component)
LowSaltAlertNumber = culligan_ns.class_("LowSaltAlertNumber", cg.Component)
BrineTankTypeNumber = culligan_ns.class_("BrineTankTypeNumber", cg.Component)
BrineFillHeightNumber = culligan_ns.class_("BrineFillHeightNumber", cg.Component)

# Custom units
UNIT_GPM = "GPM"
UNIT_GPG = "GPG"
UNIT_LBS = "lbs"
UNIT_GALLON = "gal"
UNIT_MINUTES = "min"
UNIT_HOURS = "hrs"
UNIT_GRAINS = "grains"

# Configuration keys
CONF_PASSWORD = "password"
CONF_POLL_INTERVAL = "poll_interval"

# Configuration schema
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CulliganWaterSoftener),
        cv.Optional(CONF_PASSWORD, default=1234): cv.int_range(min=0, max=9999),
        cv.Optional(CONF_POLL_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA).extend(ble_client.BLE_CLIENT_SCHEMA)


async def to_code(config):
    """Generate C++ code for the component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    # Set password
    if CONF_PASSWORD in config:
        cg.add(var.set_password(config[CONF_PASSWORD]))

    # Set poll interval
    if CONF_POLL_INTERVAL in config:
        cg.add(var.set_poll_interval(config[CONF_POLL_INTERVAL]))
