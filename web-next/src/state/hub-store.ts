import { flushSync } from 'react-dom';
import { create } from 'zustand';
import { bridgeCommands } from '../bridge/commands';
import { BridgeError, isWebViewHost } from '../bridge/transport';
import type {
  DeviceEntry,
  DeviceInfoResponse,
  HostEvent,
  HubSettings,
  HubSettingsPatch,
} from '../bridge/types';
import { resolveDeviceConsole } from '../devices/registry';
import { runViewTransition } from '../ui/view-transition';

export type SyncState = 'idle' | 'reading' | 'writing' | 'sleeping' | 'error';

export type ActiveView =
  | { kind: 'shelf' }
  | { kind: 'device'; key: string; mode?: string };

export interface ToastItem {
  id: number;
  tone: 'info' | 'ok' | 'warn' | 'error';
  message: string;
}

export interface HubStore {
  devices: DeviceEntry[];
  deviceInfoMap: Record<string, DeviceInfoResponse>;
  hubSettings: HubSettings | null;
  activeView: ActiveView;
  settingsOpen: boolean;
  windowMaximized: boolean;
  bootstrapped: boolean;
  isMockMode: boolean;
  syncState: SyncState;
  syncMessage: string | null;
  pendingWrites: number;
  transitionDeviceKey: string | null;
  toasts: ToastItem[];

  bootstrap: () => Promise<void>;
  refreshDevices: (probe?: boolean) => Promise<void>;
  selectShelf: () => void;
  selectDevice: (key: string, mode?: string) => Promise<void>;
  setActiveMode: (mode: string) => void;
  setSettingsOpen: (open: boolean) => void;
  toggleSettings: () => void;
  setWindowMaximized: (maximized: boolean) => void;
  setSyncStatus: (state: SyncState, message?: string | null) => void;
  beginWrite: (message: string) => void;
  endWrite: (
    result:
      | { ok: true; message?: string }
      | { ok: false; state: SyncState; message: string },
  ) => void;
  updateHubSettings: (patch: HubSettingsPatch) => Promise<void>;
  renameDevice: (key: string, alias: string) => Promise<void>;
  handleHostEvent: (event: HostEvent) => void;
  pushToast: (tone: ToastItem['tone'], message: string) => void;
  dismissToast: (id: number) => void;
}

let toastSeq = 1;
let hubWriteSeq = 0;
let confirmedHubSettings: HubSettings | null = null;

function extractErrorState(err: unknown): { state: SyncState; message: string } {
  if (err instanceof BridgeError) {
    return {
      state: err.isSleeping ? 'sleeping' : 'error',
      message: err.isSleeping ? '设备已休眠，动一下鼠标或按任意键唤醒' : err.message,
    };
  }
  return {
    state: 'error',
    message: err instanceof Error ? err.message : String(err),
  };
}

