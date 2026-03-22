function update()
    local count = led.get_count()
    if count == 0 then return end

    local period = math.max(10, (255 - config.speed) * 10) 
    local phase = millis() % period
    local threshold = (config.delay / 255.0) * period
    local br = config.brightness / 255.0

    if phase < threshold then
        local r = math.floor(config.r * br)
        local g = math.floor(config.g * br)
        local b = math.floor(config.b * br)
        for i = 0, count - 1 do 
            led.set_rgb(i, r, g, b) 
        end
    else
        led.clear()
    end
end