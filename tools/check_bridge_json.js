#!/usr/bin/env node
// Bridge JSON contract check (F3-2): runs invalid-arg requests against
// PeriphHub.exe --bridge (no hardware needed) and asserts stdout parses as
// valid JSON with ok === false and a non-empty error string.
//
// Usage: node tools/check_bridge_json.js [path/to/PeriphHub.exe]

const { execFileSync } = require("child_process");
const fs = require("fs");
const path = require("path");

const exe = path.resolve(process.argv[2] || path.join(__dirname, "..", "build", "PeriphHub.exe"));
if (!fs.existsSync(exe)) {
  console.error(`error: binary not found: ${exe}`);
  process.exit(2);
}

const cases = [
  { id: 1, cmd: "misc.set", args: {} },
  { id: 2, cmd: "misc.set", args: { debounce: "8", sleep: 60 } },
  { id: 3, cmd: "misc.set", args: { sleep: 9999 } },
  { id: 4, cmd: "sensor.set", args: {} },
  { id: 5, cmd: "sensor.set", args: { mode: "1", lod: 0 } },
  { id: 6, cmd: "sensor.set", args: { ripple: "true" } },
  { id: 7, cmd: "rate.set", args: { hz: "1000" } },
  { id: 8, cmd: "rate.set", args: { hz: 123 } },
  { id: 9, cmd: "dpi.set", args: { slot: 2, dpi: 800, dpiX: 800, dpiY: 1600 } },
  { id: 10, cmd: "dpi.set", args: { slot: 2, dpi: "800" } },
  { id: 11, cmd: "dpi.stage", args: { active: 6, count: 6 } },
  { id: 12, cmd: "dpi.stage", args: { active: "0", count: 6 } },
  { id: 13, cmd: "keys.set", args: { index: 0, binding: "Macro:0" } },
  { id: 14, cmd: "keys.set", args: { index: 9, binding: "KeyA" } },
  { id: 15, cmd: "macros.save", args: { slot: 99, macro: {} } },
  { id: 16, cmd: "macros.delete", args: { slot: -1 } },
  { id: 17, cmd: "hub.set", args: {} },
  { id: 18, cmd: "hub.set", args: { taskbarWidget: "true" } },
  { id: 19, cmd: "hub.set", args: { autoStart: 1 } },
  { id: 23, cmd: "hub.set", args: { widgetStacked: "yes" } },
  { id: 27, cmd: "hub.set", args: { lowBatteryNotify: "yes" } },
  { id: 24, cmd: "hub.set", args: { lowBatteryThreshold: 99 } },
  { id: 25, cmd: "hub.set", args: { pollIntervalSec: 3 } },
  { id: 26, cmd: "hub.set", args: { hiddenDevices: "incott:522C" } },
  { id: 20, cmd: "led.set", args: {} },
  { id: 21, cmd: "led.set", args: { mode: "1" } },
  { id: 22, cmd: "led.set", args: { mode: 3 } },
  { id: 28, cmd: "kbd.set", args: { deviceKey: "royuan:3151:4015" } },
  { id: 29, cmd: "kbd.set", args: { deviceKey: "royuan:3151:4015", profile: 9 } },
  { id: 30, cmd: "kbd.set", args: { deviceKey: "royuan:3151:4015", debounce: "2" } },
  { id: 31, cmd: "kbd.set", args: { deviceKey: "royuan:3151:4015", mainLed: { mode: 13 } } },
  { id: 32, cmd: "kbd.set", args: { deviceKey: "royuan:3151:4015", sleep: { sleep24g: 10 } } },
  { id: 33, cmd: "kbd.get", args: { deviceKey: "incott:522C" } },
  { id: 34, cmd: "kbd.keys.set", args: { deviceKey: "royuan:3151:4015", slot: 126, entry: [0, 0, 4, 0] } },
  { id: 35, cmd: "kbd.keys.set", args: { deviceKey: "royuan:3151:4015", slot: 90, entry: [10, 7, 0, 0] } },
  { id: 36, cmd: "kbd.keys.set", args: { deviceKey: "royuan:3151:4015", slot: 90, entry: [0, 0, 4] } },
  { id: 37, cmd: "kbd.keys.set", args: { deviceKey: "incott:522C", slot: 0, entry: [0, 0, 4, 0] } },
  { id: 38, cmd: "kbd.keys.reset", args: { deviceKey: "royuan:3151:4015" } },
  { id: 39, cmd: "kbd.macro.get", args: { deviceKey: "royuan:3151:4015", slot: 50 } },
  { id: 40, cmd: "kbd.macro.delete", args: { deviceKey: "royuan:3151:4015", slot: -1 } },
  { id: 41, cmd: "kbd.macro.set", args: { deviceKey: "royuan:3151:4015", slot: 0, repeat: 1, events: [] } },
  { id: 42, cmd: "kbd.macro.set", args: { deviceKey: "royuan:3151:4015", slot: 0, repeat: 0, events: [{ kind: "key", usage: 4, pressed: true, delayMs: 20 }, { kind: "key", usage: 4, pressed: false, delayMs: 20 }] } },
  { id: 43, cmd: "kbd.macro.set", args: { deviceKey: "royuan:3151:4015", slot: 0, repeat: 1, events: [{ kind: "key", usage: 4, pressed: true, delayMs: 20 }] } }
];

let failures = 0;

// WP8: positive offline request (`hub.get`) must exit 0 and return ok:true JSON.
{
  const req = { id: 100, cmd: "hub.get" };
  try {
    const stdout = execFileSync(exe, ["--bridge", JSON.stringify(req)], { encoding: "utf8" }).trim();
    const parsed = JSON.parse(stdout);
    if (parsed.id !== 100 || parsed.ok !== true || !parsed.data ||
        typeof parsed.data.taskbarWidget !== "boolean" ||
        typeof parsed.data.autoStart !== "boolean") {
      console.error(`FAIL id=100 (hub.get): unexpected reply: ${stdout}`);
      failures++;
    }
  } catch (err) {
    console.error(`FAIL id=100 (hub.get): expected exit 0, got status=${err.status}: ${err.message}`);
    failures++;
  }
}

// Invalid-arg requests must exit 1 (ok === false) and still output valid JSON.
for (const req of cases) {
  const raw = JSON.stringify(req);
  let stdout = "";
  let exitCode = 0;
  try {
    stdout = execFileSync(exe, ["--bridge", raw], { encoding: "utf8" }).trim();
  } catch (err) {
    exitCode = typeof err.status === "number" ? err.status : 1;
    stdout = String(err.stdout || "").trim();
  }

  if (exitCode !== 1) {
    console.error(`FAIL id=${req.id} (${req.cmd}): expected exit code 1, got ${exitCode} (stdout: ${stdout})`);
    failures++;
    continue;
  }

  let parsed;
  try {
    parsed = JSON.parse(stdout);
  } catch (err) {
    console.error(`FAIL id=${req.id} (${req.cmd}): invalid JSON stdout: ${stdout}`);
    failures++;
    continue;
  }

  if (parsed.id !== req.id || parsed.ok !== false || typeof parsed.error !== "string" || !parsed.error) {
    console.error(`FAIL id=${req.id} (${req.cmd}): unexpected contract reply: ${stdout}`);
    failures++;
  }
}

if (failures > 0) {
  console.error(`check_bridge_json: ${failures} failure(s)`);
  process.exit(1);
}
console.log(`check_bridge_json: OK (1 ok:true + ${cases.length} ok:false bridge requests verified with exit codes)`);
