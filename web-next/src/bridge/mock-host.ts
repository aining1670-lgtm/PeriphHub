import type {
  BridgeRequest,
  BridgeResponse,
  DeviceCapabilities,
  DeviceEntry,
  DeviceInfoResponse,
  DpiState,
  HubSettings,
  HubSettingsPatch,
  KbdKeyEntry,
  KbdMacroDetail,
  KbdState,
  KeysState,
  LedState,
  MacroDefinition,
  MacroListItem,
  MiscState,
  RateState,
  SensorState,
} from './types';

const ORIGINAL_NAMES: Record<string, string> = {
  'incott:522C': 'Incott G23',
  'royuan:3151:4015': 'AKOS BC98',
  'bt:headset:hyperx': 'Cloud III Wireless',
  'xinput:0': 'Xbox Wireless Controller',
};

let mockDevices: DeviceEntry[] = [
  {
    key: 'incott:522C',
    name: 'Incott G23',
    kind: 'mouse',
    via: '2.4 GHz receiver',
    driverLevel: 'full',
    driver: 'incott',
    battery: 84,
    charging: 0,
    connected: true,
    asleep: false,
    mode: 'wireless',
    receiver: '8K',
    psd: 1,
    sensor: 'PAW3395',
    level: 'Full',
    detail: '8K Wireless · PAW3395',
  },
  {
    key: 'royuan:3151:4015',
    name: 'AKOS BC98',
    kind: 'keyboard',
    via: '2.4 GHz receiver',
    driverLevel: 'full',
    driver: 'royuan',
    battery: 68,
    charging: 0,
    connected: true,
    asleep: false,
    mode: 'wireless',
    receiver: '1K',
    psd: 0,
    sensor: '',
    level: 'Medium',
    detail: '98-Key Gasket · Profile 0',
  },
  {
    key: 'bt:headset:hyperx',
    name: 'Cloud III Wireless',
    kind: 'headset',
    via: 'Bluetooth',
    driverLevel: 'battery',
    driver: 'none',
    battery: 42,
    charging: 0,
    connected: true,
    asleep: false,
    mode: 'bluetooth',
    receiver: '',
    psd: -1,
    sensor: '',
    level: 'Medium',
    detail: 'Bluetooth HFP/A2DP',
  },
  {
    key: 'xinput:0',
    name: 'Xbox Wireless Controller',
    kind: 'gamepad',
    via: 'XInput',
    driverLevel: 'battery',
    driver: 'none',
    battery: 18,
    charging: 0,
    connected: true,
    asleep: false,
    mode: 'wireless',
    receiver: '',
    psd: -1,
    sensor: '',
    level: 'Low',
    detail: 'Slot 1 · AA Pack',
  },
];

let mockHubSettings: HubSettings = {
  taskbarWidget: true,
  autoStart: false,
  widgetStacked: false,
  widgetBackground: true,
  widgetMonitor: 'all',
  monitors: [
    { id: 'all', label: '全部显示器' },
    { id: 'primary', label: '主显示器 (Display 1)' },
  ],
  brandAccent: true,
  lowBatteryNotify: true,
  lowBatteryThreshold: 20,
  pollIntervalSec: 30,
  hiddenDevices: [],
  deviceAliases: {},
};

let mockWindowMaximized = false;

let mockMouseDpi: DpiState = {
  count: 4,
  active: 1,
  levels: [
    { dpi: 400, x: null, y: 400, unverified: ['x'] },
    { dpi: 800, x: null, y: 800, unverified: ['x'] },
    { dpi: 1600, x: null, y: 1600, unverified: ['x'] },
    { dpi: 3200, x: null, y: 3200, unverified: ['x'] },
    { dpi: 6400, x: null, y: 6400, unverified: ['x'] },
    { dpi: 12800, x: null, y: 12800, unverified: ['x'] },
  ],
  axisNote: '0x82 returns Y axis; X axis cannot be read back',
};

let mockMouseRate: RateState = { code: 3, hz: 1000 };
let mockMouseSensor: SensorState = {
  lod: 0, // 1.0 mm
  ripple: false,
  fixline: false,
  motion: true,
  mode: 1, // standard
};
let mockMouseMisc: MiscState = {
  debounce: 8,
  sleep: 60,
  fireLoops: 3,
  fireInterval: 10,
  scrollReverse: false,
};
let mockMouseLed: LedState = {
  supported: true,
  kind: 'receiver',
  mode: 1,
};

let mockMouseKeys: KeysState = {
  bindings: [
    { index: 0, matrix: 0, code: '0x0101', binding: 'Mouse:Left', kind: 'mouse' },
    { index: 1, matrix: 1, code: '0x0201', binding: 'Mouse:Right', kind: 'mouse' },
    { index: 2, matrix: 2, code: '0x0401', binding: 'Mouse:Middle', kind: 'mouse' },
    { index: 3, matrix: 4, code: '0x1001', binding: 'Mouse:Forward', kind: 'mouse' },
    { index: 4, matrix: 3, code: '0x0801', binding: 'Mouse:Back', kind: 'mouse' },
    { index: 5, matrix: 5, code: '0x0106', binding: 'Dpi:Cycle', kind: 'dpi' },
  ],
};

