# Changelog



## v0.0.6 (Speed of copper)

- Get rid of float calculations in ledtask to calculate brightness.

- Better LUA Garbage Collection.

- Force CPU Cache on LUA C++ API.

- Use bitshifting for set\_hsv() instead of costly floating point calculations.

- Better moonraker \& wifi connection handling.

- Implement user authentication module.

- Implement MQTT integration for Homeassistant.

- Implemented strip splitting in UI.

- Implemented strip preview in UI.

- Implemented Auto zone alignment (find start pixel and count automatically) in UI.

- Fix bug: When in AP mode ACE editor wont load and crash JS \[implement external assets autoloading]

- Set fields text to accent color.

- Rewrite Rainbow.lua to achieve highest framerate the i2s bus allows (200fps @146LEDS)

- Add dithering to Rainbow.lua.

- Performance optimizations to Toolhead\_Rainbow.lua

