local state = klipper.get_state()
local count = led.get_count()
local z_prog = state.pos[3] or 0
led.clear()

local head_pos = math.floor(z_prog * (count - 1))

for i = 0, count - 1 do
    if i < head_pos then
        led.set_rgb(i, 0, config.brightness, 0)
    elseif i == head_pos then
        led.set_rgb(i, 255, 255, 255)
    else
        led.set_rgb(i, config.r/10, config.g/10, config.b/10)
    end
end