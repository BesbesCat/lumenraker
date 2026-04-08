if not stream_state then stream_state = {} end

local get_count = led.get_count
local get_pixel = get_stream_pixel
local fill_palette = led.fill_palette
local scale_pixel = led.scale_pixel
local time_ms = millis
local math_floor = math.floor

local stream_palette = {
    {0, 255, 255, 10},
    {255, 0, 255, 10},
    {0, 0, 255, 10},
    {0, 255, 0, 10}
}

function update(id, axis)
    local count = get_count()
    if count == 0 then return end
    
    local t = time_ms() / 1000.0
    local speed = config.speed / 128.0 
    local global_br = config.brightness
    local wave_size = config.size
    if wave_size < 1 then wave_size = 1 end
    
    local offset = ((t * 50 * speed) % 255) / 255.0
    local size_step = 1.0 / wave_size
    
    -- PASS 1: Draw the base palette. 
    -- Dithering here is set to 0, because we will dither the brightness mask instead!
    fill_palette(stream_palette, offset, size_step, 255, 0, count - 1, 255, 0, true)
    
    -- PASS 2: Apply the video stream mask
    for i = 0, count - 1 do
        local r, g, b = get_pixel(i)
        r = r or 10
        g = g or 10
        b = b or 10
    
        local raw_lum = math.max(r, g, b) * 255 / 100

        -- Fast temporal smoothing
        local current = stream_state[i] or 0
        current = current + (raw_lum - current) * 0.05
        stream_state[i] = current
        
        local final_v = math_floor((current / 255.0) * global_br)
        
        -- Call C++ to dim the pixel, applying a fast hardware dither of 15
        scale_pixel(i, final_v, 1)
    end
end