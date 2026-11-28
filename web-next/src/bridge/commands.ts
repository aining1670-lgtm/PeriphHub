import { invoke } from './transport';
import type {
  DeviceEntry,
  DeviceInfoResponse,
  DeviceRenameResponse,
  DpiSetResponse,
  DpiStageResponse,
  DpiState,
  HubSettings,
  HubSettingsPatch,
  KbdKeyEntry,
  KbdKeySetResponse,
  KbdKeysState,
  KbdMacroDetail,
  KbdMacroEvent,
  KbdMacroListResponse,
  KbdState,
  KeysState,
  LedState,
  MacroDefinition,
  MacroListItem,
  MacroUploadResponse,
  MiscState,
  RateState,
  SensorState,
} from './types';

function withKey(deviceKey?: string, extra?: Record<string, unknown>): Record<string, unknown> {
  if (!deviceKey) return extra ?? {};
  return { key: deviceKey, deviceKey, ...extra };
}

export const bridgeCommands = {
  // Discovery & Hub
  getDevices: () => invoke<DeviceEntry[]>('devices.get'),
  probeDevices: (deviceKey?: string) =>
    invoke<DeviceEntry[]>('device.probe', deviceKey ? { key: deviceKey } : undefined, {
      timeoutMs: 25_000,
    }),
  getDeviceInfo: (deviceKey?: string) =>
    invoke<DeviceInfoResponse>('device.info', deviceKey ? { deviceKey } : undefined),
  renameDevice: (deviceKey: string, alias: string) =>
    invoke<DeviceRenameResponse>('device.rename', { deviceKey, alias }, { serialized: true }),

  getHubSettings: () => invoke<HubSettings>('hub.get'),
  setHubSettings: (patch: HubSettingsPatch) =>
    invoke<HubSettings>('hub.set', patch as Record<string, unknown>, { serialized: true }),
  testLowBatteryNotify: () => invoke<{ sent: boolean }>('hub.testNotify'),

  // Mouse / Generic Driver Commands
  getLed: (deviceKey?: string) =>
    invoke<LedState>('led.get', deviceKey ? withKey(deviceKey) : undefined),
  setLed: (mode: number, deviceKey?: string, color?: string) =>
    invoke<LedState>('led.set', withKey(deviceKey, color ? { mode, color } : { mode }), {
      serialized: true,
    }),

  getDpi: (deviceKey: string) => invoke<DpiState>('dpi.get', withKey(deviceKey)),
  setDpi: (
    deviceKey: string,
    payload: { slot: number; dpi: number } | { slot: number; dpiX: number; dpiY: number },
  ) => invoke<DpiSetResponse>('dpi.set', withKey(deviceKey, payload), { serialized: true }),
  setDpiStage: (deviceKey: string, active: number, count: number) =>
    invoke<DpiStageResponse>('dpi.stage', withKey(deviceKey, { active, count }), {
      serialized: true,
    }),

  getRate: (deviceKey: string) => invoke<RateState>('rate.get', withKey(deviceKey)),
  setRate: (deviceKey: string, payload: { hz: number } | { code: number }) =>
    invoke<RateState>('rate.set', withKey(deviceKey, payload), { serialized: true }),

  getSensor: (deviceKey: string) => invoke<SensorState>('sensor.get', withKey(deviceKey)),
  setSensor: (
    deviceKey: string,
    patch: Partial<{ lod: number; ripple: boolean; fixline: boolean; motion: boolean; mode: number }>,
  ) => invoke<Partial<SensorState>>('sensor.set', withKey(deviceKey, patch), { serialized: true }),

  getMisc: (deviceKey: string) => invoke<MiscState>('misc.get', withKey(deviceKey)),
  setMisc: (
    deviceKey: string,
    patch: Partial<{
      debounce: number;
      sleep: number;
      fireLoops: number;
      fireInterval: number;
      scrollReverse: boolean;
    }>,
  ) => invoke<Partial<MiscState>>('misc.set', withKey(deviceKey, patch), { serialized: true }),

  getKeys: (deviceKey?: string) =>
    invoke<KeysState>('keys.get', deviceKey ? withKey(deviceKey) : undefined),
  setKey: (deviceKey: string, index: number, binding: string) =>
    invoke<KeysState>('keys.set', withKey(deviceKey, { index, binding }), { serialized: true }),

  // Mouse Local Macro Library
  listMacros: () => invoke<MacroListItem[]>('macros.list'),
  saveMacro: (slot: number, macro: MacroDefinition) =>
    invoke<{ slot: number }>('macros.save', { slot, macro }, { serialized: true }),
  uploadMacro: (slot: number, buttonIndex: number, deviceKey?: string) =>
    invoke<MacroUploadResponse>(
      'macros.upload',
      withKey(deviceKey, { slot, buttonIndex }),
      { serialized: true, timeoutMs: 25_000 },
    ),
  importMacro: (json: string, slot?: number) =>
    invoke<{ slot: number; summary: string }>(
      'macro.import',
      slot !== undefined ? { json, slot } : { json },
      { serialized: true },
    ),
  exportMacro: (slot: number) => invoke<{ json: string }>('macro.export', { slot }),
  deleteMacro: (slot: number) =>
    invoke<{ slot: number }>('macros.delete', { slot }, { serialized: true }),

  // ROYUAN / AKOS BC98 Keyboard Commands
  getKbdState: (deviceKey: string) => invoke<KbdState>('kbd.get', { deviceKey }),
  setKbdState: (deviceKey: string, patch: Record<string, unknown>) =>
    invoke<KbdState>('kbd.set', { deviceKey, ...patch }, { serialized: true }),

  getKbdKeys: (deviceKey: string, profile?: number) =>
    invoke<KbdKeysState>(
      'kbd.keys.get',
      profile !== undefined ? { deviceKey, profile } : { deviceKey },
    ),
  setKbdKey: (
    deviceKey: string,
    payload: { profile?: number; fnLayer?: boolean; slot: number; entry: KbdKeyEntry },
  ) => invoke<KbdKeySetResponse>('kbd.keys.set', { deviceKey, ...payload }, { serialized: true }),
  resetKbdKeys: (deviceKey: string, fnLayer: boolean, profile?: number) =>
    invoke<KbdKeysState>(
      'kbd.keys.reset',
      profile !== undefined ? { deviceKey, fnLayer, profile } : { deviceKey, fnLayer },
      { serialized: true, timeoutMs: 25_000 },
    ),

  listKbdMacros: (deviceKey: string, count?: number) =>
    invoke<KbdMacroListResponse>(
      'kbd.macro.list',
      count !== undefined ? { deviceKey, count } : { deviceKey },
    ),
  getKbdMacro: (deviceKey: string, slot: number) =>
    invoke<KbdMacroDetail>('kbd.macro.get', { deviceKey, slot }),
  setKbdMacro: (
    deviceKey: string,
    payload: { slot: number; name?: string; repeat: number; events: KbdMacroEvent[] },
  ) => invoke<KbdMacroDetail>('kbd.macro.set', { deviceKey, ...payload }, { serialized: true }),
  deleteKbdMacro: (deviceKey: string, slot: number) =>
    invoke<{ slot: number; deleted: boolean }>('kbd.macro.delete', { deviceKey, slot }, { serialized: true }),

  // Native Frameless Window Controls
  windowState: () => invoke<{ maximized: boolean }>('window.state'),
  windowMinimize: () => invoke<{ maximized: boolean }>('window.minimize'),
  windowMaximize: () => invoke<{ maximized: boolean }>('window.maximize'),
  windowClose: () => invoke<Record<string, never>>('window.close'),
  windowDrag: () => invoke<Record<string, never>>('window.drag'),
  windowResize: (
    edge:
      | 'left'
      | 'right'
      | 'top'
      | 'bottom'
      | 'topLeft'
      | 'topRight'
      | 'bottomLeft'
      | 'bottomRight',
  ) => invoke<Record<string, never>>('window.resize', { edge }),
  windowSysMenu: (x: number, y: number) =>
    invoke<Record<string, never>>('window.sysmenu', { x, y }),
  windowTheme: (dark: boolean) => invoke<Record<string, never>>('window.theme', { dark }),

  // Selftest hook
  reportSelftest: (payload: Record<string, unknown>) =>
    invoke<Record<string, never>>('selftest.report', payload),
};
