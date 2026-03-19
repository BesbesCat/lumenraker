-- 1. Setup Globals & Persistence
local my_id = id or 0
local my_axis = axis or 2 

_G.tracker = _G.tracker or {}
_G.tracker[my_id] = _G.tracker[my_id] or { 
    smooth = 0.5, 
    last_pos = 0.0,
    hue_acc = 0.0,
    last_time = os.clock()
}
local mem = _G.tracker[my_id]

-- 2. Time Delta for Ultra-Smooth Animation
local current_time = os.clock()
local dt = current_time - mem.last_time
mem.last_time = current_time

-- 3. Pull Configs
local state = klipper.get_state()
local count = led.get_count()
local br = (config.brightness and config.brightness > 0) and config.brightness or 255
local speed = (config.speed and config.speed > 0) and config.speed or 128
local bed_size = (config.delay and config.delay > 0) and config.delay or 300
local wave_size = (config.size and config.size > 0) and config.size or 6

-- 4. Tracking Logic
local raw_mm = mem.last_pos
if state and state.pos and state.pos[my_axis] then
    raw_mm = state.pos[my_axis]
end
mem.last_pos = raw_mm

local target = math.max(0, math.min(1, raw_mm / bed_size))
mem.smooth = mem.smooth + (target - mem.smooth) * 0.1 -- Slightly slower smoothing for "liquid" feel
local center = mem.smooth * (count - 1)

-- 5. Calculate Global Hue (Rotation independent of frame rate)
-- speed/255 * 60 approx = degrees per second
mem.hue_acc = (mem.hue_acc + (dt * (speed / 255 * 100))) % 255

-- 6. Rendering Loop
for i = 0, count - 1 do
    -- A. Calculate Base Rainbow Hue (Static across the strip length)
    local hue_offset = (i * (255 / count)) -- Stretches one full rainbow across the zone
    local final_hue = (mem.hue_acc + hue_offset + (my_id * 40)) % 255

    -- B. Calculate Distance to Toolhead
    local dist = math.abs(i - center)
    
    -- C. The "White Light" Logic (Desaturation)
    -- Instead of changing Value (brightness), we change Saturation.
    -- Saturation 0 = Pure White. Saturation 255 = Pure Color.
    local saturation = 255
    if dist <= wave_size then
        -- Closer to center = lower saturation (whiter)
        local factor = dist / wave_size
        saturation = math.floor(255 * factor)
    end

    -- D. Output
    -- Value is always tied to global brightness config.
    led.set_hsv(i, 
        math.floor(final_hue), 
        saturation, 
        br
    )
end