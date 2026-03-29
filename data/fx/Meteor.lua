function update(id, axis)
    local count = led.get_count()
    if count == 0 then return end

    local tail_fade = 100 + math.floor(config.size * 0.55)
    led.fade(tail_fade)

    local speed = config.speed / 3000
    local t = millis()
    
    local br = config.brightness / 255
    local r = math.floor(config.r * br)
    local g = math.floor(config.g * br)
    local b = math.floor(config.b * br)

    local num_meteors = 3
    local spacing = count / num_meteors

    for i = 0, num_meteors - 1 do
        local head_pos = math.floor(((t * speed) + (i * spacing)) % count)
        
        led.set_rgb(head_pos, r, g, b)
        led.set_rgb((head_pos - 1 + count) % count, r, g, b)
    end
end