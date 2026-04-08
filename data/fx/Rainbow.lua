local get_count = led.get_count
local fill_palette = led.fill_palette
local time_ms = millis

-- 1. Define the true HSV color wheel peaks in RGB
-- We give them all an equal weight (10) so the colors are perfectly spaced.
local rainbow_palette = {
    {255, 0, 0, 10},     -- Red
    {255, 255, 0, 10},   -- Yellow
    {0, 255, 0, 10},     -- Green
    {0, 255, 255, 10},   -- Cyan
    {0, 0, 255, 10},     -- Blue
    {255, 0, 255, 10}    -- Magenta
    -- No need to add Red at the end! is_cyclic = true handles the loop.
}

function update(id, axis)
    local count = get_count()
    if count == 0 then return end
    
    local speed = config.speed
    local br = config.brightness
    
    -- 2. Convert old Size to size_step
    -- The old fill_rainbow expected size to be 0-255.
    -- fill_palette expects a fraction (0.0 to 1.0).
    local old_size = config.size * 10 / config.delay
    local size_step = old_size / 256.0 
    
    -- 3. Convert old Hue to offset
    -- The old fill_rainbow expected Hue to be an endless number that wrapped at 255.
    -- fill_palette expects an endless float that wraps at 1.0.
    local h = (time_ms() * speed) / 500 
    local offset = (h % 256) / 256.0 
    
    -- 4. Dial in the Eye Candy
    -- Set smoothing to 0 if you want the harsh, mathematically precise HSV look.
    -- Set smoothing to 150-255 for a "pastel/eased" rainbow that looks incredible.
    local smoothing = 0 
    local dithering = 20 
    
    -- 5. Render (Ensure cyclic is TRUE so Magenta loops back to Red)
    fill_palette(rainbow_palette, offset, size_step, br, 0, count - 1, smoothing, dithering, true)
end