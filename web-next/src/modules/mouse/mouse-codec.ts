import type { DriverName } from '../../bridge/types';

export type MouseButtonId = 'left' | 'right' | 'middle' | 'side_fwd' | 'side_back' | 'dpi';
export type MouseCatId = 'mouse' | 'dpi' | 'kb' | 'media' | 'rapid' | 'disable';

export interface MouseButtonMeta {
  id: MouseButtonId;
  index: number;
  name: string;
}

export const MOUSE_BUTTONS: MouseButtonMeta[] = [
  { id: 'left', index: 0, name: '左键' },
  { id: 'right', index: 1, name: '右键' },
  { id: 'middle', index: 2, name: '滚轮中键' },
  { id: 'side_fwd', index: 3, name: '前侧键' },
  { id: 'side_back', index: 4, name: '后侧键' },
  { id: 'dpi', index: 5, name: 'DPI 键' },
];

export const BUTTON_ID_BY_INDEX: Record<number, MouseButtonId> = {
  0: 'left',
  1: 'right',
  2: 'middle',
  3: 'side_fwd',
  4: 'side_back',
  5: 'dpi',
};

export const BUTTON_INDEX_BY_ID: Record<MouseButtonId, number> = {
  left: 0,
  right: 1,
  middle: 2,
  side_fwd: 3,
  side_back: 4,
  dpi: 5,
};

export interface DecodedMouseBinding {
  category: 'mouse' | 'dpi' | 'kb' | 'media' | 'rapid' | 'macro' | 'profile' | 'disable';
  label: string;
  raw: string;
  macroSlot?: number;
}

export interface BindingChoice {
  label: string;
  hwBinding: string;
}

export const MOUSE_BINDING_CHOICES: Record<MouseCatId, BindingChoice[]> = {
  mouse: [
    { label: '鼠标左键', hwBinding: 'Mouse:Left' },
    { label: '鼠标右键', hwBinding: 'Mouse:Right' },
    { label: '滚轮中键', hwBinding: 'Mouse:Middle' },
    { label: '前侧键（前进）', hwBinding: 'Mouse:Forward' },
    { label: '后侧键（后退）', hwBinding: 'Mouse:Back' },
  ],
  dpi: [
    { label: 'DPI 循环切换', hwBinding: 'Dpi:Cycle' },
    { label: 'DPI +', hwBinding: 'Dpi:Up' },
    { label: 'DPI -', hwBinding: 'Dpi:Down' },
  ],
  kb: [
    { label: 'Ctrl + C', hwBinding: 'Ctrl+KeyC' },
    { label: 'Ctrl + V', hwBinding: 'Ctrl+KeyV' },
    { label: 'Ctrl + Z', hwBinding: 'Ctrl+KeyZ' },
    { label: 'Alt + Tab', hwBinding: 'Alt+Tab' },
    { label: 'Win + D', hwBinding: 'Meta+KeyD' },
    { label: 'Space', hwBinding: 'Space' },
    { label: 'Esc', hwBinding: 'Escape' },
  ],
  media: [
    { label: '播放 / 暂停', hwBinding: 'Media:PlayPause' },
    { label: '音量 +', hwBinding: 'Media:VolUp' },
    { label: '音量 -', hwBinding: 'Media:VolDown' },
    { label: '静音', hwBinding: 'Media:Mute' },
    { label: '上一曲', hwBinding: 'Media:Prev' },
    { label: '下一曲', hwBinding: 'Media:Next' },
    { label: '计算器', hwBinding: 'Media:Calculator' },
  ],
  rapid: [{ label: '火力连发', hwBinding: 'Fire' }],
  disable: [{ label: '禁用此键', hwBinding: 'Off' }],
};

export function formatKbDisplay(combo: string): string {
  return combo
    .replace(/\+/g, ' + ')
    .replace(/Key([A-Z])/g, '$1')
    .replace(/Digit([0-9])/g, '$1');
}