interface MockMacroRecord {
  macro: MacroDefinition;
  bind: number[];
}

const mockMouseMacroSlots: Array<MockMacroRecord | null> = Array.from({ length: 10 }, () => null);
mockMouseMacroSlots[0] = {
  macro: {
    name: '速切回弹',
    looptype: 3,
    cycle: 1,
    uid: 1001,
    item: [
      { key: 'KeyQ', press: true, delay: 25 },
      { key: 'KeyQ', press: false, delay: 40 },
      { key: 'KeyQ', press: true, delay: 25 },
      { key: 'KeyQ', press: false, delay: 0 },
    ],
  },
  bind: [],
};
mockMouseMacroSlots[1] = {
  macro: {
    name: '一键复制粘贴',
    looptype: 3,
    cycle: 1,
    uid: 1002,
    item: [
      { key: 'ControlLeft', press: true, delay: 10 },
      { key: 'KeyC', press: true, delay: 20 },
      { key: 'KeyC', press: false, delay: 20 },
      { key: 'KeyV', press: true, delay: 20 },
      { key: 'KeyV', press: false, delay: 10 },
      { key: 'ControlLeft', press: false, delay: 0 },
    ],
  },
  bind: [],
};

let mockKbdState: KbdState = {
  devId: 0x03ae,
  model: 'AKOS BC98',
  firmware: 'v1.08',
  profile: 0,
  options: {
    winLock: false,
    macMode: false,
    wasdSwap: false,
    backlightOff: false,
    sidelightOff: false,
  },
  mainLed: {
    on: true,
    mode: 4,
    brightness: 3,
    speed: 2,
    colorful: true,
    color: '#38BDF8',
    direction: 0,
  },
  sideLed: null,
  debounce: 5,
  sleep: {
    sleepBt: 120,
    sleep24g: 300,
    deepBt: 1200,
    deep24g: 1800,
  },
};

function makeEmptyKeyLayer(): KbdKeyEntry[] {
  return Array.from({ length: 128 }, () => [0, 0, 0, 0] as KbdKeyEntry);
}