export const useHubStore = create<HubStore>((set, get) => ({
  devices: [],
  deviceInfoMap: {},
  hubSettings: null,
  activeView: { kind: 'shelf' },
  settingsOpen: false,
  windowMaximized: false,
  bootstrapped: false,
  isMockMode: import.meta.env.DEV && !isWebViewHost(),
  syncState: 'idle',
  syncMessage: null,
  pendingWrites: 0,
  transitionDeviceKey: null,
  toasts: [],

  pushToast: (tone, message) => {
    const id = toastSeq++;
    set((s) => ({ toasts: [...s.toasts.slice(-3), { id, tone, message }] }));
    setTimeout(() => {
      get().dismissToast(id);
    }, 3600);
  },

  dismissToast: (id) => {
    set((s) => ({ toasts: s.toasts.filter((t) => t.id !== id) }));
  },

  bootstrap: async () => {
    set({ syncState: 'reading', syncMessage: '正在读取设备…' });
    try {
      const [devices, hubSettings] = await Promise.all([
        bridgeCommands.getDevices(),
        bridgeCommands.getHubSettings(),
      ]);

      confirmedHubSettings = hubSettings;
      set({
        devices,
        hubSettings,
        activeView: { kind: 'shelf' },
        bootstrapped: true,
        syncState: 'idle',
        syncMessage: '已同步',
      });

      void Promise.all(
        devices.map(async (d) => {
          try {
            const info = await bridgeCommands.getDeviceInfo(d.key);
            set((s) => ({ deviceInfoMap: { ...s.deviceInfoMap, [d.key]: info } }));
          } catch {
            // Keep background prefetch resilient if a device is sleeping or unavailable.
          }
        }),
      );
    } catch (err) {
      const { state, message } = extractErrorState(err);
      set({ bootstrapped: true, activeView: { kind: 'shelf' }, syncState: state, syncMessage: message });
      get().pushToast('error', message);
    }
  },

  refreshDevices: async (probe = false) => {
    set({
      syncState: 'reading',
      syncMessage: probe ? '正在扫描设备…' : '正在更新设备列表…',
    });
    try {
      const devices = probe
        ? await bridgeCommands.probeDevices()
        : await bridgeCommands.getDevices();

      const currentInfo = { ...get().deviceInfoMap };
      await Promise.all(
        devices.map(async (d) => {
          try {
            currentInfo[d.key] = await bridgeCommands.getDeviceInfo(d.key);
          } catch {
            // Keep existing or fallback.
          }
        }),
      );

      const view = get().activeView;
      let nextView = view;
      if (view.kind === 'device' && !devices.some((d) => d.key === view.key)) {
        nextView = { kind: 'shelf' };
      }

      set({
        devices,
        deviceInfoMap: currentInfo,
        activeView: nextView,
        syncState: 'idle',
        syncMessage: `已找到 ${devices.length} 台设备`,
      });
      if (probe) {
        get().pushToast('ok', `已找到 ${devices.length} 台设备`);
      }
    } catch (err) {
      const { state, message } = extractErrorState(err);
      set({ syncState: state, syncMessage: message });
      get().pushToast(state === 'sleeping' ? 'warn' : 'error', message);
    }
  },

  selectShelf: () => {
    const view = get().activeView;
    if (view.kind === 'shelf') return;
    const transitionKey = view.key;
    flushSync(() => {
      set({ transitionDeviceKey: transitionKey });
    });
    runViewTransition(
      () => {
        set({ activeView: { kind: 'shelf' } });
      },
      'app-stage-root',
      () => {
        set((s) => (s.transitionDeviceKey === transitionKey ? { transitionDeviceKey: null } : s));
      },
    );
  },

  selectDevice: async (key, mode) => {
    const dev = get().devices.find((d) => d.key === key);
    const cachedInfo = get().deviceInfoMap[key];
    const resolved = dev ? resolveDeviceConsole(dev, cachedInfo) : null;

    flushSync(() => {
      set({ transitionDeviceKey: key });
    });
    runViewTransition(
      () => {
        set({
          activeView: {
            kind: 'device',
            key,
            mode: mode ?? resolved?.defaultMode,
          },
        });
      },
      'app-stage-root',
      () => {
        set((s) => (s.transitionDeviceKey === key ? { transitionDeviceKey: null } : s));
      },
    );

    if (!cachedInfo) {
      try {
        const info = await bridgeCommands.getDeviceInfo(key);
        set((s) => ({ deviceInfoMap: { ...s.deviceInfoMap, [key]: info } }));
      } catch {
        // Fallback to capability-empty shell.
      }
    }
  },

  setActiveMode: (mode) => {
    const view = get().activeView;
    if (view.kind !== 'device' || view.mode === mode) return;
    set({ activeView: { ...view, mode } });
  },

  setSettingsOpen: (open) => set({ settingsOpen: open }),
  toggleSettings: () => set((s) => ({ settingsOpen: !s.settingsOpen })),
  setWindowMaximized: (windowMaximized) => set({ windowMaximized }),
  setSyncStatus: (syncState, syncMessage = null) => set({ syncState, syncMessage }),

  beginWrite: (message) =>
    set((s) => ({
      pendingWrites: s.pendingWrites + 1,
      syncState: 'writing',
      syncMessage: message,
    })),

  endWrite: (result) =>
    set((s) => {
      const next = Math.max(0, s.pendingWrites - 1);
      if (!result.ok) {
        return {
          pendingWrites: next,
          syncState: next > 0 ? 'writing' : result.state,
          syncMessage: next > 0 ? s.syncMessage : result.message,
        };
      }
      return {
        pendingWrites: next,
        syncState: next > 0 ? 'writing' : 'idle',
        syncMessage: next > 0 ? s.syncMessage : (result.message ?? '已保存'),
      };
    }),

  updateHubSettings: async (patch) => {
    const seq = ++hubWriteSeq;
    const prev = get().hubSettings;
    if (prev && !confirmedHubSettings) {
      confirmedHubSettings = prev;
    }
    if (prev) {
      set({ hubSettings: { ...prev, ...patch } });
    }
    get().beginWrite('正在保存设置…');
    try {
      const updated = await bridgeCommands.setHubSettings(patch);
      confirmedHubSettings = updated;
      if (hubWriteSeq === seq) {
        set({ hubSettings: updated });
      }
      get().endWrite({ ok: true, message: '设置已保存' });
    } catch (err) {
      const { state, message } = extractErrorState(err);
      if (hubWriteSeq === seq && confirmedHubSettings) {
        set({ hubSettings: confirmedHubSettings });
      }
      get().endWrite({ ok: false, state, message });
      get().pushToast('error', message);
    }
  },

  renameDevice: async (key, alias) => {
    get().beginWrite('正在保存名称…');
    try {
      const res = await bridgeCommands.renameDevice(key, alias);
      set((s) => {
        let nextSettings = s.hubSettings;
        if (nextSettings) {
          const nextAliases = { ...nextSettings.deviceAliases };
          if (res.alias) {
            nextAliases[key] = res.alias;
          } else {
            delete nextAliases[key];
          }
          nextSettings = { ...nextSettings, deviceAliases: nextAliases };
          confirmedHubSettings = nextSettings;
        }
        return {
          devices: s.devices.map((d) => (d.key === key ? { ...d, name: res.name } : d)),
          hubSettings: nextSettings,
        };
      });
      get().endWrite({ ok: true, message: '已保存' });
      get().pushToast('ok', `已改名为「${res.name}」`);
    } catch (err) {
      const { state, message } = extractErrorState(err);
      get().endWrite({ ok: false, state, message });
      get().pushToast('error', message);
    }
  },

  handleHostEvent: (evt) => {
    if (evt.event === 'battery') {
      set((s) => ({
        devices: s.devices.map((d) => (d.key === evt.key ? { ...d, battery: evt.value } : d)),
      }));
    } else if (evt.event === 'device') {
      const exists = get().devices.some((d) => d.key === evt.key);
      if (!exists && evt.connected) {
        void get().refreshDevices(false);
        return;
      }
      set((s) => ({
        devices: s.devices.map((d) =>
          d.key === evt.key ? { ...d, connected: evt.connected } : d,
        ),
      }));
    } else if (evt.event === 'windowState') {
      set({ windowMaximized: evt.maximized });
    } else if (evt.event === 'navigate') {
      if (!evt.deviceKey) return;
      const found = get().devices.some((d) => d.key === evt.deviceKey);
      if (found) {
        void get().selectDevice(evt.deviceKey);
      } else {
        void get()
          .refreshDevices(false)
          .then(() => {
            if (get().devices.some((d) => d.key === evt.deviceKey)) {
              void get().selectDevice(evt.deviceKey);
            }
          });
      }
    } else if (evt.event === 'error') {
      const sleeping = /sleeping|wake/i.test(evt.message);
      set({
        syncState: sleeping ? 'sleeping' : 'error',
        syncMessage: evt.message,
      });
      get().pushToast(sleeping ? 'warn' : 'error', evt.message);
    }
  },
}));
