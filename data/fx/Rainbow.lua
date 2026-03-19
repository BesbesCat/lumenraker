local NUM_LEDS = led.get_count()

if NUM_LEDS == 0 then return end

local t = os.clock() 

local speed_factor = (config.speed or 155) / 10

local hue_density = ((config.size or 22)  / 255) * 20 + 2

for i = 0, NUM_LEDS - 1 do

     local h = math.floor((i * hue_density) + (t * speed_factor * 255)) % 256

     led.set_hsv(i, h, 255, config.brightness)

end