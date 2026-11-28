/**
 * Typed JSON bridge contracts matching docs/02-bridge-protocol.md
 * and native/src/command_json.cpp + native/src/drivers/builtin_caps.cpp.
 *
 * Nullable fields represent values that firmware did not report or cannot
 * read back; never substitute fabricated hardware defaults.
 */

export type DriverName = 'incott' | 'yjx' | 'royuan' | 'none' | (string & {});
export type DriverLevel = 'full' | 'battery';
export type DeviceKind = 'mouse' | 'keyboard' | 'headset' | 'gamepad' | 'phone' | 'mic' | 'other' | string;

export interface BridgeRequest {
  id: number;
  cmd: string;
  args?: Record<string, unknown>;
}

export interface BridgeOkResponse<T = unknown> {
  id: number;
  ok: true;
  data: T;
}

export interface BridgeErrorResponse {
  id: number;
  ok: false;
  error: string;
}

export type BridgeResponse<T = unknown> = BridgeOkResponse<T> | BridgeErrorResponse;

export type HostEvent =
  | { event: 'battery'; key: string; value: number }
  | { event: 'device'; key: string; connected: boolean }
  | { event: 'navigate'; deviceKey: string }
  | { event: 'windowState'; maximized: boolean }
  | { event: 'error'; message: string };

export interface DeviceEntry {
  key: string;
  name: string;
  kind: DeviceKind;
  via: string;
  driverLevel: DriverLevel;
  driver: DriverName;
  battery: number; // -1 = unknown
  charging: number; // 2 = charging, 0+ = discharging, -1 = unknown
  connected: boolean;
  asleep: boolean;
  mode: string; // "wireless" | "wired" | "bluetooth" | ""
  receiver: string; // "1K" | "4K" | "8K" | ""
  psd: number;
  sensor: string;
  level: string;
  detail: string;
}

export interface DpiCapability {
  min: number;
  max: number;
  step: number;
  maxStages: number;
  xySplit: boolean;
  stageCount: boolean;
}

export interface SensorCapability {
  lod: string[];
  modes: string[];
  ripple: boolean;
  fixline: boolean;
  motion: boolean;
}

export interface MiscCapability {
  debounce: { min: number; max: number };
  sleep: {
    min?: number;
    max?: number;
    step?: number;
    presetsSec: number[];
    discrete: boolean;
  };
  scrollReverse: boolean;
  fire: boolean;
}

export interface LedModeOption {
  code: number;
  label: string;
  supportsColor?: boolean;
}

export interface LightingCapability {
  mouseLed: LedModeOption[];
  receiverLed: boolean;
}

export interface KeyboardKeyDef {
  slot: number;
  code?: string;
  label: string;
  sub?: string;
  x: number;
  y: number;
  w: number;
  h: number;
  tone?: 'alpha' | 'mod' | 'accent' | 'knob';
  base?: KbdKeyEntry;
  fn?: KbdKeyEntry;
}

export interface KeyboardCapability {
  model: string;
  layout: string;
  fnSlot?: number | null;
  profiles: number[];
  keymap: boolean;
  fnLayer: boolean;
  macros?: {
    maxSlots: number;
    defaultVisibleSlots: number;
    maxStreamBytes: number;
  } | null;
  brightnessMax: number;
  speedMax: number;
  debounce: { min: number; max: number };
  sleep: {
    minSec: number;
    maxSec: number;
    shallowPresetsSec: number[];
    deepPresetsSec: number[];
  };
  options: Array<'winLock' | 'macMode' | 'wasdSwap' | 'backlightOff' | 'sidelightOff'>;
  mainModes: LedModeOption[];
  sideModes: LedModeOption[];
  keys?: KeyboardKeyDef[];
}

export interface DeviceCapabilities {
  dpi: DpiCapability | null;
  rates: number[];
  sensor: SensorCapability | null;
  misc: MiscCapability | null;
  lighting: LightingCapability | null;
  keys: boolean;
  macros: boolean;
  keyboard?: KeyboardCapability;
}

export interface DeviceInfoResponse {
  key: string;
  kind: DeviceKind;
  driver: DriverName;
  driverLevel: DriverLevel;
  capabilities: DeviceCapabilities;
}

export interface MonitorOption {
  id: string;
  label: string;
}

