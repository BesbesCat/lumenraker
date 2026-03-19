local count = led.get_count()
local speed = (config.speed or 128) / 20.0
local tail_length = math.max(1, (config.size or 100) / 5.0)
local brightness = (config.brightness or 255) / 255.0

local r = config.r or 255
local g = config.g or 120
local b = config.b or 0

local time = os.clock() * speed * 10
local pos = (time % count)
for i = 0, count - 1 do
    local dist = pos - i
    
    if dist < 0 then dist = dist + count end

    local intensity = 0
    if dist < tail_length then
        intensity = math.exp(-dist * (4.0 / tail_length))
    end

    led.set_rgb(i,
        math.floor(r * intensity * brightness),
        math.floor(g * intensity * brightness),
        math.floor(b * intensity * brightness)
    )
end