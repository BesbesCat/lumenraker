_G.tick = (_G.tick or 0) + 1
local speed = config.speed or 128
local delay_val = config.delay or 128
local br = (config.brightness or 255) / 255.0

-- Invert speed so a higher value means a faster flash (smaller period)
local period = 255 - speed
if period < 10 then period = 10 end 

local phase = math.fmod(_G.tick, period)
local threshold = (delay_val / 255.0) * period

local count = led.get_count()

if phase < threshold then
    local r = math.floor((config.r or 255) * br)
    local g = math.floor((config.g or 0) * br)
    local b = math.floor((config.b or 0) * br)
    for i = 0, count - 1 do 
        led.set_rgb(i, r, g, b) 
    end
else
    led.clear()
end