export interface HubSettings {
  taskbarWidget: boolean;
  autoStart: boolean;
  widgetStacked: boolean;
  widgetBackground: boolean;
  widgetMonitor: string;
  monitors: MonitorOption[];
  brandAccent: boolean;
  lowBatteryNotify: boolean;
  lowBatteryThreshold: number;
  pollIntervalSec: number;
  hiddenDevices: string[];
  deviceAliases: Record<string, string>;
}

export type HubSettingsPatch = Partial<
  Pick<
    HubSettings,
    | 'taskbarWidget'
    | 'autoStart'
    | 'widgetStacked'
    | 'widgetBackground'
    | 'widgetMonitor'
    | 'brandAccent'
    | 'lowBatteryNotify'
    | 'lowBatteryThreshold'
    | 'pollIntervalSec'
    | 'hiddenDevices'
  >
>;

export interface DeviceRenameResponse {
  key: string;
  name: string;
  alias: string;
}

export interface LedState {
  supported: boolean;
  kind?: string;
  mode: number | null;
  color?: string | null;
}

export interface DpiLevel {
  dpi: number;
  x: number | null;
  y: number;
  unverified?: string[];
}

export interface DpiState {
  count: number;
  active: number | null;
  levels: Array<DpiLevel | null>;
  axisNote?: string;
}

export interface DpiSetResponse {
  slot: number;
  dpi?: number;
  dpiX: number | null;
  dpiY: number;
  unverified?: string[];
}

export interface DpiStageResponse {
  active: number;
  count: number;
}

export interface RateState {
  code: number | null;
  hz: number | null;
}

export interface SensorState {
  lod: number | null;
  ripple: boolean | null;
  fixline: boolean | null;
  motion: boolean | null;
  mode: number | null;
}

export interface MiscState {
  debounce: number | null;
  sleep: number | null;
  fireLoops: number | null;
  fireInterval: number | null;
  scrollReverse?: boolean | null;
}

export interface KeyBindingItem {
  index: number;
  slot?: string;
  matrix: number;
  code: string | number;
  binding: string;
  kind: string;
}

export interface KeysState {
  bindings: KeyBindingItem[];
}

export interface StudioMacroItem {
  key: string;
  press: boolean;
  delay: number;
}

export interface MacroDefinition {
  name: string;
  looptype: 1 | 2 | 3; // 1=hold, 2=toggle, 3=cycles
  cycle: number; // 1..65535
  uid?: number;
  item: StudioMacroItem[];
}

export interface MacroListItem {
  slot: number;
  name: string;
  loop: number;
  cycles: number;
  actions: number;
  bind: number[];
}

export interface MacroUploadResponse {
  slot: number;
  buttonIndex: number;
  binding: string;
  warning?: string;
}

export interface KbdLedState {
  on: boolean | null;
  mode: number;
  brightness: number;
  speed: number;
  colorful: boolean;
  color: string;
  direction: number;
}

export interface KbdOptionsState {
  winLock: boolean;
  macMode: boolean;
  wasdSwap: boolean;
  backlightOff: boolean;
  sidelightOff: boolean;
}

export interface KbdSleepState {
  sleepBt: number;
  sleep24g: number;
  deepBt: number;
  deep24g: number;
}

export interface KbdState {
  devId: number;
  model: string;
  firmware: string | null;
  profile: number | null;
  options: KbdOptionsState | null;
  mainLed: KbdLedState | null;
  sideLed: KbdLedState | null;
  debounce: number | null;
  sleep: KbdSleepState | null;
}

export type KbdKeyEntry = [number, number, number, number];

export interface KbdKeysState {
  profile: number;
  base: KbdKeyEntry[];
  fn: KbdKeyEntry[];
}

export interface KbdKeySetResponse {
  profile: number;
  fnLayer: boolean;
  slot: number;
  entry: KbdKeyEntry;
  bulkUsed: boolean;
}

export interface KbdMacroSummary {
  slot: number;
  name: string;
  empty: boolean;
  repeat: number;
  eventCount: number;
  byteLength: number;
}

export interface KbdMacroListResponse {
  maxSlots: number;
  slots: KbdMacroSummary[];
}

export type KbdMacroEvent =
  | { kind: 'key'; usage: number; pressed: boolean; delayMs: number }
  | { kind: 'mouseButton'; button: number; pressed: boolean; delayMs: number }
  | { kind: 'mouseMove'; dx: number; dy: number; delayMs: number };

export interface KbdMacroDetail {
  slot: number;
  name: string;
  empty: boolean;
  repeat: number;
  byteLength: number;
  events: KbdMacroEvent[];
}
