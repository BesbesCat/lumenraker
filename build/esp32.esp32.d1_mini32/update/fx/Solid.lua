local NUM_LEDS = led.get_count()
local br = config.brightness / 255  -- Convert 0-255 to a 0.0-1.0 multiplier

-- Apply brightness scaling to the colors
local r = math.floor(config.r * br)
local g = math.floor(config.g * br)
local b = math.floor(config.b * br)

for i = 0, NUM_LEDS - 1 do
    led.set_rgb(i, r, g, b)
end