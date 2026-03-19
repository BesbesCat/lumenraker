local count = led.get_count()
local speed = (config.speed or 128) / 40.0
local wave_width = (config.size or 128) / 20.0
local brightness = (config.brightness or 255) / 255.0

local r = config.r or 0
local g = config.g or 255
local b = config.b or 150

local time = os.clock() * speed

for i = 0, count - 1 do
    local phase_offset = i / wave_width
    local pulse = (math.sin(time + phase_offset) + 1) / 2
    
    pulse = pulse * pulse 

    led.set_rgb(i,
        math.floor(r * pulse * brightness),
        math.floor(g * pulse * brightness),
        math.floor(b * pulse * brightness)
    )
end