const BC98_SEED_ENTRIES: Array<[number, KbdKeyEntry, KbdKeyEntry]> = [
  [0, [0, 0, 41, 0], [10, 2, 0, 0]],
  [6, [0, 0, 58, 0], [3, 0, 112, 0]],
  [12, [0, 0, 59, 0], [3, 0, 111, 0]],
  [18, [0, 0, 60, 0], [3, 0, 138, 1]],
  [24, [0, 0, 61, 0], [3, 0, 146, 1]],
  [30, [0, 0, 62, 0], [3, 0, 148, 1]],
  [36, [0, 0, 63, 0], [3, 0, 131, 1]],
  [42, [0, 0, 64, 0], [3, 0, 182, 0]],
  [48, [0, 0, 65, 0], [3, 0, 205, 0]],
  [54, [0, 0, 66, 0], [3, 0, 181, 0]],
  [60, [0, 0, 67, 0], [3, 0, 226, 0]],
  [66, [0, 0, 68, 0], [3, 0, 234, 0]],
  [72, [0, 0, 69, 0], [3, 0, 233, 0]],
  [84, [0, 0, 76, 0], [13, 1, 0, 0]],
  [90, [0, 0, 70, 0], [13, 5, 1, 0]],
  [96, [0, 0, 75, 0], [10, 12, 0, 0]],
  [102, [0, 0, 78, 0], [0, 0, 0, 0]],
  [1, [0, 0, 53, 0], [0, 0, 0, 0]],
  [7, [0, 0, 30, 0], [14, 0, 0, 0]],
  [13, [0, 0, 31, 0], [14, 0, 1, 0]],
  [19, [0, 0, 32, 0], [14, 0, 2, 0]],
  [25, [0, 0, 33, 0], [14, 0, 5, 0]],
  [31, [0, 0, 34, 0], [0, 0, 0, 0]],
  [37, [0, 0, 35, 0], [0, 0, 0, 0]],
  [43, [0, 0, 36, 0], [0, 0, 0, 0]],
  [49, [0, 0, 37, 0], [0, 0, 0, 0]],
  [55, [0, 0, 38, 0], [0, 0, 0, 0]],
  [61, [0, 0, 39, 0], [0, 0, 0, 0]],
  [67, [0, 0, 45, 0], [0, 0, 0, 0]],
  [73, [0, 0, 46, 0], [0, 0, 0, 0]],
  [79, [0, 0, 42, 0], [10, 8, 0, 0]],
  [85, [0, 0, 83, 0], [0, 0, 0, 0]],
  [91, [0, 0, 84, 0], [0, 0, 0, 0]],
  [97, [0, 0, 85, 0], [0, 0, 0, 0]],
  [103, [0, 0, 86, 0], [0, 0, 0, 0]],
  [2, [0, 0, 43, 0], [0, 0, 0, 0]],
  [8, [0, 0, 20, 0], [0, 0, 0, 0]],
  [14, [0, 0, 26, 0], [0, 0, 0, 0]],
  [20, [0, 0, 8, 0], [0, 0, 0, 0]],
  [26, [0, 0, 21, 0], [0, 0, 0, 0]],
  [32, [0, 0, 23, 0], [0, 0, 0, 0]],
  [38, [0, 0, 28, 0], [0, 0, 0, 0]],
  [44, [0, 0, 24, 0], [0, 0, 0, 0]],
  [50, [0, 0, 12, 0], [0, 0, 0, 0]],
  [56, [0, 0, 18, 0], [0, 0, 71, 0]],
  [62, [0, 0, 19, 0], [0, 0, 72, 0]],
  [68, [0, 0, 47, 0], [0, 0, 0, 0]],
  [74, [0, 0, 48, 0], [0, 0, 0, 0]],
  [80, [0, 0, 49, 0], [0, 0, 0, 0]],
  [86, [0, 0, 95, 0], [0, 0, 0, 0]],
  [92, [0, 0, 96, 0], [0, 0, 0, 0]],
  [98, [0, 0, 97, 0], [0, 0, 0, 0]],
  [104, [0, 0, 87, 0], [0, 0, 0, 0]],
  [3, [0, 0, 57, 0], [0, 0, 0, 0]],
  [9, [0, 0, 4, 0], [0, 0, 0, 0]],
  [15, [0, 0, 22, 0], [0, 0, 0, 0]],
  [21, [0, 0, 7, 0], [0, 0, 0, 0]],
  [27, [0, 0, 9, 0], [0, 0, 0, 0]],
  [33, [0, 0, 10, 0], [0, 0, 0, 0]],
  [39, [0, 0, 11, 0], [0, 0, 0, 0]],
  [45, [0, 0, 13, 0], [0, 0, 0, 0]],
  [51, [0, 0, 14, 0], [0, 0, 0, 0]],
  [57, [0, 0, 15, 0], [0, 0, 0, 0]],
  [63, [0, 0, 51, 0], [0, 0, 0, 0]],
  [69, [0, 0, 52, 0], [0, 0, 0, 0]],
  [81, [0, 0, 40, 0], [0, 0, 0, 0]],
  [87, [0, 0, 92, 0], [0, 0, 0, 0]],
  [93, [0, 0, 93, 0], [0, 0, 0, 0]],
  [99, [0, 0, 94, 0], [0, 0, 0, 0]],
  [4, [0, 0, 225, 0], [0, 0, 0, 0]],
  [16, [0, 0, 29, 0], [0, 0, 0, 0]],
  [22, [0, 0, 27, 0], [0, 0, 0, 0]],
  [28, [0, 0, 6, 0], [0, 0, 0, 0]],
  [34, [0, 0, 25, 0], [0, 0, 0, 0]],
  [40, [0, 0, 5, 0], [0, 0, 0, 0]],
  [46, [0, 0, 17, 0], [0, 0, 0, 0]],
  [52, [0, 0, 16, 0], [0, 0, 0, 0]],
  [58, [0, 0, 54, 0], [0, 0, 0, 0]],
  [64, [0, 0, 55, 0], [0, 0, 0, 0]],
  [70, [0, 0, 56, 0], [0, 0, 0, 0]],
  [76, [0, 0, 229, 0], [0, 0, 0, 0]],
  [82, [0, 0, 82, 0], [13, 2, 1, 0]],
  [88, [0, 0, 89, 0], [0, 0, 0, 0]],
  [94, [0, 0, 90, 0], [0, 0, 0, 0]],
  [100, [0, 0, 91, 0], [0, 0, 0, 0]],
  [105, [0, 0, 88, 0], [0, 0, 0, 0]],
  [5, [0, 0, 224, 0], [0, 0, 0, 0]],
  [17, [0, 0, 227, 0], [10, 3, 0, 0]],
  [23, [0, 0, 226, 0], [0, 0, 0, 0]],
  [41, [0, 0, 44, 0], [0, 0, 0, 0]],
  [59, [0, 0, 230, 0], [0, 0, 0, 0]],
  [65, [10, 1, 0, 0], [0, 0, 0, 0]],
  [71, [0, 0, 228, 0], [0, 0, 0, 0]],
  [77, [0, 0, 80, 0], [13, 3, 1, 0]],
  [83, [0, 0, 81, 0], [13, 2, 2, 0]],
  [89, [0, 0, 79, 0], [13, 3, 2, 0]],
  [95, [0, 0, 98, 0], [0, 0, 0, 0]],
  [101, [0, 0, 99, 0], [0, 0, 0, 0]],
];

function buildFactoryLayer(fnLayer: boolean): KbdKeyEntry[] {
  const arr = makeEmptyKeyLayer();
  for (const [slot, base, fn] of BC98_SEED_ENTRIES) {
    arr[slot] = (fnLayer ? fn : base).slice() as KbdKeyEntry;
  }
  return arr;
}

const mockKbdKeymaps: Record<number, { base: KbdKeyEntry[]; fn: KbdKeyEntry[] }> = {
  0: { base: buildFactoryLayer(false), fn: buildFactoryLayer(true) },
  1: { base: buildFactoryLayer(false), fn: buildFactoryLayer(true) },
  2: { base: buildFactoryLayer(false), fn: buildFactoryLayer(true) },
  3: { base: buildFactoryLayer(false), fn: buildFactoryLayer(true) },
};

