if not stream_state then 
    stream_state = { 
        startp = 0, 
        endp = 0,
        stable_start = -1, 
        stable_end = -1 
    } 
end

local get_count = led.get_count
local get_pixel = get_stream_pixel
local fill_palette = led.fill_palette
local time_ms = millis
local math_max = math.max
local math_abs = math.abs

local stream_palette = {
    {225, 50, 0, 5},
    {255, 180, 0, 5},
    {225, 50, 0, 5},
}

local off1 = {
    {220, 150, 0, 5},
    {255, 0, 0, 5},
}

local off2 = {
    {255, 0, 0, 5},
    {220, 150, 0, 5},
}

function update(id, axis)
    local count = get_count()
    if count == 0 then return end
    
    local t = time_ms() / 1000.0
    local speed = config.speed / 128.0 
    
    local offset = ((t * 50 * speed) % 255) / 255.0

    local target_start = -1
    local target_end = -1

    -- 1. Scan the raw stream
    for i = 0, count - 1 do
        local r, g, b = get_pixel(i)
        local raw_lum = math_max(r, g, b) * 2.55 
        
        if raw_lum > 1 then 
            if target_start == -1 then target_start = i end
            target_end = i 
        end
    end

    if target_start == -1 then
        target_start = count / 2
        target_end = count / 2
    end

    -- 2. HYSTERESIS NOISE GATE
    if stream_state.stable_start == -1 or math_abs(target_start - stream_state.stable_start) > 1.0 then
        stream_state.stable_start = target_start
    end
    
    if stream_state.stable_end == -1 or math_abs(target_end - stream_state.stable_end) > 1.0 then
        stream_state.stable_end = target_end
    end

    -- 3. APPLY SUB-PIXEL EASING to the Stable Target
    local smoothing_factor = 0.05 
    
    stream_state.startp = stream_state.startp + (stream_state.stable_start - stream_state.startp) * smoothing_factor
    stream_state.endp = stream_state.endp + (stream_state.stable_end - stream_state.endp) * smoothing_factor
    
    if math_abs(stream_state.stable_start - stream_state.startp) < 0.05 then stream_state.startp = stream_state.stable_start end
    if math_abs(stream_state.stable_end - stream_state.endp) < 0.05 then stream_state.endp = stream_state.stable_end end
    
    -- 4. Calculate mapping safely
    local diff = stream_state.endp - stream_state.startp
    local bgdiff = stream_state.startp + 5

    local size_step = 1.0 / diff
    local bg_size_step = 1.0 / bgdiff

    -- 5. Render
    fill_palette(off1, 0, bg_size_step, 100, 0, stream_state.startp, 17, 17, false, 7.0, 5)
    fill_palette(off2, 0, bg_size_step, 100, stream_state.endp, count, 17, 17, false, 7.0, 5)
    fill_palette(stream_palette, offset, size_step, 255, stream_state.startp, stream_state.endp, 7, 7, false, 7, 0)
end