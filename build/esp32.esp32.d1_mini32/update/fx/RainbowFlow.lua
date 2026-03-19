local count = led.get_count()
local speed = (config.speed or 128) / 50.0
local size = (config.size or 128) / 10.0
local brightness = config.brightness or 255

local time = os.clock() * speed

for i = 0, count - 1 do
    local hueOffset = i * size
    local h = math.floor((time * 20 + hueOffset) % 255)
    
    led.set_hsv(i, h, 255, brightness)
end