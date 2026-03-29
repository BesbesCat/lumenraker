function update(id, axis)
    local count = led.get_count()
    if count == 0 then return end

    local speed = config.speed / 2000.0
    local wave_width = math.max(1, config.size / 20.0)
    local br = config.brightness / 255.0

    local r, g, b = config.r, config.g, config.b
    local time = millis() * speed

    for i = 0, count - 1 do
        local phase_offset = i / wave_width
        local pulse = (math.sin(time + phase_offset) + 1) / 2
        pulse = pulse * pulse

        led.set_rgb(i,
            math.floor(r * pulse * br),
            math.floor(g * pulse * br),
            math.floor(b * pulse * br)
        )
    end
end