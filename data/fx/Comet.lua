local get_count = led.get_count
local fill_palette = led.fill_palette
local fade = led.fade
local time_ms = millis
local math_sin = math.sin
local math_pi = math.pi

local comet_palette = {
    {0, 255, 0, 5},   -- Head
    {255, 0, 0, 5},     -- Body
    {128, 0, 255, 5},   -- Tail
}

function update(id, axis)
    local count = get_count()
    if count == 0 then return end
    
    local speed = config.speed
    local br = config.brightness
    
    fade(0) 
    
    local t = (time_ms() * speed) / 20000
    local wave = (math_sin(t * math_pi * 2) + 1) / 2
    
    local segment_length = config.size * count / 255
    local start_pixel = math.floor(wave * (count - segment_length))
    local end_pixel = start_pixel + segment_length
    
    local size_step = 1.0 / (segment_length)
    
    -- Eye Candy Configuration
    local smoothing = 10
    local dither_intensity = 10 
    
    local cyclic = false
    fill_palette(comet_palette, 0.0, size_step, br, start_pixel, end_pixel, smoothing, dithering, cyclic)
end