const mockKbdMacros: KbdMacroDetail[] = Array.from({ length: 50 }, (_, slot) => ({
  slot,
  name: `宏 ${slot + 1}`,
  empty: true,
  repeat: 1,
  byteLength: 0,
  events: [],
}));
mockKbdMacros[0] = {
  slot: 0,
  name: '终端清屏',
  empty: false,
  repeat: 1,
  byteLength: 10,
  events: [
    { kind: 'key', usage: 224, pressed: true, delayMs: 10 },
    { kind: 'key', usage: 15, pressed: true, delayMs: 18 },
    { kind: 'key', usage: 15, pressed: false, delayMs: 10 },
    { kind: 'key', usage: 224, pressed: false, delayMs: 0 },
  ],
};

const EMPTY_CAPS: DeviceCapabilities = {
  dpi: null,
  rates: [],
  sensor: null,
  misc: null,
  lighting: null,
  keys: false,
  macros: false,
};

function buildMockCapabilities(device: DeviceEntry): DeviceCapabilities {
  if (device.driver === 'incott') {
    return {
      dpi: { min: 50, max: 32000, step: 50, maxStages: 6, xySplit: true, stageCount: true },
      rates: [125, 250, 500, 1000, 2000, 4000, 8000],
      sensor: {
        lod: ['0.7', '1.0', '2.0'],
        modes: ['eco', 'standard', 'game'],
        ripple: true,
        fixline: true,
        motion: true,
      },
      misc: {
        debounce: { min: 0, max: 30 },
        sleep: { min: 10, max: 900, step: 10, presetsSec: [60, 180, 300, 900], discrete: false },
        scrollReverse: false,
        fire: true,
      },
      lighting: { mouseLed: [], receiverLed: true },
      keys: true,
      macros: true,
    };
  }
  if (device.driver === 'royuan') {
    return {
      ...EMPTY_CAPS,
      keyboard: {
        model: 'AKOS BC98',
        layout: 'bc98',
        fnSlot: 65,
        profiles: [0, 1, 2, 3],
        keymap: true,
        fnLayer: true,
        macros: { maxSlots: 50, defaultVisibleSlots: 16, maxStreamBytes: 250 },
        brightnessMax: 4,
        speedMax: 4,
        debounce: { min: 1, max: 10 },
        sleep: {
          minSec: 60,
          maxSec: 3600,
          shallowPresetsSec: [60, 120, 300, 600, 1800],
          deepPresetsSec: [600, 1200, 1680, 1800, 3600],
        },
        options: ['winLock', 'macMode', 'wasdSwap', 'backlightOff', 'sidelightOff'],
        mainModes: [
          { code: 1, label: '常亮', supportsColor: true },
          { code: 2, label: '呼吸', supportsColor: true },
          { code: 3, label: '光谱循环', supportsColor: false },
          { code: 4, label: '波浪', supportsColor: true },
          { code: 5, label: '涟漪扩散', supportsColor: true },
          { code: 6, label: '繁星点点', supportsColor: true },
          { code: 7, label: '流光溢彩', supportsColor: true },
          { code: 8, label: '踏雪无痕', supportsColor: true },
          { code: 9, label: '层峦叠嶂', supportsColor: true },
          { code: 10, label: '正弦波', supportsColor: true },
          { code: 11, label: '弹簧涌动', supportsColor: true },
          { code: 12, label: '霓虹', supportsColor: false },
          { code: 14, label: '放射扩散', supportsColor: true },
          { code: 15, label: '环形流光', supportsColor: true },
          { code: 16, label: '多彩矩阵', supportsColor: false },
          { code: 17, label: '飘雪', supportsColor: true },
          { code: 18, label: '流星', supportsColor: true },
          { code: 19, label: '静谧落雪', supportsColor: true },
        ],
        sideModes: [],
      },
    };
  }
  return EMPTY_CAPS;
}

const RATE_HZ_LIST = [125, 250, 500, 1000, 2000, 4000, 8000];

