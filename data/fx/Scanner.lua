function update(id, axis)
    local count = led.get_count()
    if count == 0 then return end

    local speed = config.speed / 35000.0
    local size = math.max(1, config.size / 5.0) 
    local br = config.brightness / 255.0
    
    local r, g, b = config.r, config.g, config.b
    local time = millis() * speed
    
    local pos = (time % 2 < 1) and (time % 1) * (count - 1) or (1 - (time % 1)) * (count - 1)

    for i = 0, count - 1 do
        local dist = math.abs(i - pos)
        local intensity = 0
        if dist < size then
            intensity = (math.cos((dist / size) * (math.pi / 2))) ^ 2
        end

        led.set_rgb(i, 
            math.floor(r * intensity * br), 
            math.floor(g * intensity * br), 
            math.floor(b * intensity * br)
        )
    end
end