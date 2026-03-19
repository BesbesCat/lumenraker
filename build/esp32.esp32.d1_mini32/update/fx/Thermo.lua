local state = klipper.get_state()
local count = led.get_count()
local temp = state.temp or 0
local target = state.target or 0

led.clear()

if target > 20 then
    local progress = math.min(1.0, temp / target)
    local active_leds = math.floor(progress * count)

    for i = 0, count - 1 do
        if i < active_leds then
            led.set_rgb(i, 200, 40, 0)
        elseif i == active_leds then
            local flicker = math.random(150, 255)
            led.set_rgb(i, flicker, 100, 0)
        end
    end
else
    local b = math.floor(127 + 126 * math.sin(os.clock() * config.speed))
    for i = 0, count - 1 do
        led.set_rgb(i, 0, 0, b)
    end
end