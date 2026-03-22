
# Lumenraker

**The Scriptable LED Controller for Klipper and Moonraker**

A hyper-optimized, Lua-driven, dual-core LED effects engine for Klipper/Moonraker 3D printers.

Lumenraker is a custom ESP32 firmware designed to bring complex, hardware-accelerated LED lighting to closed-ecosystem 3D printers (like Anycubic) via Moonraker. By offloading the heavy rendering to a dedicated ESP32 and executing effects via a lightweight Lua Virtual Machine, Lumenraker delivers buttery-smooth 60+ FPS animations with absolutely zero overhead on your printer's mainboard.

----------

## Architecture & How It Works

Lumenraker operates on an ESP32 microcontroller, acting as an independent client on your local network.

1.  **The Networking Engine:** It connects to your Wi-Fi and asynchronously polls your Moonraker API at high frequencies to retrieve printer status, temperatures, and job progress.
    
2.  **The Zone Manager:** It maps physical GPIO pins to "Strips", and divides those strips into logical "Zones". Every zone listens to all printer events. When the printer transitions to a new state (e.g., from "Idle" to "Heating"), every zone automatically executes the specific Lua script you have assigned to that event.
    
3.  **The Lua VM:** The embedded Lua Virtual Machine executes the assigned script for the current state, calculating colors based on variables like temperature or print progress.
    
4.  **The Render Pipeline:** The C++ backend retrieves the calculated colors from the Lua VM and safely pushes the data to the LEDs using hardware I2S, ensuring high-framerate animations without interrupting network tasks.
    

----------

## Features

-   **Embedded Lua Engine:** Write completely custom LED effects in Lua using the built-in Web IDE without needing to recompile C++ code.

-   **True Hardware DMA:** Built on NeoPixelBus. Pushes data to up to 10 independent LED strips using the ESP32's I2S and RMT silicon. Zero CPU bit-banging, zero disabled interrupts, and perfect Wi-Fi stability.
    
-   **Moonraker Integration:** Reacts dynamically to printer states: Idle, Start Print, Heating, Moving, Error, and Shutdown.
    
-   **Multi-Strip & Multi-Zone:** Split a single physical LED strip into multiple logical zones, or run multiple separate strips simultaneously. Direction inversion is handled automatically at the rendering level.
    
- **Dual-Core FreeRTOS Architecture:** Core 0: Dedicated strictly to Wi-Fi, the Async Web Server, and real-time Moonraker WebSocket telemetry.
Core 1: Dedicated entirely to the Lua Virtual Machine and calculating the LED frame buffer.

- **Live Lua Effects Engine:** Write, upload, and test .lua effect scripts directly through the Web UI. No need to recompile the C++ firmware to change how your printer lights up.

-   **Modern WebUI:** A responsive, dark-mode dashboard featuring an Ace-powered syntax-highlighting code editor, live system resource monitoring (RAM, LittleFS, FPS, RSSI), and a real-time Lua debug terminal.
    
-   **In-Memory Configuration Merging:** System configuration updates are merged in RAM before being written to flash, minimizing flash wear and preventing file corruption during power loss.
    
-   **OTA & Package Management:** Flash firmware updates and backup/restore configurations and scripts via `.tar` bundles directly from the browser.
    
    
----------


## Hardware Requirements

-   **Microcontroller:** ESP32 (Tested on MH-ET LIVE D1 Mini). _Note: ESP8266 is not supported due to the lack of dual cores and RMT hardware._
    
-   **LEDs:** WS2812B (NeoPixels) or compatible 3-wire addressable strips.
    
-   **Level Shifter:** A 74AHCT125 (or similar) 3.3V to 5V logic level shifter is highly recommended for signal integrity.
    
-   **Recommended GPIOs:** 2, 4, 5, 12-19, 21-23, 25-27, 32, 33. (Avoid 34-39 as they are input-only).
----------

## Installation

### 1. Flashing the ESP32

The easiest way to install Lumenraker is via the official web flasher.

1.  Connect your ESP32 to your computer via USB.
    
