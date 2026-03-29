function update(id, axis)
    local count = led.get_count()
    local br = config.brightness / 255.0

    local r = math.floor(config.r * br)
    local g = math.floor(config.g * br)
    local b = math.floor(config.b * br)

    for i = 0, count - 1 do
        led.set_rgb(i, r, g, b)
    end
end