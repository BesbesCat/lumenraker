_G.smooth_t = _G.smooth_t or 0
local state = klipper.get_state()

local t_cur = state.temp or 0
local t_tgt = state.target or 0
if t_tgt < 1
then
t_tgt = 1
end

local sf = (config.speed or 128) / 255.0

sf = sf * 0.1

if sf < 0.001
then
sf = 0.001
end

_G.smooth_t = _G.smooth_t + (t_cur - _G.smooth_t) * sf

local ratio = _G.smooth_t / t_tgt
if ratio > 1
then
ratio = 1
end

if ratio < 0
then
ratio = 0
end

local br = (config.brightness or 255) / 255

local base_r, base_g, base_b = config.r or 0, config.g or 0, config.b or 255

local r = math.floor((base_r + (255 - base_r) * ratio) * br)

local g = math.floor((base_g + (0 - base_g) * ratio) * br)

local b = math.floor((base_b + (0 - base_b) * ratio) * br)

local count = led.get_count()
for i = 0, count - 1 do
    led.set_rgb(i, r, g, b)
end