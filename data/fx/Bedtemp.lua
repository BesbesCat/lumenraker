if not bedtemp_track then bedtemp_track = {} end

function update(id, axis)
    local count = led.get_count()
    if count == 0 then return end

    local my_id = id or 0
    local t_cur = klipper.temp
    local t_tgt = math.max(1, klipper.target)
    
    bedtemp_track[my_id] = bedtemp_track[my_id] or t_cur

    local sf = math.max(0.001, (config.speed / 255.0) * 0.1)
    bedtemp_track[my_id] = bedtemp_track[my_id] + (t_cur - bedtemp_track[my_id]) * sf

    local ratio = math.max(0, math.min(1, bedtemp_track[my_id] / t_tgt))
    local br = config.brightness / 255.0

    local r = math.floor((config.r + (255 - config.r) * ratio) * br)
    local g = math.floor((config.g + (0 - config.g) * ratio) * br)
    local b = math.floor((config.b + (0 - config.b) * ratio) * br)

    for i = 0, count - 1 do
        led.set_rgb(i, r, g, b)
    end
end