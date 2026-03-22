function update()
    local count = led.get_count()
    if count == 0 then return end

    local temp = klipper.temp
    local target = klipper.target
    local br = config.brightness / 255.0

    led.clear()

    if target > 20 then
        local progress = math.min(1.0, math.max(0.0, temp / target))
        local active_leds = math.floor(progress * count)

        for i = 0, count - 1 do
            if i < active_leds then
                led.set_rgb(i, math.floor(200 * br), math.floor(40 * br), 0)
            elseif i == active_leds then
                local flicker = math.random(150, 255)
                led.set_rgb(i, math.floor(flicker * br), math.floor(100 * br), 0)
            end
        end
    else
        local time = millis() * (config.speed / 5000.0)
        local pulse = (math.sin(time) + 1) / 2
        local blue_val = math.floor((127 + 128 * pulse) * br)
        for i = 0, count - 1 do
            led.set_rgb(i, 0, 0, blue_val)
        end
    end
end