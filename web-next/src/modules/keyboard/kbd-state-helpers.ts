import type { KbdLedState, KbdState } from '../../bridge/types';

export const DEFAULT_SHALLOW_PRESETS = [60, 120, 300, 600, 1800];
export const DEFAULT_DEEP_PRESETS = [600, 1200, 1680, 1800, 3600];
export const LIGHT_COLORS = ['#38BDF8', '#34D399', '#F43F5E', '#F5B027', '#A78BFA', '#EC4899', '#F8FAFC'];

export const DEFAULT_MAIN_MODES = [
  { code: 1, label: '常亮', supportsColor: true },
  { code: 2, label: '呼吸', supportsColor: true },
  { code: 3, label: '光谱循环', supportsColor: false },
  { code: 4, label: '波浪', supportsColor: true },
];

export function formatMinutesLabel(sec: number): string {
  return `${Math.round(sec / 60)}m`;
}

export function applyKbdPatchOptimistic(
  prev: KbdState | null,
  patch: Record<string, unknown>,
): KbdState | null {
  if (!prev) return prev;
  const next: KbdState = { ...prev };

  if (typeof patch.profile === 'number') {
    next.profile = patch.profile;
  }
  if (typeof patch.debounce === 'number') {
    next.debounce = patch.debounce;
  }

  const optPatch: Partial<NonNullable<KbdState['options']>> = {};
  if (typeof patch.macMode === 'boolean') optPatch.macMode = patch.macMode;
  if (typeof patch.winLock === 'boolean') optPatch.winLock = patch.winLock;
  if (typeof patch.wasdSwap === 'boolean') optPatch.wasdSwap = patch.wasdSwap;
  if (Object.keys(optPatch).length > 0 && prev.options) {
    next.options = { ...prev.options, ...optPatch };
  }

  if (patch.sleep && typeof patch.sleep === 'object' && prev.sleep) {
    next.sleep = { ...prev.sleep, ...(patch.sleep as Partial<NonNullable<KbdState['sleep']>>) };
  }

  if (patch.mainLed && typeof patch.mainLed === 'object' && prev.mainLed) {
    const lp = patch.mainLed as Partial<KbdLedState>;
    next.mainLed = { ...prev.mainLed, ...lp };
    if (typeof lp.on === 'boolean' && next.options) {
      next.options = { ...next.options, backlightOff: !lp.on };
    }
  }

  if (patch.sideLed && typeof patch.sideLed === 'object' && prev.sideLed) {
    const lp = patch.sideLed as Partial<KbdLedState>;
    next.sideLed = { ...prev.sideLed, ...lp };
    if (typeof lp.on === 'boolean' && next.options) {
      next.options = { ...next.options, sidelightOff: !lp.on };
    }
  }

  return next;
}

export function revertKbdField(
  prev: KbdState | null,
  confirmed: KbdState | null,
  fieldKey: string,
): KbdState | null {
  if (!prev || !confirmed) return confirmed;
  const next: KbdState = { ...prev };
  if (fieldKey === 'profile') {
    next.profile = confirmed.profile;
  } else if (fieldKey === 'debounce') {
    next.debounce = confirmed.debounce;
  } else if (fieldKey.startsWith('options.')) {
    const sub = fieldKey.slice('options.'.length) as keyof NonNullable<KbdState['options']>;
    if (prev.options && confirmed.options) {
      next.options = { ...prev.options, [sub]: confirmed.options[sub] };
    }
  } else if (fieldKey.startsWith('sleep.')) {
    const sub = fieldKey.slice('sleep.'.length) as keyof NonNullable<KbdState['sleep']>;
    if (prev.sleep && confirmed.sleep) {
      next.sleep = { ...prev.sleep, [sub]: confirmed.sleep[sub] };
    }
  } else if (fieldKey === 'mainLed') {
    next.mainLed = confirmed.mainLed;
    if (prev.options && confirmed.options) {
      next.options = { ...prev.options, backlightOff: confirmed.options.backlightOff };
    }
  } else if (fieldKey === 'sideLed') {
    next.sideLed = confirmed.sideLed;
    if (prev.options && confirmed.options) {
      next.options = { ...prev.options, sidelightOff: confirmed.options.sidelightOff };
    }
  } else {
    return confirmed;
  }
  return next;
}
