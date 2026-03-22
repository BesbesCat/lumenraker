if not th_trackers then th_trackers = {} end

function update()
    local count = led.get_count()
    if count == 0 then return end

    local my_id = id or 0
    local my_axis = axis or 2 

    if not th_trackers[my_id] then
        th_trackers[my_id] = { smooth = 0, hue_acc = 0 }
    end
    local mem = th_trackers[my_id]

    local br = config.brightness
    local speed = config.speed / 1000.0
    local bed_size = math.max(1, config.delay)
    local wave_size = math.max(1, config.size)

    local raw_mm = klipper.get_pos(my_axis) 
    
    local target = math.max(0, math.min(1, raw_mm / bed_size))
    mem.smooth = mem.smooth + (target - mem.smooth) * 0.15
    local center = mem.smooth * (count - 1)

    mem.hue_acc = (millis() * speed) % 255

    for i = 0, count - 1 do
        local hue_offset = i * 3
        local final_hue = (mem.hue_acc + hue_offset + (my_id * 100)) % 255

        local dist = math.abs(i - center)
        local saturation = 255
        
        if dist <= (wave_size * 0.11) then
            local factor = dist / wave_size
            if dist > 0.01 then
                saturation = math.floor(255 * factor / dist)
            else
                saturation = 0
            end
            saturation = math.min(255, math.max(0, saturation))
        end

        led.set_hsv(i, math.floor(final_hue), saturation, br)
    end
end