2.  Open a Chromium-based browser (Chrome, Edge, Brave) and navigate to the [Lumenraker Web Flasher](https://besbescat.github.io/lumenraker/).
    
3.  Click "Install", select the COM port corresponding to your ESP32, and wait for the firmware and LittleFS filesystem to be written.
    

### 2. First Boot & Network Setup

Lumenraker requires access to the same local network as your 3D printer to communicate with Moonraker.

1.  Upon initial boot (or if the configured Wi-Fi network is unavailable), the ESP32 will create its own temporary Wi-Fi Access Point.
    
2.  Using your computer or smartphone, connect to the Wi-Fi network named **LUMENRAKER_SETUP**.
    
3.  Once connected, open a web browser and navigate to `http://192.168.4.1`.
    
4.  Enter your home network's Wi-Fi SSID and Password (this must be the same network your printer uses).
    
5.  Save the credentials. The ESP32 will automatically reboot and attempt to join your home network.
    

### 3. Obtaining the Local IP Address

To access the Lumenraker WebUI, you need its new local IP address.

1.  Reconnect the ESP32 to your computer via USB (if disconnected).
    
2.  Return to the [Lumenraker Web Flasher](https://besbescat.github.io/lumenraker/) page.
    
3.  Click "Logs" or "Serial Monitor" and connect to the ESP32's COM port.
    
4.  Press the physical `RST` button on the ESP32 to reboot it while watching the terminal.
    
5.  The terminal will output the assigned local IP address once connected to your Wi-Fi (e.g., `192.168.1.120`).
    
6.  Enter this IP address into your web browser to access the Lumenraker dashboard.
    

----------

## Configuration

Navigate through the WebUI tabs to configure your system.

### System (Connection Settings)

-   **Moonraker IP:** This is the local network IP address of the host machine running Klipper and Moonraker (usually a Raspberry Pi, e.g., `192.168.1.50`).
    
-   **Port:** The port Moonraker is listening on. The default is `7125`.
    
-   **Global Brightness:** A hardware-level brightness limit to protect your power supply and LEDs from drawing too much current.
    

### LED Strips

A "Strip" represents a physical connection to a specific pin on the ESP32.

1.  Click **+ NEW STRIP**.
    
2.  **GPIO:** Enter the physical pin number your LED data line is connected to (e.g., pin 5).
    
3.  **LEDs:** Enter the total, physical count of individual LED modules on this strip.
    
**Note:** Because Lumenraker uses dynamic mapping, you are not locked into specific hardware pins. You can assign up to 10 distinct GPIO pins in the Web UI, and the engine will automatically route the hardware DMA channels to them:

**Strip 0 & 1**: Assigned to the ultra-efficient I2S hardware channels.
**Strips 2 through 9**: Assigned to the high-speed RMT hardware channels.
**Safe Pins to use:** 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33.
(Avoid pins 34-39 as they are input-only).


### Lighting Zones

A "Zone" is a logical segment of a Strip. You can assign the entirety of a strip to a single zone, or split it into multiple zones (e.g., left case, right case, toolhead) that act independently.

1.  Click **+ NEW ZONE**.
    
2.  **Strip ID:** Select which physical strip this zone belongs to (0 corresponds to the first strip created in the LED Strips tab).
    
3.  **Start:** The index of the first LED in this zone (0-indexed).
    
4.  **Length:** How many LEDs are in this zone.
    
5.  **Direction:** Select "Inverted" if the physical wiring of this zone runs backwards relative to your desired visual start point.
    
6.  **Event Assignments:** Below the zone settings, you will see a list of printer states (Idle, Heating, etc.). Every zone listens to all of these states. For each state, use the dropdown to select which Lua script should execute when the printer enters that state. You can also define custom parameters (Brightness, Color, Speed, Size) for each event, which are passed to the Lua script.
    

----------

## Lua Scripting API Reference

The power of Lumenraker lies in its exposed Lua API. Scripts have direct access to the LEDs, the WebUI parameter sliders, and the printer's current state.

### Global Variables

These variables are injected automatically into the environment every time a zone executes a script.

|Variable|Description  |
|--|--|
|`id`| The current Lighting Zone index executing the script.|
|`axis`|Returns `1` or `2` (Odd zone id returns 1, Even zone id returns 2). Useful for alternating patterns on mirrored strips.


### Global Functions

|Function  |Description  |
|--|--|
|`log(message)`|Sends a string directly to the WebUI Debug Console. Essential for troubleshooting math or logic in your scripts.|
|`millis()`|Returns the number of milliseconds since the ESP32 began running the current program.|
    
----------

### The `led` Module

Controls the physical pixels for the current executing zone. Lumenraker automatically maps your 0-based index to the correct physical position, respecting zone offsets and inverted directions.
|Function  |Description  |
|--|--|
| `led.get_count()` | Returns the total number of LEDs allocated to the current zone. |
| `led.set_rgb(i, r, g, b)` | Sets the LED at index `i` to the specified Red, Green, and Blue values (0-255). |
| `led.set_hsv(i, h, s, v)` | Sets the LED at index `i` using Hue, Saturation, and Value (0-255). |
| `led.clear()` | Instantly turns off all LEDs in the current zone. |
| `led.fade(amount)` | Fades all LEDs in the zone by the specified amount (0-255). |

----------

### The `config` Module

Pulls data directly from the sliders in the WebUI for the current event. This allows you to write a single generic script (like a breathing effect) but tweak its behavior (speed, colors, sizing) independently for every zone without duplicating code.

|Variable|Description  |
|--|--|
| `config.r`, `config.g`, `config.b` | Base RGB color values (0-255). |
| `config.speed` | Speed slider value (0-255). |
| `config.size` | Scale/Size slider value (0-255). |
| `config.delay` | Delay slider value (0-255). |
| `config.brightness` | Zone-specific brightness multiplier (0-255). |

----------

### The `klipper` Module

Reads the live data polled from Moonraker.
|Variable / Function|Description  |
|--|--|
| `klipper.event` | Current state (e.g., "Idle", "Start Print", "Heating"). |
| `klipper.temp` | Current bed temperature. |
| `klipper.target` | Target temperature. |
| `klipper.get_pos(idx)` | Returns progress or positional data tracking (idx 1-16). |
| `klipper.get_json()` | Returns the raw JSON string of the last successful Moonraker poll. |


----------

### Example Lua Script: Heating Progress Bar

The following example utilizes the API to create a dynamic progress bar based on the current heating temperatures. Note that scripts should define an `update()` function which is called every frame.

Lua

```lua
local count = led.get_count()

function update()
    -- Retrieve values from the WebUI configuration sliders
    local r, g, b = config.r, config.g, config.b

    -- Retrieve current printer state directly from klipper module
    local current_temp = klipper.temp
    local target_temp = klipper.target

    -- Prevent division by zero
    if target_temp <= 0 then target_temp = 1 end

    -- Calculate proportional illumination
    local percentage = current_temp / target_temp
    if percentage > 1 then percentage = 1 end
    local leds_to_light = math.floor(count * percentage)

    -- Render the LEDs
    led.clear()
    for i = 0, leds_to_light - 1 do
        led.set_rgb(i, r, g, b)
    end

    -- Optional: Log heating progress occasionally
    if millis() % 5000 < 20 then
        log("Heating Progress: " .. math.floor(percentage * 100) .. "%")
    end
end
```

----------

## Backup & Restore



To ensure your custom configurations and Lua scripts are safely preserved across updates or hardware migrations, Lumenraker includes a robust backup system utilizing standard `.tar` archives.

### Exporting a Backup Bundle

1.  Navigate to the **System** tab in the WebUI.
    
2.  Locate the **Export System Bundle** card.
    
3.  Select the items you wish to back up:
    
    -   **Export config.json:** Saves your Wi-Fi credentials, Moonraker connection settings, strip definitions, and zone configurations.
        
    -   **Export Lua Effects (/fx):** Iterates through your filesystem and saves all custom Lua scripts.
        
4.  Click **Generate Bundle**. The system will dynamically build a `.tar` archive and download it to your local machine.
    

### Restoring a Backup Bundle

1.  Navigate to the **System** tab.
    
2.  Locate the **Manual Installation** card.
    
3.  Select your previously exported `.tar` bundle.
    
4.  Click **Install Local Bundle**.
    

**The Restoration Process:** The JavaScript engine securely unpacks the archive in your browser. It restores the Lua files directly to the `/fx/` directory on the ESP32. To prevent file corruption or connection drops, the system intercepts the `config.json` file, performs an in-memory merge with the current controller settings, safely overwrites the values, and executes a clean reboot.

----------

## License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.

You may copy, distribute, and modify the software as long as you track changes/dates in source files. Any modifications to or software including (via compiler) GPL-licensed code must also be made available under the GPL along with build & install instructions.

For the full license text, see the [LICENSE](https://www.gnu.org/licenses/gpl-3.0.html) page.