export function decodeHardwareBinding(hwStr: string | null | undefined): DecodedMouseBinding {
  if (!hwStr || hwStr === 'Off') {
    return { category: 'disable', label: '已禁用', raw: 'Off' };
  }
  if (hwStr === 'Fire') {
    return { category: 'rapid', label: '火力连发', raw: 'Fire' };
  }
  if (hwStr === 'Profile') {
    return { category: 'profile', label: '配置切换', raw: 'Profile' };
  }
  if (hwStr.startsWith('Mouse:')) {
    const action = hwStr.slice(6);
    const map: Record<string, string> = {
      Left: '鼠标左键',
      Right: '鼠标右键',
      Middle: '滚轮中键',
      Forward: '前侧键（前进）',
      Back: '后侧键（后退）',
    };
    return { category: 'mouse', label: map[action] ?? `鼠标 ${action}`, raw: hwStr };
  }
  if (hwStr.startsWith('Dpi:')) {
    const action = hwStr.slice(4);
    if (action === 'Cycle') return { category: 'dpi', label: 'DPI 循环切换', raw: hwStr };
    if (action === 'Up') return { category: 'dpi', label: 'DPI +', raw: hwStr };
    if (action === 'Down') return { category: 'dpi', label: 'DPI -', raw: hwStr };
    return { category: 'dpi', label: `DPI ${action}`, raw: hwStr };
  }
  if (hwStr.startsWith('Media:')) {
    const action = hwStr.slice(6);
    const mediaMap: Record<string, string> = {
      VolUp: '音量 +',
      VolDown: '音量 -',
      Mute: '静音',
      PlayPause: '播放 / 暂停',
      Prev: '上一曲',
      Next: '下一曲',
      Calculator: '计算器',
      Explorer: '我的电脑',
      Home: '默认主页',
    };
    return { category: 'media', label: mediaMap[action] ?? `多媒体 ${action}`, raw: hwStr };
  }
  if (hwStr.startsWith('Macro:')) {
    const slot = Number.parseInt(hwStr.slice(6), 10);
    return {
      category: 'macro',
      label: Number.isFinite(slot) ? `宏缓冲 #${slot}` : hwStr,
      raw: hwStr,
      macroSlot: Number.isFinite(slot) ? slot : undefined,
    };
  }
  return { category: 'kb', label: formatKbDisplay(hwStr), raw: hwStr };
}

/**
 * Maps capability LOD string ('0.7' | '1.0' | '2.0') to wire integer expected by sensor.set.
 * Incott G23/G24 hardware wire order is misordered: 0=1.0mm, 1=2.0mm, 2=0.7mm.
 * YJX wire order is 0=1.0mm, 1=2.0mm.
 */
export function lodLabelToWire(label: string, _driver: DriverName): number | null {
  if (label === '1.0') return 0;
  if (label === '2.0') return 1;
  if (label === '0.7') return 2;
  return null;
}

export function lodWireToLabel(wire: number | null): string | null {
  if (wire === 0) return '1.0';
  if (wire === 1) return '2.0';
  if (wire === 2) return '0.7';
  return null;
}

export function sensorModeToWire(mode: string): number | null {
  if (mode === 'eco') return 0;
  if (mode === 'standard') return 1;
  if (mode === 'game') return 2;
  return null;
}

export function sensorModeWireToKey(wire: number | null): string | null {
  if (wire === 0) return 'eco';
  if (wire === 1) return 'standard';
  if (wire === 2) return 'game';
  return null;
}

export function sensorModeDisplay(mode: string): string {
  if (mode === 'eco') return '节能';
  if (mode === 'standard') return '标准';
  if (mode === 'game') return '竞技';
  return mode;
}

export function ledModeDisplay(mode: number): string {
  if (mode === 0) return '连接 / 回报率';
  if (mode === 1) return '电量指示';
  if (mode === 2) return '仅低电警告';
  return `模式 ${mode}`;
}
