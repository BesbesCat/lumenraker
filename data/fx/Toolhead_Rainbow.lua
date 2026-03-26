if not th_trackers then th_trackers = {} end
local get_count = led.get_count
local set_hsv = led.set_hsv
local get_pos = klipper.get_pos
local time_ms = millis
local math_abs = math.abs
local math_max = math.max
local math_min = math.min
local math_floor = math.floor
function update()
    local count = get_count()
    if count == 0 then return end
    local my_id = id or 0
    local mem = th_trackers[my_id]
    if not mem then
        mem = { smooth = 0 }
        th_trackers[my_id] = mem
    end
    local br = config.brightness
    local bed_size = math_max(1, config.delay)
    local wave_size = math_max(1, config.size)
    local raw_mm = get_pos(axis or 2) 
    local target = math_max(0, math_min(1, raw_mm / bed_size))
    mem.smooth = mem.smooth + (target - mem.smooth) * 0.15
    local center = mem.smooth * (count - 1)
    local wave_radius = wave_size * 0.11
    local wave_sat = math_floor(255 / wave_size) 
    local h = ((time_ms() * config.speed) / 1000) + (my_id * 100)
    for i = 0, count - 1 do
        local saturation = 255
        local dist = math_abs(i - center)
        if dist <= wave_radius then
            if dist > 0.01 then
                saturation = wave_sat
            else
                saturation = 0
            end
        end
        set_hsv(i, h, saturation, br)
        h = h + 3
    end
end