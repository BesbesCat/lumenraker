function update()
    local count = led.get_count()
    if count == 0 then return end
    
    local z_prog = klipper.get_pos(3) 
    local head_pos = math.floor(math.max(0, math.min(1, z_prog)) * (count - 1))
    local br = config.brightness / 255.0
    
    local bg_r = math.floor((config.r / 10) * br)
    local bg_g = math.floor((config.g / 10) * br)
    local bg_b = math.floor((config.b / 10) * br)

    for i = 0, count - 1 do
        if i < head_pos then
            led.set_rgb(i, 0, math.floor(255 * br), 0)
        elseif i == head_pos then
            led.set_rgb(i, math.floor(255 * br), math.floor(255 * br), math.floor(255 * br))
        else
            led.set_rgb(i, bg_r, bg_g, bg_b)
        end
    end
end