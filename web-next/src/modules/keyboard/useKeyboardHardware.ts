import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { bridgeCommands } from '../../bridge/commands';
import { BridgeError } from '../../bridge/transport';
import { classifyBridgeError } from '../../bridge/ui-helpers';
import type { DeviceEntry, KbdKeyEntry, KbdKeysState, KbdState } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import { FieldVersionTracker } from '../../state/optimistic';
import { resolveKeyboardLayout } from './bc98-layout';
import {
  applyKbdPatchOptimistic,
  DEFAULT_DEEP_PRESETS,
  DEFAULT_MAIN_MODES,
  DEFAULT_SHALLOW_PRESETS,
  revertKbdField,
} from './kbd-state-helpers';

export function useKeyboardHardware(device: DeviceEntry) {
  const pushToast = useHubStore((s) => s.pushToast);
  const setSyncStatus = useHubStore((s) => s.setSyncStatus);
  const beginWrite = useHubStore((s) => s.beginWrite);
  const endWrite = useHubStore((s) => s.endWrite);
  const deviceInfoMap = useHubStore((s) => s.deviceInfoMap);

  const kbdCaps = deviceInfoMap[device.key]?.capabilities.keyboard;
  const profiles = useMemo(
    () => (kbdCaps?.profiles && kbdCaps.profiles.length > 0 ? kbdCaps.profiles : [0, 1, 2, 3]),
    [kbdCaps?.profiles],
  );
  const fnSlot = kbdCaps?.fnSlot !== undefined ? kbdCaps.fnSlot : 65;
  const fnLayerEnabled = kbdCaps?.fnLayer !== false;
  const layout = useMemo(
    () => resolveKeyboardLayout(kbdCaps?.layout, kbdCaps?.keys),
    [kbdCaps?.layout, kbdCaps?.keys],
  );
  const supportedOptions = kbdCaps?.options;
  const brightnessMax = kbdCaps?.brightnessMax ?? 4;
  const speedMax = kbdCaps?.speedMax ?? 4;
  const debounceMin = kbdCaps?.debounce?.min ?? 1;
  const debounceMax = kbdCaps?.debounce?.max ?? 10;
  const shallowPresets = useMemo(
    () =>
      kbdCaps?.sleep?.shallowPresetsSec?.length
        ? kbdCaps.sleep.shallowPresetsSec
        : DEFAULT_SHALLOW_PRESETS,
    [kbdCaps?.sleep?.shallowPresetsSec],
  );
  const deepPresets = useMemo(
    () =>
      kbdCaps?.sleep?.deepPresetsSec?.length
        ? kbdCaps.sleep.deepPresetsSec
        : DEFAULT_DEEP_PRESETS,
    [kbdCaps?.sleep?.deepPresetsSec],
  );
  const mainModes = useMemo(
    () => (kbdCaps?.mainModes?.length ? kbdCaps.mainModes : DEFAULT_MAIN_MODES),
    [kbdCaps?.mainModes],
  );
  const sideModes = useMemo(() => kbdCaps?.sideModes ?? [], [kbdCaps?.sideModes]);
  const hasSidelight = sideModes.length > 0;

  const [loading, setLoading] = useState(true);
  const [kbdState, setKbdState] = useState<KbdState | null>(null);
  const [keysState, setKeysState] = useState<KbdKeysState | null>(null);
  const [switchingProfile, setSwitchingProfile] = useState(false);
  const [resettingLayer, setResettingLayer] = useState(false);

  const trackerRef = useRef(new FieldVersionTracker());
  const confirmedKbdRef = useRef<KbdState | null>(null);
  const confirmedKeysRef = useRef<KbdKeysState | null>(null);

  const finishWriteError = useCallback(
    (actionLabel: string, err: unknown) => {
      const { state, message, isSleeping } = classifyBridgeError(
        err,
        '键盘休眠中，请按任意键唤醒后重试',
      );
      endWrite({ ok: false, state, message });
      pushToast(isSleeping ? 'warn' : 'error', isSleeping ? message : `${actionLabel}失败：${message}`);
    },
    [endWrite, pushToast],
  );

  const loadKeyboard = useCallback(async () => {
    trackerRef.current.reset();
    setLoading(true);
    setSyncStatus('reading', '正在读取键盘板载状态…');
    try {
      const [stRes, keysRes] = await Promise.allSettled([
        bridgeCommands.getKbdState(device.key),
        bridgeCommands.getKbdKeys(device.key),
      ]);

      let anySleep = false;
      if (stRes.status === 'fulfilled') {
        confirmedKbdRef.current = stRes.value;
        setKbdState(stRes.value);
      } else {
        confirmedKbdRef.current = null;
        setKbdState(null);
        if (stRes.reason instanceof BridgeError && stRes.reason.isSleeping) anySleep = true;
      }

      if (keysRes.status === 'fulfilled') {
        confirmedKeysRef.current = keysRes.value;
        setKeysState(keysRes.value);
      } else {
        confirmedKeysRef.current = null;
        setKeysState(null);
        if (keysRes.reason instanceof BridgeError && keysRes.reason.isSleeping) anySleep = true;
      }

      if (anySleep || device.asleep) {
        setSyncStatus('sleeping', '键盘浅休眠中，按任意键唤醒');
      } else {
        setSyncStatus('idle', '已同步');
      }
    } finally {
      setLoading(false);
    }
  }, [device.asleep, device.key, setSyncStatus]);

  useEffect(() => {
    void loadKeyboard();
  }, [loadKeyboard]);

  const patchKbdState = async (
    patch: Record<string, unknown>,
    label: string,
    fieldKey: string,
  ) => {
    const seq = trackerRef.current.next(fieldKey);
    setKbdState((prev) => applyKbdPatchOptimistic(prev, patch));
    beginWrite(`正在写入${label}…`);
    try {
      const updated = await bridgeCommands.setKbdState(device.key, patch);
      confirmedKbdRef.current = updated;
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        setKbdState(updated);
      }
      endWrite({ ok: true, message: '已保存' });
      return updated;
    } catch (err) {
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        setKbdState((prev) => revertKbdField(prev, confirmedKbdRef.current, fieldKey));
      }
      finishWriteError(`设置${label}`, err);
      return null;
    }
  };

  const handleProfileChange = async (p: number) => {
    if (p === kbdState?.profile || switchingProfile) return;
    setSwitchingProfile(true);
    try {
      const updated = await patchKbdState({ profile: p }, `配置层 P${p + 1}`, 'profile');
      if (updated) {
        try {
          const km = await bridgeCommands.getKbdKeys(device.key, p);
          confirmedKeysRef.current = km;
          setKeysState(km);
        } catch (err) {
          const { state, message, isSleeping } = classifyBridgeError(
            err,
            '键盘休眠中，请按任意键唤醒后重试',
          );
          setSyncStatus(state, message);
          pushToast(
            isSleeping ? 'warn' : 'error',
            isSleeping ? message : `读取配置层按键失败：${message}`,
          );
        }
      }
    } finally {
      setSwitchingProfile(false);
    }
  };

  const handleWriteKeyEntry = async (
    slot: number,
    entry: KbdKeyEntry,
    fnLayer: boolean,
    _label?: string,
  ) => {
    if (fnLayer && fnSlot !== null && slot === fnSlot) {
      pushToast('warn', 'Fn 键自身在 Fn 功能层不可重新映射');
      return;
    }
    // Reject KB Lock [10, 7, 0, 0]
    if (entry[0] === 10 && entry[1] === 7 && entry[2] === 0 && entry[3] === 0) {
      pushToast('error', '硬件禁止映射 KB Lock');
      return;
    }

    const fieldKey = `key.${fnLayer ? 'fn' : 'base'}.${slot}`;
    const seq = trackerRef.current.next(fieldKey);
    const applyKey = (
      base: KbdKeysState | null,
      isFn: boolean,
      targetSlot: number,
      val: KbdKeyEntry,
    ): KbdKeysState | null => {
      if (!base) return base;
      const arr = isFn ? [...base.fn] : [...base.base];
      arr[targetSlot] = val;
      return isFn ? { ...base, fn: arr } : { ...base, base: arr };
    };

    setKeysState((prev) => applyKey(prev, fnLayer, slot, entry));
    beginWrite('正在写入改键并回读校验…');
    try {
      const res = await bridgeCommands.setKbdKey(device.key, {
        profile: kbdState?.profile ?? undefined,
        fnLayer,
        slot,
        entry,
      });
      confirmedKeysRef.current = applyKey(
        confirmedKeysRef.current,
        res.fnLayer,
        res.slot,
        res.entry,
      );
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        setKeysState((prev) => applyKey(prev, res.fnLayer, res.slot, res.entry));
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        const fallback = fnLayer
          ? confirmedKeysRef.current?.fn?.[slot]
          : confirmedKeysRef.current?.base?.[slot];
        if (fallback) {
          setKeysState((prev) => applyKey(prev, fnLayer, slot, fallback));
        }
      }
      finishWriteError('改键', err);
    }
  };

  const handleResetCurrentLayer = async (fnLayer: boolean) => {
    setResettingLayer(true);
    beginWrite(`正在重置 ${fnLayer ? 'Fn' : 'Base'} 层默认键位…`);
    try {
      const res = await bridgeCommands.resetKbdKeys(
        device.key,
        fnLayer,
        kbdState?.profile ?? undefined,
      );
      confirmedKeysRef.current = res;
      setKeysState(res);
      endWrite({ ok: true, message: '已保存' });
      pushToast('ok', `已恢复并回读校验 ${fnLayer ? 'Fn 功能层' : 'Base 主层'} 默认键位`);
    } catch (err) {
      finishWriteError('重置层默认', err);
    } finally {
      setResettingLayer(false);
    }
  };

  return {
    profiles,
    fnSlot,
    fnLayerEnabled,
    layout,
    supportedOptions,
    brightnessMax,
    speedMax,
    debounceMin,
    debounceMax,
    shallowPresets,
    deepPresets,
    mainModes,
    sideModes,
    hasSidelight,
    loading,
    kbdState,
    setKbdState,
    keysState,
    switchingProfile,
    resettingLayer,
    patchKbdState,
    handleProfileChange,
    handleWriteKeyEntry,
    handleResetCurrentLayer,
  };
}
