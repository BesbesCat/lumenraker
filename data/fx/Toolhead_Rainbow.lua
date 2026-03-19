-- 1. Setup Globals
local my_id = id or 0
local my_axis = axis or 2 

_G.tracker = _G.tracker or {}
_G.tracker[my_id] = _G.tracker[my_id] or { smooth = 0.5, last = 0.0 }
local mem = _G.tracker[my_id]

local state = klipper.get_state()
local count = led.get_count()

-- 2. Pull Configs (with fallbacks)
local br = (config.brightness and config.brightness > 0) and config.brightness or 255
local speed = (config.speed and config.speed > 0) and config.speed or 128
local bed_size = (config.delay and config.delay > 0) and config.delay or 300
local wave_size = config.size or 6

-- 3. Positioning Logic (The "Follower" influence)
local raw_mm = mem.last
if state and state.pos and state.pos[my_axis] then
    raw_mm = state.pos[my_axis]
end

-- NaN/Clamp Shield
if raw_mm ~= raw_mm then raw_mm = mem.last end
mem.last = raw_mm
local target = math.max(0, math.min(1, raw_mm / bed_size))

-- Smooth the toolhead marker
mem.smooth = mem.smooth + (target - mem.smooth) * 0.15
local center = mem.smooth * (count - 1)

-- 4. Rendering Loop
for i = 0, count - 1 do
    -- A. Calculate Base Rainbow Hue
    -- We use (my_id * 40) so Zone 0 starts at a different color than Zone 1
    local hue_offset = (i * 5) + (my_id * 45)
    local time_shift = (_G.tick or 0) * (speed / 255 * 5)
    local final_hue = math.fmod(hue_offset + time_shift, 255)

    -- B. Calculate Distance to Toolhead
    local dist = math.abs(i - center)
    
    -- C. Apply "Pressure" Brightness
    -- LEDs near the toolhead get 100% brightness, others drop to 30%
    local intensity = 0.3
    if dist <= wave_size then
        intensity = 1.0 - (dist / (wave_size + 1))
        intensity = math.max(0.3, intensity) -- Never go below 30%
    end

    -- D. Output to Strip
    -- Using HSV is way easier for rainbows than calculating RGB manually
    led.set_hsv(i, 
        math.floor(final_hue), 
        255, 
        math.floor(br * intensity)
    )
end

-- Increment animation clock
_G.tick = (_G.tick or 0) + 1