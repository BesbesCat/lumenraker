if not th_trackers then th_trackers = {} end

local get_count = led.get_count
local set_hsv = led.set_hsv
local get_pos = klipper.get_pos
local time_ms = millis
local math_abs = math.abs
local math_max = math.max
local math_min = math.min
local math_floor = math.floor

-- SYNCHRONIZATION SETTINGS
local SYNC_DELAY_MS = 1000 
local RECORD_RATE_MS = 20  
local MAX_BUFFER = 60     

function update()
    local count = get_count()
    if count == 0 then return end
    
    local my_id = id or 0
    local mem = th_trackers[my_id]
    
    -- 1. ZERO-ALLOCATION PREPARATION
    if not mem then
        mem = { 
            smooth = 0,
            smooth_speed = 0,
            smooth_dir = 1.0, 
            target_dir = 1.0, 
            last_dir_sign = 1.0, -- Tracks the mathematical sign of the last movement
            stroke_dist = 0.0,   -- Accumulates distance traveled in the current direction
            locked_dist = 0.0,   -- The final distance of the previous stroke
            h_t = {}, 
            h_v = {}, 
            head = 1,
            tail = 1,
            last = 0,
            last_rec = 0
        }
        for i = 1, MAX_BUFFER do
            mem.h_t[i] = 0
            mem.h_v[i] = 0
        end
        th_trackers[my_id] = mem
    end
    
    local current_time = time_ms()
    local br = config.brightness
    local bed_size = math_max(1, config.delay) 
    
    -- 2. RATE-LIMITED RING BUFFER WRITE
    if current_time - mem.last_rec >= RECORD_RATE_MS then
        local target = math_max(0, math_min(1, get_pos(axis or 2) / bed_size))
        
        local h = mem.head
        mem.h_t[h] = current_time
        mem.h_v[h] = target
        
        h = h + 1
        if h > MAX_BUFFER then h = 1 end
        mem.head = h
        
        if h == mem.tail then
            local t = mem.tail + 1
            if t > MAX_BUFFER then t = 1 end
            mem.tail = t
        end
        
        mem.last_rec = current_time
    end
    
    -- 3. FAST RING BUFFER READ
    local delayed_target = mem.last
    local t = mem.tail
    
    while t ~= mem.head do
        if current_time - mem.h_t[t] >= SYNC_DELAY_MS then
            delayed_target = mem.h_v[t]
            mem.last = delayed_target
            
            t = t + 1
            if t > MAX_BUFFER then t = 1 end
            mem.tail = t
        else
            break 
        end
    end
    
    -- 4. SMOOTHING, INERTIA & DYNAMIC SLOSH
    local prev_smooth = mem.smooth
    mem.smooth = mem.smooth + (delayed_target - mem.smooth) * 0.015
    
    local velocity = mem.smooth - prev_smooth
    local abs_vel = math_abs(velocity)
    
    -- Track continuous stroke distance for dynamic slosh mapping
    if abs_vel > 0.0001 then
        local current_dir_sign = (velocity > 0) and 1.0 or -1.0
        
        if current_dir_sign ~= mem.last_dir_sign then
            -- Reversal detected: Lock in the stroke distance to drive the upcoming slosh
            mem.locked_dist = mem.stroke_dist
            mem.stroke_dist = 0 -- Reset accumulator for the new direction
            mem.last_dir_sign = current_dir_sign
            mem.target_dir = current_dir_sign
        else
            -- Moving continuously: Accumulate distance
            mem.stroke_dist = mem.stroke_dist + abs_vel
        end
    end
    
    -- Calculate dynamic slosh speed
    -- Base: 0.03 (Very slow slosh for tiny infill moves)
    -- Multiplier: Adds 1.5x the normalized distance traveled
    local dynamic_slosh = 0.03 + (mem.locked_dist * 1.5)
    
    -- Cap the maximum speed so massive travel moves don't instantly snap
    if dynamic_slosh > 0.25 then 
        dynamic_slosh = 0.25 
    end
    
    -- Animate the direction sliding from left to right using the dynamic inertia
    mem.smooth_dir = mem.smooth_dir + (mem.target_dir - mem.smooth_dir) * dynamic_slosh
    
    -- Calculate raw speed for tail stretching
    local raw_speed = math_min(1.0, abs_vel * 150)
    mem.smooth_speed = mem.smooth_speed + (raw_speed - mem.smooth_speed) * 0.05
    local speed_factor = mem.smooth_speed
    
    local center = mem.smooth * (count - 1)
    local wave_size = math_max(1, config.size* 2)
    
    -- 5. CALCULATE LEFT & RIGHT SIDES INDEPENDENTLY
    local radius_lead = wave_size * 0.06
    local sharp_lead  = 5.0
    
    local target_radius_tail = radius_lead + (wave_size * 0.22 * speed_factor)
    local target_sharp_tail  = sharp_lead - (2.5 * speed_factor)
    
    local right_factor = (mem.smooth_dir + 1.0) * 0.5
    local left_factor  = 1.0 - right_factor
    
    local radius_right = target_radius_tail + (radius_lead - target_radius_tail) * right_factor
    local sharp_right  = target_sharp_tail + (sharp_lead - target_sharp_tail) * right_factor
    
    local radius_left = target_radius_tail + (radius_lead - target_radius_tail) * left_factor
    local sharp_left  = target_sharp_tail + (sharp_lead - target_sharp_tail) * left_factor
    
    local hue = ((current_time * config.speed) / 1000) + (my_id * 100)
    
    -- 6. DYNAMIC RENDER LOOP
    for i = 0, count - 1 do
        local saturation = 255
        
        local offset = i - center
        local dist = math_abs(offset)
        local is_right = (offset > 0)
        
        local active_radius = is_right and radius_right or radius_left
        
        if dist <= active_radius then
            local fraction = dist / active_radius
            local active_sharpness = is_right and sharp_right or sharp_left
            
            fraction = fraction ^ active_sharpness
            local smooth_frac = fraction * fraction * (3 - 2 * fraction)
            saturation = math_floor(255 * smooth_frac)
        end
        
        set_hsv(i, hue, saturation, br)
        hue = hue + 3
    end
end