local last_update_time = 0
local cached_total_layers = 0
local current_layer = 0

function update(id, axis)
    local count = led.get_count()
    if count == 0 then return end
    
    local current_time = millis()

    local json_str = klipper.get_json()
    if json_str and json_str ~= "" then
      current_layer = tonumber(string.match(json_str, '"current_layer"%s*:%s*(%d+)')) or current_layer
      cached_total_layers = tonumber(string.match(json_str, '"total_layer"%s*:%s*(%d+)')) or cached_total_layers
    end
    last_update_time = current_time

    local progress = 0
    if cached_total_layers > 0 and current_layer > 0 then
        progress = current_layer / cached_total_layers
    end
    
    local head_pos = math.floor(math.max(0, math.min(1, progress)) * (count - 1))
    local br = config.brightness / 255.0
    
    local bg_r = math.floor((config.r / 10) * br)
    local bg_g = math.floor((config.g / 10) * br)
    local bg_b = math.floor((config.b / 10) * br)

    for i = 0, count - 1 do
        if i < head_pos then
            led.set_rgb(i, 0, math.floor(255 * br), 0)
        elseif i == head_pos then
            led.set_rgb(i, math.floor(255 * br), math.floor(255 * br), math.floor(255 * br))
        else
            led.set_rgb(i, bg_r, bg_g, bg_b)
        end
    end
end