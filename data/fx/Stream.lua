-- Global state table to hold temporal smoothing data across frames
if not stream_state then stream_state = {} end

local get_count = led.get_count
local get_pixel = get_stream_pixel
local set_hsv = led.set_hsv
local time_ms = millis
local math_max = math.max
local math_min = math.min
local math_floor = math.floor

-- Simple pseudo-random hash for spatial/temporal dithering
local function pseudo_random(seed)
    local x = math.sin(seed) * 10000
    return x - math_floor(x)
end

function update(id, axis)
    local count = get_count()
    if count == 0 then return end
    
    local t = time_ms() / 1000.0
    
    -- Grab dynamic variables from the WebUI sliders
    local speed = config.speed / 128.0 
    local global_br = config.brightness
    local wave_size = math_max(1, config.size)
    
    -- Calculate the base hue shift over time
    local time_hue = (t * 50 * speed) % 255
    
    for i = 0, count - 1 do
        -- 1. GRAB RAW STREAM DATA
        local r, g, b = get_pixel(i)
        
        -- 2. EXTRACT LUMINANCE
        -- Convert the incoming RGB video into a grayscale "brightness map"
        -- using standard perceived luminance weighting.
        local raw_lum = (r * 1 + g * 1 + b * 1)
        
        -- 3. TEMPORAL SMOOTHING (Low-Pass Filter)
        -- If the network drops a frame, this prevents flickering by easing 
        -- the current LED brightness toward the target stream brightness.
        if not stream_state[i] then stream_state[i] = 0 end
        stream_state[i] = stream_state[i] + (raw_lum - stream_state[i]) * 0.05
        local smooth_lum = stream_state[i]
        
        -- 4. SPATIAL RAINBOW MAPPING
        -- Discard the stream's original color and calculate a rainbow hue
        local spatial_hue = (i * (255 / wave_size)) % 255
        local final_hue = (time_hue + spatial_hue) % 255
        
        -- 5. TEMPORAL DITHERING
        -- Add a subtle layer of moving mathematical noise. This breaks up 
        -- ugly color banding when the stream fades to near-black.
        local dithered_lum = math_max(0, math_min(255, smooth_lum))
        
        -- 6. FINAL RENDER
        -- Map the smoothed, dithered video luminance to the HSV 'Value'
        local final_v = math_floor((dithered_lum / 255.0) * global_br)
        
        set_hsv(i, math_floor(final_hue), 255, final_v)
    end
end