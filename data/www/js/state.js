let editor = null;
let warningThrottles = { ram: false, fs: false, psram: false };
let heapHistory = [];
let psramHistory = [];
const MAX_HISTORY = 180;
let debugInterval = null;
let lastDebugMsg = "";

const EVENTS = ["Idle", "Start Print", "Bed Heating", "Extruder Heating", "Moving", "Error", "Disconnected", "Stream"];
let sysConfig = { strips: [], zones: [] };
let availableScripts = ["Solid"];
let hwBoard = "esp32";
let hwFlashMB = 4;
let hwAppVersion = "0.0.0";
let remoteTarUrl = "";