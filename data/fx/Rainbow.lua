function update()
    local count = led.get_count()
    if count == 0 then return end
    
    local speed = config.speed
    local size = config.size
    local brightness = config.brightness
    
    local current_time = millis()
    local offset = (current_time * (speed / 500)) % 255
    
    local size_multiplier = size
    
    for i = 0, count - 1 do
        local h = (i * size_multiplier + offset) % 255
        led.set_hsv(i, h, 255, brightness)
    end
end