local count = led.get_count()
local r = config.r or 255
local g = config.g or 0
local b = config.b or 0

local speed = (config.speed or 128) / 64
local size = (config.size or 20) / 50
local brightness = (config.brightness or 255) / 255

local time = os.clock() * speed
local pos = (time % 2 < 1) 
    and (time % 1) * (count - 1) 
    or (1 - (time % 1)) * (count - 1)

for i = 0, count - 1 do
    local dist = math.abs(i - pos)
    
    local intensity = 0
    if dist < size then
        intensity = (math.cos((dist / size) * (math.pi / 2))) ^ 2
    end

    local final_r = math.floor(r * intensity * brightness)
    local final_g = math.floor(g * intensity * brightness)
    local final_b = math.floor(b * intensity * brightness)

    led.set_rgb(i, final_r, final_g, final_b)
end