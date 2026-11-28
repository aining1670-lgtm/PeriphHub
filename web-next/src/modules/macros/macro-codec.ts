import type { KbdMacroEvent, StudioMacroItem } from '../../bridge/types';
import { BC98_CODE_TO_USAGE, BC98_USAGE_TO_LABEL } from '../keyboard/bc98-layout';

export type MacroStudioMode = 'mouse' | 'keyboard';

export interface StudioAction {
  id: number;
  type: 'key' | 'mouse' | 'delay';
  code?: string;
  usage?: number;
  action?: 'down' | 'up';
  ms?: number;
}

export interface StudioMacroSlot {
  slot: number;
  name: string;
  empty: boolean;
  loopType: 0 | 1 | 2; // 0: cycles (Studio 3), 1: hold (Studio 1), 2: toggle (Studio 2)
  cycles: number;
  boundButtons: number[];
  targetButton: number | null; // null = save only, 0..5 = upload & bind
  actions: StudioAction[];
}

export const MAX_MOUSE_SLOTS = 10;
export const MAX_MOUSE_ACTIONS = 71;
export const DEFAULT_KBD_SLOTS = 16;
export const MAX_KBD_SLOTS = 50;
export const MAX_KBD_BYTES = 250;
export const MAX_NAME_LEN = 32;

export const MOUSE_BIND_OPTIONS: Array<{ index: number | null; label: string }> = [
  { index: null, label: '仅保存' },
  { index: 3, label: '前侧键' },
  { index: 4, label: '后侧键' },
  { index: 2, label: '滚轮中键' },
  { index: 5, label: 'DPI 键' },
];

export const MOUSE_BUTTON_NAMES: Record<number, string> = {
  0: '左键',
  1: '右键',
  2: '滚轮中键',
  3: '前侧键',
  4: '后侧键',
  5: 'DPI 键',
};

let actionIdSeq = 100;
export function nextActionId(): number {
  return actionIdSeq++;
}

export function isMouseButtonCode(code: string): boolean {
  return code === 'LButton' || code === 'RButton' || code === 'MButton';
}

export function studioLoopToUi(looptype: number): 0 | 1 | 2 {
  if (looptype === 1) return 1;
  if (looptype === 2) return 2;
  return 0;
}

export function uiLoopToStudio(loopType: 0 | 1 | 2): 1 | 2 | 3 {
  if (loopType === 1) return 1;
  if (loopType === 2) return 2;
  return 3;
}

export function studioItemsToActions(items: StudioMacroItem[]): StudioAction[] {
  const actions: StudioAction[] = [];
  for (const it of items) {
    actions.push({
      id: nextActionId(),
      type: isMouseButtonCode(it.key) ? 'mouse' : 'key',
      code: it.key,
      action: it.press ? 'down' : 'up',
    });
    if (it.delay > 0) {
      actions.push({ id: nextActionId(), type: 'delay', ms: it.delay });
    }
  }
  return actions;
}

export function actionsToStudioItems(actions: StudioAction[]): StudioMacroItem[] {
  const items: StudioMacroItem[] = [];
  let leadingDelay = 0;

  for (const act of actions) {
    if (act.type === 'delay') {
      const ms = Math.max(0, Math.min(65535, Math.trunc(act.ms ?? 0)));
      if (items.length === 0) {
        leadingDelay = Math.min(65535, leadingDelay + ms);
      } else {
        const last = items[items.length - 1];
        last.delay = Math.min(65535, last.delay + ms);
      }
      continue;
    }
    if (!act.code || !act.action) continue;
    const item: StudioMacroItem = {
      key: act.code,
      press: act.action === 'down',
      delay: items.length === 0 ? leadingDelay : 0,
    };
    leadingDelay = 0;
    items.push(item);
  }
  return items;
}

export function kbdEventsToActions(events: KbdMacroEvent[]): StudioAction[] {
  const actions: StudioAction[] = [];
  for (const ev of events) {
    if (ev.kind === 'key') {
      actions.push({
        id: nextActionId(),
        type: 'key',
        usage: ev.usage,
        code: BC98_USAGE_TO_LABEL[ev.usage] ?? `HID(${ev.usage})`,
        action: ev.pressed ? 'down' : 'up',
      });
      if (ev.delayMs > 0) {
        actions.push({ id: nextActionId(), type: 'delay', ms: ev.delayMs });
      }
    } else if (ev.kind === 'mouseButton') {
      const code = ev.button === 0 ? 'LButton' : ev.button === 1 ? 'RButton' : 'MButton';
      actions.push({
        id: nextActionId(),
        type: 'mouse',
        usage: ev.button,
        code,
        action: ev.pressed ? 'down' : 'up',
      });
      if (ev.delayMs > 0) {
        actions.push({ id: nextActionId(), type: 'delay', ms: ev.delayMs });
      }
    }
  }
  return actions;
}

export function actionsToKbdEvents(actions: StudioAction[]): { events: KbdMacroEvent[]; error?: string } {
  const events: KbdMacroEvent[] = [];
  let leadingDelay = 0;

  for (const act of actions) {
    if (act.type === 'delay') {
      const ms = Math.max(0, Math.min(65535, Math.trunc(act.ms ?? 0)));
      if (events.length === 0) {
        leadingDelay = Math.min(65535, leadingDelay + ms);
      } else {
        const last = events[events.length - 1];
        last.delayMs = Math.min(65535, last.delayMs + ms);
      }
      continue;
    }

    if (act.type === 'mouse') {
      const button =
        act.code === 'RButton' ? 1 : act.code === 'MButton' ? 2 : (act.usage ?? 0);
      events.push({
        kind: 'mouseButton',
        button,
        pressed: act.action === 'down',
        delayMs: events.length === 0 ? leadingDelay : 0,
      });
      leadingDelay = 0;
      continue;
    }

    const usage =
      act.usage ?? (act.code ? BC98_CODE_TO_USAGE[act.code] : undefined);
    if (usage === undefined || usage < 0x04 || usage > 0xef) {
      return { events: [], error: `不支持的键盘按键代码：${act.code ?? '未知'}` };
    }
    events.push({
      kind: 'key',
      usage,
      pressed: act.action === 'down',
      delayMs: events.length === 0 ? leadingDelay : 0,
    });
    leadingDelay = 0;
  }

  return { events };
}

export function makeEmptySlots(mode: MacroStudioMode, kbdCount = DEFAULT_KBD_SLOTS): StudioMacroSlot[] {
  const count = mode === 'mouse' ? MAX_MOUSE_SLOTS : kbdCount;
  return Array.from({ length: count }, (_, slot) => ({
    slot,
    name: `空槽位 ${String(slot + 1).padStart(2, '0')}`,
    empty: true,
    loopType: 0,
    cycles: 1,
    boundButtons: [],
    targetButton: null,
    actions: [],
  }));
}
