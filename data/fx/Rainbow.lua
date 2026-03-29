local get_count = led.get_count
local set_hsv = led.set_hsv
local time_ms = millis
if not dither_frame then dither_frame = 0 end
function update(id, axis)
    local count = get_count()
    if count == 0 then return end
    local speed = config.speed
    local size = config.size * 10 / config.delay
    local br = config.brightness
    local h = (time_ms() * speed) / 500 
    dither_frame = (dither_frame + 1) % 65025
    local t_dither = dither_frame * (1 / 65025)
    for i = 0, count - 1 do
        local dither = (t_dither + (i * (1 / 65025))) % 1.0
        set_hsv(i, h + dither, 255, br)
        h = h + size
    end
end