export async function handleMockRequest(req: BridgeRequest): Promise<BridgeResponse<unknown>> {
  const { id, cmd, args = {} } = req;

  switch (cmd) {
    case 'window.state':
    case 'window.minimize':
      return { id, ok: true, data: { maximized: mockWindowMaximized } };
    case 'window.maximize':
      mockWindowMaximized = !mockWindowMaximized;
      return { id, ok: true, data: { maximized: mockWindowMaximized } };
    case 'window.close':
    case 'window.drag':
    case 'window.resize':
    case 'window.sysmenu':
    case 'window.theme':
      return { id, ok: true, data: {} };

    case 'devices.get':
    case 'device.probe':
      return { id, ok: true, data: mockDevices.map((d) => ({ ...d })) };

    case 'device.info': {
      const requestedKey = typeof args.deviceKey === 'string' ? args.deviceKey : '';
      const matched =
        mockDevices.find((d) => d.key === requestedKey) ??
        mockDevices.find((d) => d.driverLevel === 'full') ??
        mockDevices[0];
      const info: DeviceInfoResponse = {
        key: matched?.key ?? requestedKey,
        kind: matched?.kind ?? 'other',
        driver: matched?.driver ?? 'none',
        driverLevel: matched?.driverLevel ?? 'battery',
        capabilities: matched ? buildMockCapabilities(matched) : EMPTY_CAPS,
      };
      return { id, ok: true, data: info };
    }

    case 'device.rename': {
      const deviceKey = typeof args.deviceKey === 'string' ? args.deviceKey : '';
      const alias = typeof args.alias === 'string' ? args.alias.trim() : '';
      const origName = ORIGINAL_NAMES[deviceKey] ?? mockDevices.find((d) => d.key === deviceKey)?.name ?? alias;
      const resolvedName = alias || origName;
      mockDevices = mockDevices.map((d) =>
        d.key === deviceKey ? { ...d, name: resolvedName } : d,
      );
      if (alias) {
        mockHubSettings = {
          ...mockHubSettings,
          deviceAliases: { ...mockHubSettings.deviceAliases, [deviceKey]: alias },
        };
      } else {
        const nextAliases = { ...mockHubSettings.deviceAliases };
        delete nextAliases[deviceKey];
        mockHubSettings = { ...mockHubSettings, deviceAliases: nextAliases };
      }
      return {
        id,
        ok: true,
        data: { key: deviceKey, name: resolvedName, alias },
      };
    }

    case 'hub.get':
      return { id, ok: true, data: { ...mockHubSettings } };

    case 'hub.set': {
      const patch = args as HubSettingsPatch;
      mockHubSettings = { ...mockHubSettings, ...patch };
      return { id, ok: true, data: { ...mockHubSettings } };
    }

    case 'hub.testNotify':
      return { id, ok: true, data: { sent: true } };

    // Mouse commands
    case 'dpi.get':
      return {
        id,
        ok: true,
        data: {
          ...mockMouseDpi,
          levels: mockMouseDpi.levels.map((l) => (l ? { ...l } : null)),
        },
      };

    case 'dpi.set': {
      const slot = Number(args.slot);
      if (!Number.isInteger(slot) || slot < 0 || slot >= mockMouseDpi.count) {
        return { id, ok: false, error: `slot must be 0..${mockMouseDpi.count - 1}` };
      }
      if (typeof args.dpi === 'number') {
        const dpi = args.dpi;
        if (!Number.isInteger(dpi) || dpi < 50 || dpi > 32000 || dpi % 50 !== 0) {
          return { id, ok: false, error: 'dpi must be 50..32000 in steps of 50' };
        }
        mockMouseDpi.levels[slot] = { dpi, x: dpi, y: dpi };
        return { id, ok: true, data: { slot, dpi, dpiX: dpi, dpiY: dpi } };
      }
      const dpiX = Number(args.dpiX);
      const dpiY = Number(args.dpiY);
      if (
        !Number.isInteger(dpiX) ||
        !Number.isInteger(dpiY) ||
        dpiX < 50 ||
        dpiX > 32000 ||
        dpiY < 50 ||
        dpiY > 32000 ||
        dpiX % 50 !== 0 ||
        dpiY % 50 !== 0
      ) {
        return { id, ok: false, error: 'dpiX/dpiY must be 50..32000 in steps of 50' };
      }
      mockMouseDpi.levels[slot] = { dpi: dpiY, x: null, y: dpiY, unverified: ['x'] };
      return { id, ok: true, data: { slot, dpiX: null, dpiY, unverified: ['dpiX'] } };
    }

    case 'dpi.stage': {
      const active = Number(args.active);
      const count = Number(args.count);
      if (!Number.isInteger(count) || count < 1 || count > 6) {
        return { id, ok: false, error: 'count must be 1..6' };
      }
      if (!Number.isInteger(active) || active < 0 || active >= count) {
        return { id, ok: false, error: 'active must be 0..count-1' };
      }
      mockMouseDpi.count = count;
      mockMouseDpi.active = active;
      return { id, ok: true, data: { active, count } };
    }

    case 'rate.get':
      return { id, ok: true, data: { ...mockMouseRate } };

    case 'rate.set': {
      let hz = typeof args.hz === 'number' ? args.hz : -1;
      if (hz < 0 && typeof args.code === 'number') {
        hz = RATE_HZ_LIST[args.code] ?? -1;
      }
      const code = RATE_HZ_LIST.indexOf(hz);
      if (code < 0) return { id, ok: false, error: 'invalid polling rate' };
      mockMouseRate = { code, hz };
      return { id, ok: true, data: { ...mockMouseRate } };
    }

    case 'sensor.get':
      return { id, ok: true, data: { ...mockMouseSensor } };

    case 'sensor.set': {
      const reply: Partial<SensorState> = {};
      if (typeof args.lod === 'number') {
        if (!Number.isInteger(args.lod) || args.lod < 0 || args.lod > 2) {
          return { id, ok: false, error: 'lod must be 0..2' };
        }
        mockMouseSensor.lod = args.lod;
        reply.lod = args.lod;
      }
      if (typeof args.mode === 'number') {
        if (!Number.isInteger(args.mode) || args.mode < 0 || args.mode > 2) {
          return { id, ok: false, error: 'mode must be 0..2' };
        }
        mockMouseSensor.mode = args.mode;
        reply.mode = args.mode;
      }
      if (typeof args.ripple === 'boolean') {
        mockMouseSensor.ripple = args.ripple;
        reply.ripple = args.ripple;
      }
      if (typeof args.fixline === 'boolean') {
        mockMouseSensor.fixline = args.fixline;
        reply.fixline = args.fixline;
      }
      if (typeof args.motion === 'boolean') {
        mockMouseSensor.motion = args.motion;
        reply.motion = args.motion;
      }
      return { id, ok: true, data: reply };
    }

    case 'misc.get':
      return { id, ok: true, data: { ...mockMouseMisc } };

    case 'misc.set': {
      const reply: Partial<MiscState> = {};
      if (typeof args.debounce === 'number') {
        if (!Number.isInteger(args.debounce) || args.debounce < 0 || args.debounce > 30) {
          return { id, ok: false, error: 'debounce must be 0..30' };
        }
        mockMouseMisc.debounce = args.debounce;
        reply.debounce = args.debounce;
      }
      if (typeof args.sleep === 'number') {
        if (!Number.isInteger(args.sleep) || args.sleep < 10 || args.sleep > 900) {
          return { id, ok: false, error: 'sleep must be 10..900' };
        }
        mockMouseMisc.sleep = args.sleep;
        reply.sleep = args.sleep;
      }
      if (typeof args.fireLoops === 'number') {
        if (!Number.isInteger(args.fireLoops) || args.fireLoops < 0 || args.fireLoops > 3) {
          return { id, ok: false, error: 'fireLoops must be 0..3' };
        }
        mockMouseMisc.fireLoops = args.fireLoops;
        reply.fireLoops = args.fireLoops;
      }
      if (typeof args.fireInterval === 'number') {
        if (!Number.isInteger(args.fireInterval) || args.fireInterval < 0 || args.fireInterval > 255) {
          return { id, ok: false, error: 'fireInterval must be 0..255' };
        }
        mockMouseMisc.fireInterval = args.fireInterval;
        reply.fireInterval = args.fireInterval;
      }
      if (typeof args.scrollReverse === 'boolean') {
        mockMouseMisc.scrollReverse = args.scrollReverse;
        reply.scrollReverse = args.scrollReverse;
      }
      return { id, ok: true, data: reply };
    }

    case 'led.get':
      return { id, ok: true, data: { ...mockMouseLed } };

    case 'led.set': {
      const mode = Number(args.mode);
      if (!Number.isInteger(mode) || mode < 0 || mode > 2) {
        return { id, ok: false, error: 'mode must be 0..2' };
      }
      const color = typeof args.color === 'string' ? args.color : mockMouseLed.color;
      mockMouseLed = { ...mockMouseLed, mode, ...(color !== undefined ? { color } : {}) };
      return { id, ok: true, data: { ...mockMouseLed } };
    }

    case 'keys.get':
      return {
        id,
        ok: true,
        data: { bindings: mockMouseKeys.bindings.map((b) => ({ ...b })) },
      };

    case 'keys.set': {
      const index = Number(args.index);
      const binding = typeof args.binding === 'string' ? args.binding : '';
      if (!Number.isInteger(index) || index < 0 || index > 5) {
        return { id, ok: false, error: 'index must be 0..5' };
      }
      if (/^macro:/i.test(binding)) {
        return { id, ok: false, error: 'Macro:N is set via macros.upload, not keys.set' };
      }
      mockMouseKeys.bindings = mockMouseKeys.bindings.map((b) =>
        b.index === index ? { ...b, binding } : b,
      );
      for (const rec of mockMouseMacroSlots) {
        if (rec) rec.bind = rec.bind.filter((bi) => bi !== index);
      }
      return {
        id,
        ok: true,
        data: { bindings: mockMouseKeys.bindings.map((b) => ({ ...b })) },
      };
    }

    case 'macros.list': {
      const list: MacroListItem[] = [];
      mockMouseMacroSlots.forEach((rec, slot) => {
        if (!rec) return;
        list.push({
          slot,
          name: rec.macro.name,
          loop: rec.macro.looptype,
          cycles: rec.macro.cycle,
          actions: rec.macro.item.length,
          bind: [...rec.bind],
        });
      });
      return { id, ok: true, data: list };
    }

    case 'macros.save': {
      const slot = Number(args.slot);
      const macro = args.macro as MacroDefinition | undefined;
      if (!Number.isInteger(slot) || slot < 0 || slot >= 10 || !macro) {
        return { id, ok: false, error: 'invalid slot or macro' };
      }
      if (macro.item.length < 1 || macro.item.length > 71) {
        return { id, ok: false, error: 'macro items must be 1..71' };
      }
      const prevBind = mockMouseMacroSlots[slot]?.bind ?? [];
      mockMouseMacroSlots[slot] = {
        macro: { ...macro, item: macro.item.map((x) => ({ ...x })) },
        bind: prevBind,
      };
      return { id, ok: true, data: { slot } };
    }

    case 'macros.upload': {
      const slot = Number(args.slot);
      const buttonIndex = Number(args.buttonIndex);
      if (!Number.isInteger(slot) || slot < 0 || slot >= 10 || !mockMouseMacroSlots[slot]) {
        return { id, ok: false, error: `slot ${slot} is empty` };
      }
      if (!Number.isInteger(buttonIndex) || buttonIndex < 0 || buttonIndex > 5) {
        return { id, ok: false, error: 'buttonIndex must be 0..5' };
      }
      for (const rec of mockMouseMacroSlots) {
        if (rec) rec.bind = rec.bind.filter((bi) => bi !== buttonIndex);
      }
      mockMouseMacroSlots[slot]!.bind.push(buttonIndex);
      const binding = `Macro:${buttonIndex}`;
      mockMouseKeys.bindings = mockMouseKeys.bindings.map((b) =>
        b.index === buttonIndex ? { ...b, binding, kind: 'macro' } : b,
      );
      return { id, ok: true, data: { slot, buttonIndex, binding } };
    }

    case 'macro.export': {
      const slot = Number(args.slot);
      const rec = Number.isInteger(slot) ? mockMouseMacroSlots[slot] : null;
      if (!rec) return { id, ok: false, error: 'slot is empty' };
      const json = JSON.stringify(
        { format: 'incott-studio-macro', version: 1, macro: rec.macro },
        null,
        2,
      );
      return { id, ok: true, data: { json } };
    }

    case 'macro.import': {
      const text = typeof args.json === 'string' ? args.json : '';
      let parsed: Record<string, unknown>;
      try {
        parsed = JSON.parse(text) as Record<string, unknown>;
      } catch {
        return { id, ok: false, error: 'invalid JSON' };
      }
      const macroObj = (parsed.macro && typeof parsed.macro === 'object'
        ? parsed.macro
        : parsed) as MacroDefinition;
      if (!macroObj || !Array.isArray(macroObj.item) || macroObj.item.length === 0) {
        return { id, ok: false, error: 'macro needs an item array' };
      }
      let targetSlot = typeof args.slot === 'number' ? args.slot : -1;
      if (targetSlot < 0) {
        targetSlot = mockMouseMacroSlots.findIndex((s) => s === null);
      }
      if (targetSlot < 0 || targetSlot >= 10) {
        return { id, ok: false, error: 'library is full (10 macros)' };
      }
      mockMouseMacroSlots[targetSlot] = { macro: macroObj, bind: [] };
      return { id, ok: true, data: { slot: targetSlot, summary: macroObj.name } };
    }

    case 'macros.delete': {
      const slot = Number(args.slot);
      if (!Number.isInteger(slot) || slot < 0 || slot >= 10 || !mockMouseMacroSlots[slot]) {
        return { id, ok: false, error: `slot ${slot} is empty` };
      }
      mockMouseMacroSlots[slot] = null;
      return { id, ok: true, data: { slot } };
    }

    case 'macros.rename': {
      const slot = Number(args.slot);
      const name = typeof args.name === 'string' ? args.name.trim() : '';
      if (!Number.isInteger(slot) || slot < 0 || slot >= 10 || !mockMouseMacroSlots[slot]) {
        return { id, ok: false, error: `slot ${slot} is empty` };
      }
      if (!name || name.length > 32) {
        return { id, ok: false, error: 'invalid name' };
      }
      mockMouseMacroSlots[slot]!.macro.name = name;
      return { id, ok: true, data: { slot, name } };
    }

    // ROYUAN / AKOS BC98 Keyboard commands
    case 'kbd.get':
      return {
        id,
        ok: true,
        data: {
          ...mockKbdState,
          options: mockKbdState.options ? { ...mockKbdState.options } : null,
          mainLed: mockKbdState.mainLed ? { ...mockKbdState.mainLed } : null,
          sleep: mockKbdState.sleep ? { ...mockKbdState.sleep } : null,
        },
      };

    case 'kbd.set': {
      if (typeof args.profile === 'number') mockKbdState.profile = args.profile;
      if (typeof args.debounce === 'number') mockKbdState.debounce = args.debounce;
      const optSrc =
        args.options && typeof args.options === 'object'
          ? (args.options as Record<string, unknown>)
          : args;
      if (mockKbdState.options) {
        if (typeof optSrc.winLock === 'boolean') mockKbdState.options.winLock = optSrc.winLock;
        if (typeof optSrc.macMode === 'boolean') mockKbdState.options.macMode = optSrc.macMode;
        if (typeof optSrc.wasdSwap === 'boolean') mockKbdState.options.wasdSwap = optSrc.wasdSwap;
        if (typeof optSrc.backlightOff === 'boolean') {
          mockKbdState.options.backlightOff = optSrc.backlightOff;
          if (mockKbdState.mainLed) mockKbdState.mainLed.on = !optSrc.backlightOff;
        }
      }
      if (args.mainLed && typeof args.mainLed === 'object' && mockKbdState.mainLed) {
        const ml = args.mainLed as Record<string, unknown>;
        if (typeof ml.on === 'boolean') {
          mockKbdState.mainLed.on = ml.on;
          if (mockKbdState.options) mockKbdState.options.backlightOff = !ml.on;
        }
        if (typeof ml.mode === 'number') mockKbdState.mainLed.mode = ml.mode;
        if (typeof ml.brightness === 'number') mockKbdState.mainLed.brightness = ml.brightness;
        if (typeof ml.speed === 'number') mockKbdState.mainLed.speed = ml.speed;
        if (typeof ml.colorful === 'boolean') mockKbdState.mainLed.colorful = ml.colorful;
        if (typeof ml.color === 'string') mockKbdState.mainLed.color = ml.color;
      }
      if (args.sleep && typeof args.sleep === 'object' && mockKbdState.sleep) {
        const sl = args.sleep as Record<string, unknown>;
        if (typeof sl.sleepBt === 'number') mockKbdState.sleep.sleepBt = sl.sleepBt;
        if (typeof sl.sleep24g === 'number') mockKbdState.sleep.sleep24g = sl.sleep24g;
        if (typeof sl.deepBt === 'number') mockKbdState.sleep.deepBt = sl.deepBt;
        if (typeof sl.deep24g === 'number') mockKbdState.sleep.deep24g = sl.deep24g;
      }
      return { id, ok: true, data: { ...mockKbdState } };
    }

    case 'kbd.keys.get': {
      const profile =
        typeof args.profile === 'number' ? args.profile : mockKbdState.profile ?? 0;
      const km = mockKbdKeymaps[profile] ?? mockKbdKeymaps[0];
      return {
        id,
        ok: true,
        data: {
          profile,
          base: km.base.map((e) => [...e]),
          fn: km.fn.map((e) => [...e]),
        },
      };
    }

    case 'kbd.keys.set': {
      const profile =
        typeof args.profile === 'number' ? args.profile : mockKbdState.profile ?? 0;
      const fnLayer = Boolean(args.fnLayer);
      const slot = Number(args.slot);
      const entry = args.entry as KbdKeyEntry;
      if (!Number.isInteger(slot) || slot < 0 || slot > 125 || !Array.isArray(entry)) {
        return { id, ok: false, error: 'invalid slot or entry' };
      }
      if (fnLayer && slot === 65) {
        return { id, ok: false, error: 'cannot remap Fn on Fn layer' };
      }
      const km = mockKbdKeymaps[profile] ?? mockKbdKeymaps[0];
      const layer = fnLayer ? km.fn : km.base;
      layer[slot] = [...entry] as KbdKeyEntry;
      return {
        id,
        ok: true,
        data: { profile, fnLayer, slot, entry: [...entry], bulkUsed: false },
      };
    }

    case 'kbd.keys.reset': {
      const profile =
        typeof args.profile === 'number' ? args.profile : mockKbdState.profile ?? 0;
      const fnLayer = Boolean(args.fnLayer);
      const km = mockKbdKeymaps[profile] ?? mockKbdKeymaps[0];
      if (fnLayer) km.fn = buildFactoryLayer(true);
      else km.base = buildFactoryLayer(false);
      return {
        id,
        ok: true,
        data: {
          profile,
          base: km.base.map((e) => [...e]),
          fn: km.fn.map((e) => [...e]),
        },
      };
    }

    case 'kbd.macro.list': {
      const count = typeof args.count === 'number' ? Math.min(50, Math.max(1, args.count)) : 16;
      return {
        id,
        ok: true,
        data: {
          maxSlots: 50,
          slots: mockKbdMacros.slice(0, count).map((m) => ({
            slot: m.slot,
            name: m.name,
            empty: m.empty,
            repeat: m.repeat,
            eventCount: m.events.length,
            byteLength: m.byteLength,
          })),
        },
      };
    }

    case 'kbd.macro.get': {
      const slot = Number(args.slot);
      if (!Number.isInteger(slot) || slot < 0 || slot >= 50) {
        return { id, ok: false, error: 'macro slot must be 0..49' };
      }
      return { id, ok: true, data: { ...mockKbdMacros[slot] } };
    }

    case 'kbd.macro.set': {
      const slot = Number(args.slot);
      if (!Number.isInteger(slot) || slot < 0 || slot >= 50) {
        return { id, ok: false, error: 'macro slot must be 0..49' };
      }
      const events = Array.isArray(args.events) ? args.events : [];
      const repeat = typeof args.repeat === 'number' ? args.repeat : 1;
      const name = typeof args.name === 'string' && args.name.trim() ? args.name.trim() : `宏 ${slot + 1}`;
      const byteLength = events.reduce((acc: number, ev: Record<string, unknown>) => {
        if (ev.kind === 'mouseMove') return acc + 4;
        const d = Number(ev.delayMs || 0);
        return acc + (d >= 1 && d <= 127 ? 2 : 4);
      }, 0);
      if (byteLength > 250) {
        return { id, ok: false, error: 'macro stream exceeds 250 bytes' };
      }
      mockKbdMacros[slot] = {
        slot,
        name,
        empty: events.length === 0,
        repeat,
        byteLength,
        events: events as KbdMacroDetail['events'],
      };
      return { id, ok: true, data: { ...mockKbdMacros[slot] } };
    }

    case 'kbd.macro.delete': {
      const slot = Number(args.slot);
      if (!Number.isInteger(slot) || slot < 0 || slot >= 50) {
        return { id, ok: false, error: 'macro slot must be 0..49' };
      }
      mockKbdMacros[slot] = {
        slot,
        name: `宏 ${slot + 1}`,
        empty: true,
        repeat: 1,
        byteLength: 0,
        events: [],
      };
      return { id, ok: true, data: { slot, deleted: true } };
    }

    case 'selftest.report':
      return { id, ok: true, data: {} };

    default:
      return {
        id,
        ok: false,
        error: `mock host: unknown command ${cmd}`,
      };
  }
}
