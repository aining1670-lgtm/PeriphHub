import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { bridgeCommands } from '../../bridge/commands';
import { BridgeError } from '../../bridge/transport';
import { classifyBridgeError } from '../../bridge/ui-helpers';
import type {
  DeviceCapabilities,
  DeviceEntry,
  DpiState,
  KeyBindingItem,
  LedState,
  MiscState,
  RateState,
  SensorState,
} from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import { FieldVersionTracker } from '../../state/optimistic';
import { BUTTON_ID_BY_INDEX, decodeHardwareBinding } from './mouse-codec';
import { buildDpiAnchors } from './mouse-utils';

export function useMouseHardware(device: DeviceEntry) {
  const pushToast = useHubStore((s) => s.pushToast);
  const setSyncStatus = useHubStore((s) => s.setSyncStatus);
  const beginWrite = useHubStore((s) => s.beginWrite);
  const endWrite = useHubStore((s) => s.endWrite);
  const deviceInfoMap = useHubStore((s) => s.deviceInfoMap);

  const info = deviceInfoMap[device.key];
  const caps: DeviceCapabilities | undefined = info?.capabilities;
  const driver = info?.driver ?? device.driver;

  const dpiMin = caps?.dpi?.min ?? 50;
  const dpiMax = caps?.dpi?.max ?? 32000;
  const dpiStep = caps?.dpi?.step ?? 50;
  const maxStages = caps?.dpi?.maxStages ?? 6;
  const supportsXySplit = Boolean(caps?.dpi?.xySplit);
  const dpiAnchors = useMemo(() => buildDpiAnchors(dpiMin, dpiMax), [dpiMin, dpiMax]);

  const rateOptions = useMemo(
    () => (caps?.rates && caps.rates.length > 0 ? caps.rates : [125, 250, 500, 1000]),
    [caps?.rates],
  );
  const lodOptions = useMemo(
    () => (caps?.sensor?.lod && caps.sensor.lod.length > 0 ? caps.sensor.lod : ['1.0', '2.0']),
    [caps?.sensor?.lod],
  );
  const sensorModes = useMemo(() => caps?.sensor?.modes ?? [], [caps?.sensor?.modes]);
  const sleepPresets = useMemo(
    () =>
      caps?.misc?.sleep?.presetsSec && caps.misc.sleep.presetsSec.length > 0
        ? caps.misc.sleep.presetsSec
        : [60, 180, 300, 900],
    [caps?.misc?.sleep?.presetsSec],
  );
  const isDiscreteSleep = caps?.misc?.sleep?.discrete ?? true;
  const sleepMin = caps?.misc?.sleep?.min ?? 10;
  const sleepMax = caps?.misc?.sleep?.max ?? 900;
  const sleepStep = caps?.misc?.sleep?.step ?? 10;
  const debounceMin = caps?.misc?.debounce?.min ?? 0;
  const debounceMax = caps?.misc?.debounce?.max ?? 30;
  const supportsFire = Boolean(caps?.misc?.fire);
  const supportsScrollReverse = Boolean(caps?.misc?.scrollReverse);
  const supportsReceiverLed = Boolean(caps?.lighting?.receiverLed);
  const supportsMacros = caps?.macros !== false;
  const mouseLedModes = useMemo(() => caps?.lighting?.mouseLed ?? [], [caps?.lighting?.mouseLed]);
  const hasAnyLighting = Boolean(supportsReceiverLed || mouseLedModes.length > 0);
  const loadSeqRef = useRef(0);

  // Hardware states (null means unread / unsupported — never fabricate defaults)
  const [loading, setLoading] = useState(true);
  const [dpiState, setDpiState] = useState<DpiState | null>(null);
  const [stageIdx, setStageIdx] = useState(0);
  const [xySplit, setXySplit] = useState(false);
  const [editAxis, setEditAxis] = useState<'x' | 'y'>('y');
  const [localSplitX, setLocalSplitX] = useState<Record<number, number | null>>({});

  const [rateState, setRateState] = useState<RateState | null>(null);
  const [sensorState, setSensorState] = useState<SensorState | null>(null);
  const [miscState, setMiscState] = useState<MiscState | null>(null);
  const [ledState, setLedState] = useState<LedState | null>(null);
  const [bindings, setBindings] = useState<KeyBindingItem[]>([]);
  const [resetting, setResetting] = useState(false);

  const trackerRef = useRef(new FieldVersionTracker());
  const confirmedRef = useRef<{
    dpiState: DpiState | null;
    stageIdx: number;
    rateState: RateState | null;
    sensorState: SensorState | null;
    miscState: MiscState | null;
    ledState: LedState | null;
    bindings: KeyBindingItem[];
  }>({
    dpiState: null,
    stageIdx: 0,
    rateState: null,
    sensorState: null,
    miscState: null,
    ledState: null,
    bindings: [],
  });

  const finishWriteError = useCallback(
    (actionLabel: string, err: unknown) => {
      const { state, message, isSleeping } = classifyBridgeError(
        err,
        '设备浅休眠中，请移动鼠标唤醒后重试',
      );
      endWrite({ ok: false, state, message });
      pushToast(isSleeping ? 'warn' : 'error', isSleeping ? message : `${actionLabel}失败：${message}`);
    },
    [endWrite, pushToast],
  );

  const loadAllHardware = useCallback(async () => {
    const seq = ++loadSeqRef.current;
    trackerRef.current.reset();
    setLoading(true);
    setSyncStatus('reading', '正在读取鼠标性能参数…');

    let anySleep = false;
    const noteSleep = (err: unknown) => {
      if (err instanceof BridgeError && err.isSleeping) anySleep = true;
    };

    try {
      const dpiPromise =
        !caps || caps.dpi
          ? bridgeCommands.getDpi(device.key).then(
              (val) => {
                if (loadSeqRef.current !== seq) return;
                confirmedRef.current.dpiState = val;
                setDpiState(val);
                if (typeof val.active === 'number' && val.active >= 0) {
                  confirmedRef.current.stageIdx = val.active;
                  setStageIdx(val.active);
                }
                setLoading(false);
              },
              (err) => {
                if (loadSeqRef.current !== seq) return;
                noteSleep(err);
                confirmedRef.current.dpiState = null;
                setDpiState(null);
                setLoading(false);
              },
            )
          : Promise.resolve().then(() => {
              if (loadSeqRef.current === seq) {
                confirmedRef.current.dpiState = null;
                setDpiState(null);
              }
            });

      const ratePromise =
        !caps || caps.rates.length > 0
          ? bridgeCommands.getRate(device.key).then(
              (val) => {
                if (loadSeqRef.current === seq) {
                  confirmedRef.current.rateState = val;
                  setRateState(val);
                }
              },
              (err) => {
                if (loadSeqRef.current !== seq) return;
                noteSleep(err);
                confirmedRef.current.rateState = null;
                setRateState(null);
              },
            )
          : Promise.resolve().then(() => {
              if (loadSeqRef.current === seq) {
                confirmedRef.current.rateState = null;
                setRateState(null);
              }
            });

      const sensorPromise =
        !caps || caps.sensor
          ? bridgeCommands.getSensor(device.key).then(
              (val) => {
                if (loadSeqRef.current === seq) {
                  confirmedRef.current.sensorState = val;
                  setSensorState(val);
                }
              },
              (err) => {
                if (loadSeqRef.current !== seq) return;
                noteSleep(err);
                confirmedRef.current.sensorState = null;
                setSensorState(null);
              },
            )
          : Promise.resolve().then(() => {
              if (loadSeqRef.current === seq) {
                confirmedRef.current.sensorState = null;
                setSensorState(null);
              }
            });

      await Promise.all([dpiPromise, ratePromise, sensorPromise]);
      if (loadSeqRef.current !== seq) return;

      if (anySleep || device.asleep) {
        setSyncStatus('sleeping', '设备浅休眠中，移动鼠标可唤醒');
      } else {
        setSyncStatus('idle', '已同步');
      }
    } finally {
      if (loadSeqRef.current === seq) {
        setLoading(false);
      }
    }

    if (anySleep || device.asleep || loadSeqRef.current !== seq) return;

    // Stage 2: silently prefetch secondary tabs without blocking interaction
    const [miscRes, keysRes, ledRes] = await Promise.allSettled([
      !caps || caps.misc ? bridgeCommands.getMisc(device.key) : Promise.resolve(null),
      !caps || caps.keys ? bridgeCommands.getKeys(device.key) : Promise.resolve(null),
      hasAnyLighting ? bridgeCommands.getLed(device.key) : Promise.resolve(null),
    ]);
    if (loadSeqRef.current !== seq) return;

    if (miscRes.status === 'fulfilled' && miscRes.value) {
      confirmedRef.current.miscState = miscRes.value;
      setMiscState(miscRes.value);
    } else {
      confirmedRef.current.miscState = null;
      setMiscState(null);
    }
    if (keysRes.status === 'fulfilled' && keysRes.value) {
      const nextBindings = keysRes.value.bindings ?? [];
      confirmedRef.current.bindings = nextBindings;
      setBindings(nextBindings);
    } else {
      confirmedRef.current.bindings = [];
      setBindings([]);
    }
    if (ledRes.status === 'fulfilled' && ledRes.value) {
      confirmedRef.current.ledState = ledRes.value;
      setLedState(ledRes.value);
    } else {
      confirmedRef.current.ledState = null;
      setLedState(null);
    }
  }, [caps, device.asleep, device.key, hasAnyLighting, setSyncStatus]);

  useEffect(() => {
    setLocalSplitX({});
    setXySplit(false);
    setEditAxis('y');
    void loadAllHardware();
    return () => {
      loadSeqRef.current += 1;
    };
  }, [loadAllHardware]);

  const currentLevel = dpiState?.levels?.[stageIdx] ?? null;
  const activeAxis = supportsXySplit && xySplit ? editAxis : 'y';
  const stageYVal = currentLevel?.y ?? null;
  const stageXVal =
    localSplitX[stageIdx] !== undefined ? localSplitX[stageIdx] : (currentLevel?.x ?? null);

  const commitDpi = async (dpiVal: number) => {
    if (dpiVal < dpiMin || dpiVal > dpiMax || dpiVal % dpiStep !== 0) {
      pushToast('error', `DPI 必须在 ${dpiMin}–${dpiMax} 且为 ${dpiStep} 的倍数`);
      return;
    }

    const targetSlot = stageIdx;
    const prevLocalX = localSplitX[targetSlot];
    let payload: { slot: number; dpi: number } | { slot: number; dpiX: number; dpiY: number };
    let optimisticLevel: { dpi: number; x: number | null; y: number };
    if (supportsXySplit && xySplit) {
      const nextX = activeAxis === 'x' ? dpiVal : stageXVal;
      const nextY = activeAxis === 'y' ? dpiVal : stageYVal;
      if (activeAxis === 'x') {
        setLocalSplitX((prev) => ({ ...prev, [targetSlot]: dpiVal }));
      }
      if (nextX === null || nextY === null) {
        pushToast('warn', '另一轴尚未设定或无法回读，请先设置两轴数值后再写入');
        return;
      }
      payload = { slot: targetSlot, dpiX: nextX, dpiY: nextY };
      optimisticLevel = { dpi: nextY, x: nextX, y: nextY };
    } else {
      payload = { slot: targetSlot, dpi: dpiVal };
      optimisticLevel = { dpi: dpiVal, x: dpiVal, y: dpiVal };
    }

    const fieldKey = `dpi.${targetSlot}`;
    const seq = trackerRef.current.next(fieldKey);
    const applyLevel = (
      base: DpiState | null,
      slot: number,
      lvl: DpiState['levels'][number],
    ): DpiState | null => {
      if (!base) return base;
      const levels = [...base.levels];
      while (levels.length <= slot) levels.push(null);
      levels[slot] = lvl;
      return { ...base, levels };
    };

    setDpiState((prev) =>
      applyLevel(prev, targetSlot, {
        ...optimisticLevel,
        unverified: prev?.levels?.[targetSlot]?.unverified,
      }),
    );

    beginWrite('正在写入 DPI…');
    try {
      const res = await bridgeCommands.setDpi(device.key, payload);
      const confirmedLvl = {
        dpi: res.dpi ?? res.dpiY,
        x: res.dpiX,
        y: res.dpiY,
        unverified: res.unverified,
      };
      confirmedRef.current.dpiState = applyLevel(
        confirmedRef.current.dpiState,
        targetSlot,
        confirmedLvl,
      );
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        setDpiState((prev) => applyLevel(prev, targetSlot, confirmedLvl));
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        const fallbackLvl = confirmedRef.current.dpiState?.levels?.[targetSlot] ?? null;
        setDpiState((prev) => applyLevel(prev, targetSlot, fallbackLvl));
        if (supportsXySplit && xySplit && activeAxis === 'x') {
          setLocalSplitX((prev) => ({ ...prev, [targetSlot]: prevLocalX ?? null }));
        }
      }
      finishWriteError('写入 DPI', err);
    }
  };

  const handleStageSelect = async (idx: number) => {
    const curCount = dpiState?.count ?? 1;
    const nextCount = idx >= curCount ? idx + 1 : curCount;
    const seq = trackerRef.current.next('dpi.stage');
    setStageIdx(idx);
    setDpiState((prev) => (prev ? { ...prev, active: idx, count: nextCount } : prev));
    beginWrite('正在切换 DPI 档位…');
    try {
      const res = await bridgeCommands.setDpiStage(device.key, idx, nextCount);
      confirmedRef.current.stageIdx = res.active;
      if (confirmedRef.current.dpiState) {
        confirmedRef.current.dpiState = {
          ...confirmedRef.current.dpiState,
          active: res.active,
          count: res.count,
        };
      }
      if (trackerRef.current.isLatest('dpi.stage', seq)) {
        setStageIdx(res.active);
        setDpiState((prev) => (prev ? { ...prev, active: res.active, count: res.count } : prev));
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest('dpi.stage', seq)) {
        setStageIdx(confirmedRef.current.stageIdx);
        const confirmedDpi = confirmedRef.current.dpiState;
        if (confirmedDpi) {
          setDpiState((prev) =>
            prev ? { ...prev, active: confirmedDpi.active, count: confirmedDpi.count } : prev,
          );
        }
      }
      finishWriteError('切换 DPI 档位', err);
    }
  };

  const handleStageCountChange = async (cnt: number) => {
    const nextActive = Math.min(stageIdx, cnt - 1);
    const seq = trackerRef.current.next('dpi.stage');
    setStageIdx(nextActive);
    setDpiState((prev) => (prev ? { ...prev, active: nextActive, count: cnt } : prev));
    beginWrite('正在更新档位数…');
    try {
      const res = await bridgeCommands.setDpiStage(device.key, nextActive, cnt);
      confirmedRef.current.stageIdx = res.active;
      if (confirmedRef.current.dpiState) {
        confirmedRef.current.dpiState = {
          ...confirmedRef.current.dpiState,
          active: res.active,
          count: res.count,
        };
      }
      if (trackerRef.current.isLatest('dpi.stage', seq)) {
        setStageIdx(res.active);
        setDpiState((prev) => (prev ? { ...prev, active: res.active, count: res.count } : prev));
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest('dpi.stage', seq)) {
        setStageIdx(confirmedRef.current.stageIdx);
        const confirmedDpi = confirmedRef.current.dpiState;
        if (confirmedDpi) {
          setDpiState((prev) =>
            prev ? { ...prev, active: confirmedDpi.active, count: confirmedDpi.count } : prev,
          );
        }
      }
      finishWriteError('设置档位数', err);
    }
  };

  const handleSetRate = async (newHz: number) => {
    const seq = trackerRef.current.next('rate');
    setRateState((prev) => ({ code: prev?.code ?? null, hz: newHz }));
    beginWrite('正在写入回报率…');
    try {
      const res = await bridgeCommands.setRate(device.key, { hz: newHz });
      confirmedRef.current.rateState = res;
      if (trackerRef.current.isLatest('rate', seq)) {
        setRateState(res);
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest('rate', seq)) {
        setRateState(confirmedRef.current.rateState);
      }
      finishWriteError('设置回报率', err);
    }
  };

  const handleSetSensor = async (
    patch: Partial<{ lod: number; ripple: boolean; fixline: boolean; motion: boolean; mode: number }>,
    label: string,
  ) => {
    const keys = Object.keys(patch) as Array<keyof SensorState>;
    const seqs = keys.map((k) => [k, trackerRef.current.next(`sensor.${k}`)] as const);

    setSensorState((prev) => ({
      lod: patch.lod !== undefined ? patch.lod : (prev?.lod ?? null),
      ripple: patch.ripple !== undefined ? patch.ripple : (prev?.ripple ?? null),
      fixline: patch.fixline !== undefined ? patch.fixline : (prev?.fixline ?? null),
      motion: patch.motion !== undefined ? patch.motion : (prev?.motion ?? null),
      mode: patch.mode !== undefined ? patch.mode : (prev?.mode ?? null),
    }));

    beginWrite(`正在写入${label}…`);
    try {
      const rb = await bridgeCommands.setSensor(device.key, patch);
      const prevConf = confirmedRef.current.sensorState;
      const nextConf: SensorState = {
        lod: rb.lod !== undefined ? rb.lod : (prevConf?.lod ?? null),
        ripple: rb.ripple !== undefined ? rb.ripple : (prevConf?.ripple ?? null),
        fixline: rb.fixline !== undefined ? rb.fixline : (prevConf?.fixline ?? null),
        motion: rb.motion !== undefined ? rb.motion : (prevConf?.motion ?? null),
        mode: rb.mode !== undefined ? rb.mode : (prevConf?.mode ?? null),
      };
      confirmedRef.current.sensorState = nextConf;
      setSensorState((prev) => {
        const next: SensorState = {
          lod: prev?.lod ?? nextConf.lod,
          ripple: prev?.ripple ?? nextConf.ripple,
          fixline: prev?.fixline ?? nextConf.fixline,
          motion: prev?.motion ?? nextConf.motion,
          mode: prev?.mode ?? nextConf.mode,
        };
        for (const [k, s] of seqs) {
          if (trackerRef.current.isLatest(`sensor.${k}`, s) && rb[k] !== undefined) {
            (next as unknown as Record<string, unknown>)[k] = rb[k];
          }
        }
        return next;
      });
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      const conf = confirmedRef.current.sensorState;
      setSensorState((prev) => {
        if (!prev) return conf;
        const next: SensorState = { ...prev };
        for (const [k, s] of seqs) {
          if (trackerRef.current.isLatest(`sensor.${k}`, s)) {
            (next as unknown as Record<string, unknown>)[k] = conf ? conf[k] : null;
          }
        }
        return next;
      });
      finishWriteError(`设置${label}`, err);
    }
  };

  const handleSetMisc = async (
    patch: Partial<{
      debounce: number;
      sleep: number;
      fireLoops: number;
      fireInterval: number;
      scrollReverse: boolean;
    }>,
    label: string,
  ) => {
    const keys = Object.keys(patch) as Array<keyof MiscState>;
    const seqs = keys.map((k) => [k, trackerRef.current.next(`misc.${k}`)] as const);

    setMiscState((prev) => ({
      debounce: patch.debounce !== undefined ? patch.debounce : (prev?.debounce ?? null),
      sleep: patch.sleep !== undefined ? patch.sleep : (prev?.sleep ?? null),
      fireLoops: patch.fireLoops !== undefined ? patch.fireLoops : (prev?.fireLoops ?? null),
      fireInterval:
        patch.fireInterval !== undefined ? patch.fireInterval : (prev?.fireInterval ?? null),
      scrollReverse:
        patch.scrollReverse !== undefined ? patch.scrollReverse : (prev?.scrollReverse ?? null),
    }));

    beginWrite(`正在写入${label}…`);
    try {
      const rb = await bridgeCommands.setMisc(device.key, patch);
      const prevConf = confirmedRef.current.miscState;
      const nextConf: MiscState = {
        debounce: rb.debounce !== undefined ? rb.debounce : (prevConf?.debounce ?? null),
        sleep: rb.sleep !== undefined ? rb.sleep : (prevConf?.sleep ?? null),
        fireLoops: rb.fireLoops !== undefined ? rb.fireLoops : (prevConf?.fireLoops ?? null),
        fireInterval:
          rb.fireInterval !== undefined ? rb.fireInterval : (prevConf?.fireInterval ?? null),
        scrollReverse:
          rb.scrollReverse !== undefined ? rb.scrollReverse : (prevConf?.scrollReverse ?? null),
      };
      confirmedRef.current.miscState = nextConf;
      setMiscState((prev) => {
        const next: MiscState = {
          debounce: prev?.debounce ?? nextConf.debounce,
          sleep: prev?.sleep ?? nextConf.sleep,
          fireLoops: prev?.fireLoops ?? nextConf.fireLoops,
          fireInterval: prev?.fireInterval ?? nextConf.fireInterval,
          scrollReverse: prev?.scrollReverse ?? nextConf.scrollReverse,
        };
        for (const [k, s] of seqs) {
          if (trackerRef.current.isLatest(`misc.${k}`, s) && rb[k] !== undefined) {
            (next as unknown as Record<string, unknown>)[k] = rb[k];
          }
        }
        return next;
      });
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      const conf = confirmedRef.current.miscState;
      setMiscState((prev) => {
        if (!prev) return conf;
        const next: MiscState = { ...prev };
        for (const [k, s] of seqs) {
          if (trackerRef.current.isLatest(`misc.${k}`, s)) {
            (next as unknown as Record<string, unknown>)[k] = conf ? conf[k] : null;
          }
        }
        return next;
      });
      finishWriteError(`设置${label}`, err);
    }
  };

  const handleSetLed = async (mode: number, color?: string) => {
    const seq = trackerRef.current.next('led');
    setLedState((prev) => ({
      supported: prev?.supported ?? true,
      mode,
      color: color ?? prev?.color ?? null,
    }));
    beginWrite('正在写入灯效…');
    try {
      const rb = await bridgeCommands.setLed(mode, device.key, color);
      confirmedRef.current.ledState = rb;
      if (trackerRef.current.isLatest('led', seq)) {
        setLedState(rb);
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest('led', seq)) {
        setLedState(confirmedRef.current.ledState);
      }
      finishWriteError('设置灯效', err);
    }
  };

  const buttonDecodedByIndex = useMemo(() => {
    const out: Record<number, ReturnType<typeof decodeHardwareBinding>> = {};
    for (const row of bindings) {
      out[row.index] = decodeHardwareBinding(row.binding);
    }
    return out;
  }, [bindings]);

  const extraBindings = useMemo(
    () => bindings.filter((b) => !(b.index in BUTTON_ID_BY_INDEX)),
    [bindings],
  );

  const applyButtonBinding = async (targetIdx: number, hwBinding: string, _label: string) => {
    const leftCount = bindings.filter((b) => b.binding === 'Mouse:Left').length;
    const currentRaw = buttonDecodedByIndex[targetIdx]?.raw;
    if (currentRaw === 'Mouse:Left' && hwBinding !== 'Mouse:Left' && leftCount <= 1) {
      pushToast('warn', '左键防误锁：请先将其他按键设为鼠标左键');
      return;
    }

    const fieldKey = `keys.${targetIdx}`;
    const seq = trackerRef.current.next(fieldKey);
    setBindings((prev) =>
      prev.map((b) => (b.index === targetIdx ? { ...b, binding: hwBinding } : b)),
    );

    beginWrite('正在写入按键映射…');
    try {
      const res = await bridgeCommands.setKey(device.key, targetIdx, hwBinding);
      const mergeBindings = (base: KeyBindingItem[]) => {
        if (res.bindings.length === 1 && base.length > 1) {
          const single = res.bindings[0];
          return base.map((b) => (b.index === single.index ? single : b));
        }
        return res.bindings;
      };
      confirmedRef.current.bindings = mergeBindings(confirmedRef.current.bindings);
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        setBindings((prev) => mergeBindings(prev));
      }
      endWrite({ ok: true, message: '已保存' });
    } catch (err) {
      if (trackerRef.current.isLatest(fieldKey, seq)) {
        const confRow = confirmedRef.current.bindings.find((b) => b.index === targetIdx);
        if (confRow) {
          setBindings((prev) => prev.map((b) => (b.index === targetIdx ? confRow : b)));
        }
      }
      finishWriteError('写入按键映射', err);
    }
  };

  const handleResetDefaultKeys = async () => {
    const defaults = ['Mouse:Left', 'Mouse:Right', 'Mouse:Middle', 'Mouse:Forward', 'Mouse:Back', 'Dpi:Cycle'];
    setResetting(true);
    beginWrite('正在恢复默认键位…');
    try {
      let latestBindings = bindings;
      for (let i = 0; i < defaults.length; i++) {
        const res = await bridgeCommands.setKey(device.key, i, defaults[i]);
        if (res.bindings.length >= 6) latestBindings = res.bindings;
      }
      confirmedRef.current.bindings = latestBindings;
      setBindings(latestBindings);
      endWrite({ ok: true, message: '已保存' });
      pushToast('ok', '已恢复默认鼠标键位并回读确认');
    } catch (err) {
      finishWriteError('恢复默认键位', err);
    } finally {
      setResetting(false);
    }
  };

  const handleResetDefaultAdvanced = async () => {
    setResetting(true);
    beginWrite('正在恢复出厂默认参数…');
    try {
      if (!caps || caps.rates.length > 0) {
        const targetHz = !caps || caps.rates.includes(1000) ? 1000 : caps.rates[0];
        await bridgeCommands.setRate(device.key, { hz: targetHz });
      }
      if (!caps || caps.sensor) {
        const sensorPatch: Partial<{
          lod: number;
          ripple: boolean;
          fixline: boolean;
          motion: boolean;
          mode: number;
        }> = { lod: 0 };
        if (!caps || caps.sensor?.modes.length) sensorPatch.mode = 1;
        if (!caps || caps.sensor?.motion) sensorPatch.motion = true;
        if (!caps || caps.sensor?.fixline) sensorPatch.fixline = false;
        if (!caps || caps.sensor?.ripple) sensorPatch.ripple = false;
        await bridgeCommands.setSensor(device.key, sensorPatch);
      }
      if (!caps || caps.misc) {
        const defaultSleep = sleepPresets.includes(300)
          ? 300
          : (sleepPresets[0] ?? 60);
        const miscPatch: Partial<{
          debounce: number;
          sleep: number;
          fireLoops: number;
          fireInterval: number;
          scrollReverse: boolean;
        }> = {
          debounce: Math.max(debounceMin, Math.min(debounceMax, 8)),
          sleep: defaultSleep,
        };
        if (supportsFire) {
          miscPatch.fireLoops = 3;
          miscPatch.fireInterval = 10;
        }
        if (supportsScrollReverse) {
          miscPatch.scrollReverse = false;
        }
        await bridgeCommands.setMisc(device.key, miscPatch);
      }
      endWrite({ ok: true, message: '已保存' });
      await loadAllHardware();
      pushToast('ok', '已恢复默认参数并回读确认');
    } catch (err) {
      finishWriteError('恢复默认参数', err);
    } finally {
      setResetting(false);
    }
  };

  return {
    caps,
    driver,
    dpiMin,
    dpiMax,
    dpiStep,
    maxStages,
    supportsXySplit,
    dpiAnchors,
    rateOptions,
    lodOptions,
    sensorModes,
    sleepPresets,
    isDiscreteSleep,
    sleepMin,
    sleepMax,
    sleepStep,
    debounceMin,
    debounceMax,
    supportsFire,
    supportsScrollReverse,
    supportsReceiverLed,
    supportsMacros,
    mouseLedModes,
    hasAnyLighting,
    loading,
    dpiState,
    stageIdx,
    xySplit,
    setXySplit,
    editAxis,
    setEditAxis,
    activeAxis,
    currentLevel,
    stageXVal,
    stageYVal,
    rateState,
    sensorState,
    miscState,
    ledState,
    bindings,
    buttonDecodedByIndex,
    extraBindings,
    resetting,
    commitDpi,
    handleStageSelect,
    handleStageCountChange,
    handleSetRate,
    handleSetSensor,
    handleSetMisc,
    handleSetLed,
    applyButtonBinding,
    handleResetDefaultKeys,
    handleResetDefaultAdvanced,
  };
}
