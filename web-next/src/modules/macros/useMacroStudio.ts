import { type ChangeEvent, useCallback, useEffect, useRef, useState } from 'react';
import { bridgeCommands } from '../../bridge/commands';
import { reportBridgeError } from '../../bridge/ui-helpers';
import type { KbdMacroDetail, MacroDefinition, MacroListItem } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import { BC98_CODE_TO_USAGE, computeKbdMacroStreamBytes } from '../keyboard/bc98-layout';
import {
  actionsToKbdEvents,
  actionsToStudioItems,
  DEFAULT_KBD_SLOTS,
  kbdEventsToActions,
  type MacroStudioMode,
  makeEmptySlots,
  MAX_KBD_BYTES,
  MAX_KBD_SLOTS,
  MAX_MOUSE_ACTIONS,
  MAX_NAME_LEN,
  MOUSE_BUTTON_NAMES,
  nextActionId,
  type StudioAction,
  type StudioMacroSlot,
  studioItemsToActions,
  studioLoopToUi,
  uiLoopToStudio,
} from './macro-codec';

export function useMacroStudio(deviceKey: string, mode: MacroStudioMode) {
  const pushToast = useHubStore((s) => s.pushToast);
  const setSyncStatus = useHubStore((s) => s.setSyncStatus);
  const syncState = useHubStore((s) => s.syncState);

  const [slots, setSlots] = useState<StudioMacroSlot[]>(() => makeEmptySlots(mode));
  const [activeSlotIdx, setActiveSlotIdx] = useState(0);
  const [recording, setRecording] = useState(false);
  const [recordDelay, setRecordDelay] = useState(true);
  const [dirtySlots, setDirtySlots] = useState<Record<number, boolean>>({});
  const [loading, setLoading] = useState(true);
  const [kbdSlotCount, setKbdSlotCount] = useState(DEFAULT_KBD_SLOTS);

  const lastEventTimeRef = useRef<number>(0);
  const dirtySlotsRef = useRef(dirtySlots);
  dirtySlotsRef.current = dirtySlots;
  const slotsRef = useRef(slots);
  slotsRef.current = slots;

  const isMouse = mode === 'mouse';
  const isBusy = syncState === 'reading' || syncState === 'writing';

  const handleBridgeError = useCallback(
    (actionLabel: string, err: unknown) => {
      reportBridgeError(setSyncStatus, pushToast, actionLabel, err, '设备浅休眠中，唤醒后重试');
    },
    [pushToast, setSyncStatus],
  );

  const loadSlotDetail = useCallback(
    async (slotNum: number, currentMode: MacroStudioMode, key: string) => {
      try {
        if (currentMode === 'mouse') {
          const res = await bridgeCommands.exportMacro(slotNum);
          const parsed = JSON.parse(res.json) as { macro?: MacroDefinition } & Partial<MacroDefinition>;
          const m = parsed.macro ?? (parsed as MacroDefinition);
          if (!m || !Array.isArray(m.item)) return;
          setSlots((prev) =>
            prev.map((s) =>
              s.slot === slotNum
                ? {
                    ...s,
                    name: m.name || s.name,
                    empty: false,
                    loopType: studioLoopToUi(m.looptype),
                    cycles: m.cycle || 1,
                    actions: studioItemsToActions(m.item),
                  }
                : s,
            ),
          );
          setDirtySlots((prev) => ({ ...prev, [slotNum]: false }));
        } else {
          const detail: KbdMacroDetail = await bridgeCommands.getKbdMacro(key, slotNum);
          setSlots((prev) =>
            prev.map((s) =>
              s.slot === slotNum
                ? {
                    ...s,
                    name: detail.name || s.name,
                    empty: detail.empty,
                    cycles: detail.repeat || 1,
                    actions: kbdEventsToActions(detail.events),
                  }
                : s,
            ),
          );
          setDirtySlots((prev) => ({ ...prev, [slotNum]: false }));
        }
      } catch {
        // Empty slot or unreadable; leave empty state.
      }
    },
    [],
  );

  const loadMacroLibrary = useCallback(
    async (overrideKbdCount?: number) => {
      setLoading(true);
      setRecording(false);
      setDirtySlots({});
      setSyncStatus('reading', '正在读取宏库…');
      const targetKbdCount = overrideKbdCount ?? kbdSlotCount;

      try {
        const base = makeEmptySlots(mode, targetKbdCount);
        if (mode === 'mouse') {
          const items: MacroListItem[] = await bridgeCommands.listMacros();
          for (const item of items) {
            if (item.slot >= 0 && item.slot < base.length) {
              base[item.slot] = {
                ...base[item.slot],
                name: item.name,
                empty: false,
                loopType: studioLoopToUi(item.loop),
                cycles: item.cycles || 1,
                boundButtons: item.bind ?? [],
                targetButton: item.bind?.[0] ?? null,
              };
            }
          }
          setSlots(base);
          const firstDefined = items[0]?.slot ?? 0;
          setActiveSlotIdx(firstDefined);
          if (items.some((x) => x.slot === firstDefined)) {
            await loadSlotDetail(firstDefined, 'mouse', deviceKey);
          }
        } else {
          const res = await bridgeCommands.listKbdMacros(deviceKey, targetKbdCount);
          for (const item of res.slots) {
            if (item.slot >= 0 && item.slot < base.length) {
              base[item.slot] = {
                ...base[item.slot],
                name: item.name,
                empty: item.empty,
                cycles: item.repeat || 1,
              };
            }
          }
          setSlots(base);
          const firstUsed = res.slots.find((s) => !s.empty)?.slot ?? 0;
          setActiveSlotIdx(firstUsed);
          if (res.slots.some((s) => s.slot === firstUsed && !s.empty)) {
            await loadSlotDetail(firstUsed, 'keyboard', deviceKey);
          }
        }
        setSyncStatus('idle', '已同步');
      } catch (err) {
        setSlots(makeEmptySlots(mode, targetKbdCount));
        handleBridgeError('读取宏列表', err);
      } finally {
        setLoading(false);
      }
    },
    [deviceKey, handleBridgeError, kbdSlotCount, loadSlotDetail, mode, setSyncStatus],
  );

  useEffect(() => {
    void loadMacroLibrary();
  }, [loadMacroLibrary]);

  const current = slots[activeSlotIdx] ?? slots[0];
  const mouseItems = isMouse ? actionsToStudioItems(current.actions) : [];
  const kbdEncoded = !isMouse ? actionsToKbdEvents(current.actions) : { events: [] };
  const usedBytes = !isMouse ? computeKbdMacroStreamBytes(kbdEncoded.events) : 0;
  const overLimit = isMouse
    ? mouseItems.length > MAX_MOUSE_ACTIONS
    : usedBytes > MAX_KBD_BYTES;

  const updateCurrent = useCallback(
    (updater: (slot: StudioMacroSlot) => StudioMacroSlot) => {
      setSlots((prev) => prev.map((s, idx) => (idx === activeSlotIdx ? updater(s) : s)));
      setDirtySlots((prev) => ({ ...prev, [activeSlotIdx]: true }));
    },
    [activeSlotIdx],
  );

  useEffect(() => {
    if (!recording) return;
    lastEventTimeRef.current = performance.now();

    const handleKey = (action: 'down' | 'up') => (e: KeyboardEvent) => {
      if (e.repeat) return;
      e.preventDefault();
      const code = e.code;
      if (!code) return;

      const usage = BC98_CODE_TO_USAGE[code];
      if (!isMouse && (usage === undefined || usage < 0x04 || usage > 0xef)) {
        pushToast('warn', `暂不支持录制键位 ${code}`);
        return;
      }

      const now = performance.now();
      const delta = Math.min(65535, Math.max(1, Math.round(now - lastEventTimeRef.current)));
      lastEventTimeRef.current = now;

      const cur = slotsRef.current[activeSlotIdx];
      if (!cur) return;

      const nextActions = [...cur.actions];
      if (recordDelay && nextActions.length > 0) {
        nextActions.push({ id: nextActionId(), type: 'delay', ms: delta });
      }
      nextActions.push({ id: nextActionId(), type: 'key', code, usage, action });

      if (isMouse && actionsToStudioItems(nextActions).length > MAX_MOUSE_ACTIONS) {
        pushToast('warn', `已达到鼠标宏上限（${MAX_MOUSE_ACTIONS} 步），停止录制`);
        setRecording(false);
        return;
      }
      if (!isMouse) {
        const nextKbd = actionsToKbdEvents(nextActions);
        if (computeKbdMacroStreamBytes(nextKbd.events) > MAX_KBD_BYTES) {
          pushToast('warn', `已达到板载宏字节上限（${MAX_KBD_BYTES} B），停止录制`);
          setRecording(false);
          return;
        }
      }

      setSlots((prev) =>
        prev.map((s, idx) => (idx === activeSlotIdx ? { ...s, empty: false, actions: nextActions } : s)),
      );
      setDirtySlots((prev) => ({ ...prev, [cur.slot]: true }));
    };

    const onDown = handleKey('down');
    const onUp = handleKey('up');
    window.addEventListener('keydown', onDown, true);
    window.addEventListener('keyup', onUp, true);
    return () => {
      window.removeEventListener('keydown', onDown, true);
      window.removeEventListener('keyup', onUp, true);
    };
  }, [recording, recordDelay, activeSlotIdx, isMouse, pushToast]);

  const handleSelectSlot = async (idx: number) => {
    if (recording) setRecording(false);
    setActiveSlotIdx(idx);
    const target = slots[idx];
    if (target && !target.empty && target.actions.length === 0 && !dirtySlots[target.slot]) {
      await loadSlotDetail(target.slot, mode, deviceKey);
    }
  };

  const handleInsertMouseClick = (btnCode: 'LButton' | 'RButton' | 'MButton') => {
    const addition: StudioAction[] = [
      { id: nextActionId(), type: 'mouse', code: btnCode, action: 'down' },
      { id: nextActionId(), type: 'delay', ms: 25 },
      { id: nextActionId(), type: 'mouse', code: btnCode, action: 'up' },
    ];
    const nextActions = [...current.actions, ...addition];
    if (isMouse && actionsToStudioItems(nextActions).length > MAX_MOUSE_ACTIONS) {
      pushToast('warn', `超出动作上限（≤ ${MAX_MOUSE_ACTIONS} 步）`);
      return;
    }
    if (!isMouse) {
      const nextKbd = actionsToKbdEvents(nextActions);
      if (computeKbdMacroStreamBytes(nextKbd.events) > MAX_KBD_BYTES) {
        pushToast('warn', `超出字节上限（≤ ${MAX_KBD_BYTES} B）`);
        return;
      }
    }
    updateCurrent((s) => ({ ...s, empty: false, actions: nextActions }));
  };

  const handleInsertDelay = () => {
    const nextActions: StudioAction[] = [
      ...current.actions,
      { id: nextActionId(), type: 'delay', ms: 30 },
    ];
    if (isMouse && actionsToStudioItems(nextActions).length > MAX_MOUSE_ACTIONS) {
      pushToast('warn', `超出动作上限（≤ ${MAX_MOUSE_ACTIONS} 步）`);
      return;
    }
    if (!isMouse) {
      const nextKbd = actionsToKbdEvents(nextActions);
      if (computeKbdMacroStreamBytes(nextKbd.events) > MAX_KBD_BYTES) {
        pushToast('warn', `超出字节上限（≤ ${MAX_KBD_BYTES} B）`);
        return;
      }
    }
    updateCurrent((s) => ({ ...s, actions: nextActions }));
  };

  const handleMoveAction = (index: number, dir: -1 | 1) => {
    const target = index + dir;
    if (target < 0 || target >= current.actions.length) return;
    updateCurrent((s) => {
      const next = [...s.actions];
      const [item] = next.splice(index, 1);
      next.splice(target, 0, item);
      return { ...s, actions: next };
    });
  };

  const handleDeleteAction = (id: number) => {
    updateCurrent((s) => ({
      ...s,
      actions: s.actions.filter((a) => a.id !== id),
    }));
  };

  const handleCommitHardware = async () => {
    if (recording) setRecording(false);
    const trimmedName = current.name.trim();
    if (!trimmedName || trimmedName.length > MAX_NAME_LEN) {
      pushToast('error', `宏名称长度需在 1–${MAX_NAME_LEN} 字符之间`);
      return;
    }
    if (current.actions.length === 0) {
      pushToast('warn', '当前宏为空，请先录制或添加动作');
      return;
    }
    if (overLimit) {
      pushToast(
        'error',
        isMouse
          ? `动作数超过 ${MAX_MOUSE_ACTIONS} 步上限，拒绝写入`
          : `字节流超过 ${MAX_KBD_BYTES} B 上限，拒绝写入`,
      );
      return;
    }
    if (current.cycles < 1 || current.cycles > 65535) {
      pushToast('error', '循环次数需在 1–65535 之间');
      return;
    }

    if (isMouse) {
      const items = actionsToStudioItems(current.actions);
      if (items.length === 0) {
        pushToast('warn', '缺少有效的按键或鼠标动作');
        return;
      }
      const macroPayload: MacroDefinition = {
        name: trimmedName,
        looptype: uiLoopToStudio(current.loopType),
        cycle: current.cycles,
        item: items,
      };
      setSyncStatus('writing', '正在保存鼠标宏…');
      try {
        await bridgeCommands.saveMacro(current.slot, macroPayload);
        if (current.targetButton !== null) {
          setSyncStatus('writing', '正在上传宏并绑定按键…');
          const uploadRes = await bridgeCommands.uploadMacro(
            current.slot,
            current.targetButton,
            deviceKey,
          );
          if (uploadRes.warning) {
            pushToast('warn', `宏已写入按键，但本地配置保存提示：${uploadRes.warning}`);
          } else {
            const btnLabel = MOUSE_BUTTON_NAMES[current.targetButton] ?? `按键 ${current.targetButton}`;
            pushToast('ok', `宏「${trimmedName}」已上传并绑定至${btnLabel}`);
          }
        } else {
          pushToast('ok', `宏「${trimmedName}」已保存至本地宏库 M${String(current.slot + 1).padStart(2, '0')}`);
        }
        const savedSlot = current.slot;
        const freshList: MacroListItem[] = await bridgeCommands.listMacros();
        const freshBySlot = new Map(freshList.map((item) => [item.slot, item]));
        setSlots((prev) =>
          prev.map((s) => {
            const fresh = freshBySlot.get(s.slot);
            if (s.slot === savedSlot) {
              return {
                ...s,
                name: trimmedName,
                empty: false,
                boundButtons: fresh?.bind ?? [],
                targetButton: fresh?.bind?.[0] ?? null,
              };
            }
            if (dirtySlotsRef.current[s.slot]) {
              return fresh ? { ...s, boundButtons: fresh.bind ?? [] } : s;
            }
            if (!fresh) {
              return {
                ...s,
                name: `空槽位 ${String(s.slot + 1).padStart(2, '0')}`,
                empty: true,
                loopType: 0,
                cycles: 1,
                boundButtons: [],
                targetButton: null,
                actions: [],
              };
            }
            return {
              ...s,
              name: fresh.name,
              empty: false,
              loopType: studioLoopToUi(fresh.loop),
              cycles: fresh.cycles || 1,
              boundButtons: fresh.bind ?? [],
              targetButton: fresh.bind?.[0] ?? null,
            };
          }),
        );
        setDirtySlots((prev) => ({ ...prev, [savedSlot]: false }));
        setSyncStatus('idle', '已保存');
      } catch (err) {
        handleBridgeError('写入鼠标宏', err);
      }
      return;
    }

    // Keyboard onboard macro
    if (kbdEncoded.error) {
      pushToast('error', kbdEncoded.error);
      return;
    }
    setSyncStatus('writing', '正在写入键盘板载宏并回读校验…');
    try {
      const res = await bridgeCommands.setKbdMacro(deviceKey, {
        slot: current.slot,
        name: trimmedName,
        repeat: current.cycles,
        events: kbdEncoded.events,
      });
      setSlots((prev) =>
        prev.map((s) =>
          s.slot === current.slot
            ? {
                ...s,
                name: res.name,
                empty: res.empty,
                cycles: res.repeat,
                actions: kbdEventsToActions(res.events),
              }
            : s,
        ),
      );
      setDirtySlots((prev) => ({ ...prev, [current.slot]: false }));
      setSyncStatus('idle', '已保存');
      pushToast(
        'ok',
        `板载宏 M${String(current.slot + 1).padStart(2, '0')}「${res.name}」已写入并回读一致（${res.byteLength} B）`,
      );
    } catch (err) {
      handleBridgeError('写入键盘板载宏', err);
    }
  };

  const handleDeleteSlot = async () => {
    if (current.empty) return;
    setSyncStatus('writing', '正在删除宏槽位…');
    try {
      if (isMouse) {
        await bridgeCommands.deleteMacro(current.slot);
      } else {
        await bridgeCommands.deleteKbdMacro(deviceKey, current.slot);
      }
      setSlots((prev) =>
        prev.map((s) =>
          s.slot === current.slot
            ? {
                ...s,
                name: `空槽位 ${String(s.slot + 1).padStart(2, '0')}`,
                empty: true,
                boundButtons: [],
                targetButton: null,
                actions: [],
              }
            : s,
        ),
      );
      setDirtySlots((prev) => ({ ...prev, [current.slot]: false }));
      setSyncStatus('idle', '已保存');
      pushToast('ok', `已清空槽位 M${String(current.slot + 1).padStart(2, '0')}`);
    } catch (err) {
      handleBridgeError('删除宏槽位', err);
    }
  };

  const handleExportJson = async () => {
    try {
      let jsonStr: string;
      if (isMouse && !current.empty && !dirtySlots[current.slot]) {
        const res = await bridgeCommands.exportMacro(current.slot);
        jsonStr = res.json;
      } else {
        const payload: MacroDefinition = {
          name: current.name,
          looptype: uiLoopToStudio(current.loopType),
          cycle: current.cycles,
          item: actionsToStudioItems(current.actions),
        };
        jsonStr = JSON.stringify(
          { format: 'incott-studio-macro', version: 1, macro: payload },
          null,
          2,
        );
      }
      let copied = false;
      try {
        if (navigator.clipboard?.writeText) {
          await navigator.clipboard.writeText(jsonStr);
          copied = true;
        }
      } catch {}
      const blob = new Blob([jsonStr], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `${current.name || 'macro'}.json`;
      a.click();
      URL.revokeObjectURL(url);
      pushToast(
        'ok',
        copied ? `已导出并复制「${current.name}」JSON` : `已导出「${current.name}」JSON`,
      );
    } catch (err) {
      handleBridgeError('导出宏', err);
    }
  };

  const handleImportFile = async (e: ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = '';
    if (!file) return;
    if (file.size > 128 * 1024) {
      pushToast('error', '宏文件超出 128 KB 上限');
      return;
    }
    try {
      const text = await file.text();
      setSyncStatus('writing', '正在导入宏文件…');
      const res = await bridgeCommands.importMacro(text, current.slot);
      pushToast('ok', `已导入至槽位 M${String(res.slot + 1).padStart(2, '0')}`);
      await loadMacroLibrary();
      await loadSlotDetail(res.slot, 'mouse', deviceKey);
      setActiveSlotIdx(res.slot);
    } catch (err) {
      handleBridgeError('导入宏', err);
    }
  };

  const handleExpandKbdSlots = () => {
    setKbdSlotCount(MAX_KBD_SLOTS);
    void loadMacroLibrary(MAX_KBD_SLOTS);
  };

  return {
    slots,
    activeSlotIdx,
    current,
    dirtySlots,
    loading,
    isMouse,
    isBusy,
    recording,
    setRecording,
    recordDelay,
    setRecordDelay,
    kbdSlotCount,
    mouseItems,
    usedBytes,
    overLimit,
    updateCurrent,
    handleSelectSlot,
    handleInsertMouseClick,
    handleInsertDelay,
    handleMoveAction,
    handleDeleteAction,
    handleCommitHardware,
    handleDeleteSlot,
    handleExportJson,
    handleImportFile,
    handleExpandKbdSlots,
  };
}
