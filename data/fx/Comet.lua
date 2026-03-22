function update()
    local count = led.get_count()
    if count == 0 then return end

    local tail_fade = 128 + math.floor(config.size * 0.47)
    led.fade(tail_fade)

    local speed = config.speed / 50 
    local current_time = millis()
    
    local head_pos = math.floor((current_time * speed) % count)

    local br = config.brightness / 255
    local r = math.floor(config.r * br)
    local g = math.floor(config.g * br)
    local b = math.floor(config.b * br)

    led.set_rgb(head_pos, r, g, b)
    
    local secondary_pos = (head_pos - 1 + count) % count
    led.set_rgb(secondary_pos, r, g, b)
end