import type { KeyboardKeyDef, KbdKeyEntry } from '../../bridge/types';

export type KeyTone = 'alpha' | 'mod' | 'accent' | 'knob';
export type KbCatId = 'basic' | 'alpha' | 'numpad' | 'combo' | 'media' | 'special';

export interface KeySpec {
  id: string;
  slot: number;
  code: string;
  label: string;
  sub: string;
  x: number;
  y: number;
  w: number;
  h: number;
  tone: KeyTone;
  base: KbdKeyEntry;
  fn: KbdKeyEntry;
}

export interface PickerItem {
  label: string;
  entry: KbdKeyEntry;
}

export interface PickerCategory {
  name: string;
  items: PickerItem[];
}

// BC98 hardware 98-key matrix (slot 0..105) matching native/src/royuan_protocol.cpp
// Coordinates in unit (U) steps derived directly from BC98_LAYOUT in web/index.html.
export const BC98_KEYS: KeySpec[] = [
  // Row 0 (y=0)
  { id: 'Esc', slot: 0, code: 'Escape', label: 'Esc', sub: '长按复位', x: 0, y: 0, w: 1, h: 1, tone: 'accent', base: [0, 0, 41, 0], fn: [10, 2, 0, 0] },
  { id: 'F1', slot: 6, code: 'F1', label: 'F1', sub: '亮度-', x: 1.5, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 58, 0], fn: [3, 0, 112, 0] },
  { id: 'F2', slot: 12, code: 'F2', label: 'F2', sub: '亮度+', x: 2.5, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 59, 0], fn: [3, 0, 111, 0] },
  { id: 'F3', slot: 18, code: 'F3', label: 'F3', sub: '邮件', x: 3.5, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 60, 0], fn: [3, 0, 138, 1] },
  { id: 'F4', slot: 24, code: 'F4', label: 'F4', sub: '计算器', x: 4.5, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 61, 0], fn: [3, 0, 146, 1] },
  { id: 'F5', slot: 30, code: 'F5', label: 'F5', sub: '电脑', x: 5.75, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 62, 0], fn: [3, 0, 148, 1] },
  { id: 'F6', slot: 36, code: 'F6', label: 'F6', sub: '媒体', x: 6.75, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 63, 0], fn: [3, 0, 131, 1] },
  { id: 'F7', slot: 42, code: 'F7', label: 'F7', sub: '上一曲', x: 7.75, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 64, 0], fn: [3, 0, 182, 0] },
  { id: 'F8', slot: 48, code: 'F8', label: 'F8', sub: '播放', x: 8.75, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 65, 0], fn: [3, 0, 205, 0] },
  { id: 'F9', slot: 54, code: 'F9', label: 'F9', sub: '下一曲', x: 10, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 66, 0], fn: [3, 0, 181, 0] },
  { id: 'F10', slot: 60, code: 'F10', label: 'F10', sub: '静音', x: 11, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 67, 0], fn: [3, 0, 226, 0] },
  { id: 'F11', slot: 66, code: 'F11', label: 'F11', sub: '音量-', x: 12, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 68, 0], fn: [3, 0, 234, 0] },
  { id: 'F12', slot: 72, code: 'F12', label: 'F12', sub: '音量+', x: 13, y: 0, w: 1, h: 1, tone: 'alpha', base: [0, 0, 69, 0], fn: [3, 0, 233, 0] },
  { id: 'Del', slot: 84, code: 'Delete', label: 'Del', sub: '灯效', x: 14.5, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 76, 0], fn: [13, 1, 0, 0] },
  { id: 'PrtSc', slot: 90, code: 'PrintScreen', label: 'PrtSc', sub: '色彩', x: 15.5, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 70, 0], fn: [13, 5, 1, 0] },
  { id: 'PgUp', slot: 96, code: 'PageUp', label: 'PgUp', sub: '省电', x: 16.5, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 75, 0], fn: [10, 12, 0, 0] },
  { id: 'PgDn', slot: 102, code: 'PageDown', label: 'PgDn', sub: '', x: 17.5, y: 0, w: 1, h: 1, tone: 'mod', base: [0, 0, 78, 0], fn: [0, 0, 0, 0] },

  // Row 1 (y=1.25)
  { id: '`', slot: 1, code: 'Backquote', label: '~', sub: '`', x: 0, y: 1.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 53, 0], fn: [0, 0, 0, 0] },
  { id: '1', slot: 7, code: 'Digit1', label: '1', sub: 'BT1', x: 1, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 30, 0], fn: [14, 0, 0, 0] },
  { id: '2', slot: 13, code: 'Digit2', label: '2', sub: 'BT2', x: 2, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 31, 0], fn: [14, 0, 1, 0] },
  { id: '3', slot: 19, code: 'Digit3', label: '3', sub: 'BT3', x: 3, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 32, 0], fn: [14, 0, 2, 0] },
  { id: '4', slot: 25, code: 'Digit4', label: '4', sub: '2.4G', x: 4, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 33, 0], fn: [14, 0, 5, 0] },
  { id: '5', slot: 31, code: 'Digit5', label: '5', sub: '%', x: 5, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 34, 0], fn: [0, 0, 0, 0] },
  { id: '6', slot: 37, code: 'Digit6', label: '6', sub: '^', x: 6, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 35, 0], fn: [0, 0, 0, 0] },
  { id: '7', slot: 43, code: 'Digit7', label: '7', sub: '&', x: 7, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 36, 0], fn: [0, 0, 0, 0] },
  { id: '8', slot: 49, code: 'Digit8', label: '8', sub: '*', x: 8, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 37, 0], fn: [0, 0, 0, 0] },
  { id: '9', slot: 55, code: 'Digit9', label: '9', sub: '(', x: 9, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 38, 0], fn: [0, 0, 0, 0] },
  { id: '0', slot: 61, code: 'Digit0', label: '0', sub: ')', x: 10, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 39, 0], fn: [0, 0, 0, 0] },
  { id: '-', slot: 67, code: 'Minus', label: '-', sub: '_', x: 11, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 45, 0], fn: [0, 0, 0, 0] },
  { id: '=', slot: 73, code: 'Equal', label: '=', sub: '+', x: 12, y: 1.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 46, 0], fn: [0, 0, 0, 0] },
  { id: 'Bksp', slot: 79, code: 'Backspace', label: '⌫', sub: '电量', x: 13, y: 1.25, w: 2, h: 1, tone: 'mod', base: [0, 0, 42, 0], fn: [10, 8, 0, 0] },
  { id: 'Num', slot: 85, code: 'NumLock', label: 'Num', sub: '', x: 15.25, y: 1.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 83, 0], fn: [0, 0, 0, 0] },
  { id: 'N/', slot: 91, code: 'NumpadDivide', label: '/', sub: '', x: 16.25, y: 1.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 84, 0], fn: [0, 0, 0, 0] },
  { id: 'N*', slot: 97, code: 'NumpadMultiply', label: '*', sub: '', x: 17.25, y: 1.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 85, 0], fn: [0, 0, 0, 0] },
  { id: 'N-', slot: 103, code: 'NumpadSubtract', label: '-', sub: '', x: 18.25, y: 1.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 86, 0], fn: [0, 0, 0, 0] },

  // Row 2 (y=2.25)
  { id: 'Tab', slot: 2, code: 'Tab', label: 'Tab', sub: '', x: 0, y: 2.25, w: 1.5, h: 1, tone: 'mod', base: [0, 0, 43, 0], fn: [0, 0, 0, 0] },
  { id: 'Q', slot: 8, code: 'KeyQ', label: 'Q', sub: '', x: 1.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 20, 0], fn: [0, 0, 0, 0] },
  { id: 'W', slot: 14, code: 'KeyW', label: 'W', sub: '', x: 2.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 26, 0], fn: [0, 0, 0, 0] },
  { id: 'E', slot: 20, code: 'KeyE', label: 'E', sub: '', x: 3.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 8, 0], fn: [0, 0, 0, 0] },
  { id: 'R', slot: 26, code: 'KeyR', label: 'R', sub: '', x: 4.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 21, 0], fn: [0, 0, 0, 0] },
  { id: 'T', slot: 32, code: 'KeyT', label: 'T', sub: '', x: 5.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 23, 0], fn: [0, 0, 0, 0] },
  { id: 'Y', slot: 38, code: 'KeyY', label: 'Y', sub: '', x: 6.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 28, 0], fn: [0, 0, 0, 0] },
  { id: 'U', slot: 44, code: 'KeyU', label: 'U', sub: '', x: 7.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 24, 0], fn: [0, 0, 0, 0] },
  { id: 'I', slot: 50, code: 'KeyI', label: 'I', sub: '', x: 8.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 12, 0], fn: [0, 0, 0, 0] },
  { id: 'O', slot: 56, code: 'KeyO', label: 'O', sub: 'ScrLk', x: 9.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 18, 0], fn: [0, 0, 71, 0] },
  { id: 'P', slot: 62, code: 'KeyP', label: 'P', sub: 'Pause', x: 10.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 19, 0], fn: [0, 0, 72, 0] },
  { id: '[', slot: 68, code: 'BracketLeft', label: '[', sub: '{', x: 11.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 47, 0], fn: [0, 0, 0, 0] },
  { id: ']', slot: 74, code: 'BracketRight', label: ']', sub: '}', x: 12.5, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 48, 0], fn: [0, 0, 0, 0] },
  { id: '\\', slot: 80, code: 'Backslash', label: '\\', sub: '|', x: 13.5, y: 2.25, w: 1.5, h: 1, tone: 'mod', base: [0, 0, 49, 0], fn: [0, 0, 0, 0] },
  { id: 'N7', slot: 86, code: 'Numpad7', label: '7', sub: 'Home', x: 15.25, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 95, 0], fn: [0, 0, 0, 0] },
  { id: 'N8', slot: 92, code: 'Numpad8', label: '8', sub: '↑', x: 16.25, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 96, 0], fn: [0, 0, 0, 0] },
  { id: 'N9', slot: 98, code: 'Numpad9', label: '9', sub: 'PgUp', x: 17.25, y: 2.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 97, 0], fn: [0, 0, 0, 0] },
  { id: 'N+', slot: 104, code: 'NumpadAdd', label: '+', sub: '', x: 18.25, y: 2.25, w: 1, h: 2, tone: 'mod', base: [0, 0, 87, 0], fn: [0, 0, 0, 0] },

  // Row 3 (y=3.25)
  { id: 'Caps', slot: 3, code: 'CapsLock', label: 'Caps', sub: '', x: 0, y: 3.25, w: 1.75, h: 1, tone: 'mod', base: [0, 0, 57, 0], fn: [0, 0, 0, 0] },
  { id: 'A', slot: 9, code: 'KeyA', label: 'A', sub: '', x: 1.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 4, 0], fn: [0, 0, 0, 0] },
  { id: 'S', slot: 15, code: 'KeyS', label: 'S', sub: '', x: 2.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 22, 0], fn: [0, 0, 0, 0] },
  { id: 'D', slot: 21, code: 'KeyD', label: 'D', sub: '', x: 3.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 7, 0], fn: [0, 0, 0, 0] },
  { id: 'F', slot: 27, code: 'KeyF', label: 'F', sub: '', x: 4.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 9, 0], fn: [0, 0, 0, 0] },
  { id: 'G', slot: 33, code: 'KeyG', label: 'G', sub: '', x: 5.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 10, 0], fn: [0, 0, 0, 0] },
  { id: 'H', slot: 39, code: 'KeyH', label: 'H', sub: '', x: 6.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 11, 0], fn: [0, 0, 0, 0] },
  { id: 'J', slot: 45, code: 'KeyJ', label: 'J', sub: '', x: 7.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 13, 0], fn: [0, 0, 0, 0] },
  { id: 'K', slot: 51, code: 'KeyK', label: 'K', sub: '', x: 8.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 14, 0], fn: [0, 0, 0, 0] },
  { id: 'L', slot: 57, code: 'KeyL', label: 'L', sub: '', x: 9.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 15, 0], fn: [0, 0, 0, 0] },
  { id: ';', slot: 63, code: 'Semicolon', label: ';', sub: ':', x: 10.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 51, 0], fn: [0, 0, 0, 0] },
  { id: "'", slot: 69, code: 'Quote', label: "'", sub: '"', x: 11.75, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 52, 0], fn: [0, 0, 0, 0] },
  { id: 'Enter', slot: 81, code: 'Enter', label: 'Enter', sub: '', x: 12.75, y: 3.25, w: 2.25, h: 1, tone: 'accent', base: [0, 0, 40, 0], fn: [0, 0, 0, 0] },
  { id: 'N4', slot: 87, code: 'Numpad4', label: '4', sub: '←', x: 15.25, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 92, 0], fn: [0, 0, 0, 0] },
  { id: 'N5', slot: 93, code: 'Numpad5', label: '5', sub: '', x: 16.25, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 93, 0], fn: [0, 0, 0, 0] },
  { id: 'N6', slot: 99, code: 'Numpad6', label: '6', sub: '→', x: 17.25, y: 3.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 94, 0], fn: [0, 0, 0, 0] },

  // Row 4 (y=4.25)
  { id: 'LShift', slot: 4, code: 'ShiftLeft', label: 'Shift', sub: '', x: 0, y: 4.25, w: 2.25, h: 1, tone: 'mod', base: [0, 0, 225, 0], fn: [0, 0, 0, 0] },
  { id: 'Z', slot: 16, code: 'KeyZ', label: 'Z', sub: '', x: 2.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 29, 0], fn: [0, 0, 0, 0] },
  { id: 'X', slot: 22, code: 'KeyX', label: 'X', sub: '', x: 3.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 27, 0], fn: [0, 0, 0, 0] },
  { id: 'C', slot: 28, code: 'KeyC', label: 'C', sub: '', x: 4.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 6, 0], fn: [0, 0, 0, 0] },
  { id: 'V', slot: 34, code: 'KeyV', label: 'V', sub: '', x: 5.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 25, 0], fn: [0, 0, 0, 0] },
  { id: 'B', slot: 40, code: 'KeyB', label: 'B', sub: '', x: 6.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 5, 0], fn: [0, 0, 0, 0] },
  { id: 'N', slot: 46, code: 'KeyN', label: 'N', sub: '', x: 7.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 17, 0], fn: [0, 0, 0, 0] },
  { id: 'M', slot: 52, code: 'KeyM', label: 'M', sub: '', x: 8.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 16, 0], fn: [0, 0, 0, 0] },
  { id: ',', slot: 58, code: 'Comma', label: ',', sub: '<', x: 9.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 54, 0], fn: [0, 0, 0, 0] },
  { id: '.', slot: 64, code: 'Period', label: '.', sub: '>', x: 10.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 55, 0], fn: [0, 0, 0, 0] },
  { id: '/', slot: 70, code: 'Slash', label: '/', sub: '?', x: 11.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 56, 0], fn: [0, 0, 0, 0] },
  { id: 'RShift', slot: 76, code: 'ShiftRight', label: 'Shift', sub: '', x: 12.25, y: 4.25, w: 1.75, h: 1, tone: 'mod', base: [0, 0, 229, 0], fn: [0, 0, 0, 0] },
  { id: 'Up', slot: 82, code: 'ArrowUp', label: '↑', sub: '亮+', x: 14.125, y: 4.35, w: 1, h: 1, tone: 'mod', base: [0, 0, 82, 0], fn: [13, 2, 1, 0] },
  { id: 'N1', slot: 88, code: 'Numpad1', label: '1', sub: 'End', x: 15.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 89, 0], fn: [0, 0, 0, 0] },
  { id: 'N2', slot: 94, code: 'Numpad2', label: '2', sub: '↓', x: 16.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 90, 0], fn: [0, 0, 0, 0] },
  { id: 'N3', slot: 100, code: 'Numpad3', label: '3', sub: 'PgDn', x: 17.25, y: 4.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 91, 0], fn: [0, 0, 0, 0] },
  { id: 'NEnter', slot: 105, code: 'NumpadEnter', label: 'Ent', sub: '', x: 18.25, y: 4.25, w: 1, h: 2, tone: 'accent', base: [0, 0, 88, 0], fn: [0, 0, 0, 0] },

  // Row 5 (y=5.25)
  { id: 'LCtrl', slot: 5, code: 'ControlLeft', label: 'Ctrl', sub: '', x: 0, y: 5.25, w: 1.25, h: 1, tone: 'mod', base: [0, 0, 224, 0], fn: [0, 0, 0, 0] },
  { id: 'LWin', slot: 17, code: 'MetaLeft', label: 'Win', sub: '锁Win', x: 1.25, y: 5.25, w: 1.25, h: 1, tone: 'mod', base: [0, 0, 227, 0], fn: [10, 3, 0, 0] },
  { id: 'LAlt', slot: 23, code: 'AltLeft', label: 'Alt', sub: '', x: 2.5, y: 5.25, w: 1.25, h: 1, tone: 'mod', base: [0, 0, 226, 0], fn: [0, 0, 0, 0] },
  { id: 'Space', slot: 41, code: 'Space', label: '', sub: '', x: 3.75, y: 5.25, w: 6.25, h: 1, tone: 'alpha', base: [0, 0, 44, 0], fn: [0, 0, 0, 0] },
  { id: 'RAlt', slot: 59, code: 'AltRight', label: 'Alt', sub: '', x: 10, y: 5.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 230, 0], fn: [0, 0, 0, 0] },
  { id: 'Fn', slot: 65, code: 'Fn', label: 'Fn', sub: '', x: 11, y: 5.25, w: 1, h: 1, tone: 'mod', base: [10, 1, 0, 0], fn: [0, 0, 0, 0] },
  { id: 'RCtrl', slot: 71, code: 'ControlRight', label: 'Ctrl', sub: '', x: 12, y: 5.25, w: 1, h: 1, tone: 'mod', base: [0, 0, 228, 0], fn: [0, 0, 0, 0] },
  { id: 'Left', slot: 77, code: 'ArrowLeft', label: '←', sub: '速-', x: 13.125, y: 5.35, w: 1, h: 1, tone: 'mod', base: [0, 0, 80, 0], fn: [13, 3, 1, 0] },
  { id: 'Down', slot: 83, code: 'ArrowDown', label: '↓', sub: '亮-', x: 14.125, y: 5.35, w: 1, h: 1, tone: 'mod', base: [0, 0, 81, 0], fn: [13, 2, 2, 0] },
  { id: 'Right', slot: 89, code: 'ArrowRight', label: '→', sub: '速+', x: 15.125, y: 5.35, w: 1, h: 1, tone: 'mod', base: [0, 0, 79, 0], fn: [13, 3, 2, 0] },
  { id: 'N0', slot: 95, code: 'Numpad0', label: '0', sub: 'Ins', x: 16.25, y: 5.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 98, 0], fn: [0, 0, 0, 0] },
  { id: 'N.', slot: 101, code: 'NumpadDecimal', label: '.', sub: 'Del', x: 17.25, y: 5.25, w: 1, h: 1, tone: 'alpha', base: [0, 0, 99, 0], fn: [0, 0, 0, 0] },
];

// Note: Numpad0 on BC98 physical layout is 1u at x=16.25 or 2u depending on bezel; wait:
// In web/index.html line 5754: Numpad0 x=773,w=40 (1u), NumpadDecimal x=819,w=40 (1u) — arrows shift left.
// Keeping exact BC98_KEYS physical coordinates above (x=13.125..15.125 arrows, 16.25 N0, 17.25 N.).

export const BC98_CODE_TO_USAGE: Record<string, number> = (() => {
  const m: Record<string, number> = {
    Enter: 40,
    Escape: 41,
    Backspace: 42,
    Tab: 43,
    Space: 44,
    Minus: 45,
    Equal: 46,
    BracketLeft: 47,
    BracketRight: 48,
    Backslash: 49,
    Semicolon: 51,
    Quote: 52,
    Backquote: 53,
    Comma: 54,
    Period: 55,
    Slash: 56,
    CapsLock: 57,
    PrintScreen: 70,
    ScrollLock: 71,
    Pause: 72,
    Insert: 73,
    Home: 74,
    PageUp: 75,
    Delete: 76,
    End: 77,
    PageDown: 78,
    ArrowRight: 79,
    ArrowLeft: 80,
    ArrowDown: 81,
    ArrowUp: 82,
    NumLock: 83,
    NumpadDivide: 84,
    NumpadMultiply: 85,
    NumpadSubtract: 86,
    NumpadAdd: 87,
    NumpadEnter: 88,
    Numpad0: 98,
    NumpadDecimal: 99,
    IntlBackslash: 100,
    ContextMenu: 101,
    ControlLeft: 224,
    ShiftLeft: 225,
    AltLeft: 226,
    MetaLeft: 227,
    ControlRight: 228,
    ShiftRight: 229,
    AltRight: 230,
    MetaRight: 231,
  };
  for (let i = 0; i < 26; i++) m[`Key${String.fromCharCode(65 + i)}`] = 4 + i;
  for (let i = 1; i <= 9; i++) m[`Digit${i}`] = 29 + i;
  m.Digit0 = 39;
  for (let i = 1; i <= 12; i++) m[`F${i}`] = 57 + i;
  for (let i = 13; i <= 24; i++) m[`F${i}`] = 91 + i;
  for (let i = 1; i <= 9; i++) m[`Numpad${i}`] = 88 + i;
  return m;
})();

export interface ResolvedKeyboardLayout {
  keys: KeySpec[];
  cols: number;
  rows: number;
  isBc98: boolean;
}

function makeStandardKey(
  slot: number,
  id: string,
  code: string,
  label: string,
  x: number,
  y: number,
  w = 1,
  h = 1,
  tone?: KeyTone,
): KeySpec {
  const usage = BC98_CODE_TO_USAGE[code] ?? 0;
  const resolvedTone: KeyTone =
    tone ??
    (code === 'Escape' || code === 'Enter' || code === 'NumpadEnter'
      ? 'accent'
      : w > 1.05 || h > 1.05
        ? 'mod'
        : 'alpha');
  return {
    id,
    slot,
    code,
    label,
    sub: '',
    x,
    y,
    w,
    h,
    tone: resolvedTone,
    base: [0, 0, usage, 0],
    fn: [0, 0, 0, 0],
  };
}

function buildAnsiTklKeys(): KeySpec[] {
  let s = 0;
  const k = (id: string, code: string, label: string, x: number, y: number, w = 1, h = 1, tone?: KeyTone) =>
    makeStandardKey(s++, id, code, label, x, y, w, h, tone);

  return [
    // Row 0 (F-row + PrtSc/ScrLk/Pause)
    k('Esc', 'Escape', 'Esc', 0, 0, 1, 1, 'accent'),
    k('F1', 'F1', 'F1', 2, 0),
    k('F2', 'F2', 'F2', 3, 0),
    k('F3', 'F3', 'F3', 4, 0),
    k('F4', 'F4', 'F4', 5, 0),
    k('F5', 'F5', 'F5', 6.5, 0, 1, 1, 'mod'),
    k('F6', 'F6', 'F6', 7.5, 0, 1, 1, 'mod'),
    k('F7', 'F7', 'F7', 8.5, 0, 1, 1, 'mod'),
    k('F8', 'F8', 'F8', 9.5, 0, 1, 1, 'mod'),
    k('F9', 'F9', 'F9', 11, 0),
    k('F10', 'F10', 'F10', 12, 0),
    k('F11', 'F11', 'F11', 13, 0),
    k('F12', 'F12', 'F12', 14, 0),
    k('PrtSc', 'PrintScreen', 'PrtSc', 15.25, 0, 1, 1, 'mod'),
    k('ScrLk', 'ScrollLock', 'ScrLk', 16.25, 0, 1, 1, 'mod'),
    k('Pause', 'Pause', 'Pause', 17.25, 0, 1, 1, 'mod'),

    // Row 1 (Number row + Ins/Home/PgUp)
    k('`', 'Backquote', '`', 0, 1.25, 1, 1, 'mod'),
    k('1', 'Digit1', '1', 1, 1.25),
    k('2', 'Digit2', '2', 2, 1.25),
    k('3', 'Digit3', '3', 3, 1.25),
    k('4', 'Digit4', '4', 4, 1.25),
    k('5', 'Digit5', '5', 5, 1.25),
    k('6', 'Digit6', '6', 6, 1.25),
    k('7', 'Digit7', '7', 7, 1.25),
    k('8', 'Digit8', '8', 8, 1.25),
    k('9', 'Digit9', '9', 9, 1.25),
    k('0', 'Digit0', '0', 10, 1.25),
    k('-', 'Minus', '-', 11, 1.25),
    k('=', 'Equal', '=', 12, 1.25),
    k('Bksp', 'Backspace', '⌫', 13, 1.25, 2, 1, 'mod'),
    k('Ins', 'Insert', 'Ins', 15.25, 1.25, 1, 1, 'mod'),
    k('Home', 'Home', 'Home', 16.25, 1.25, 1, 1, 'mod'),
    k('PgUp', 'PageUp', 'PgUp', 17.25, 1.25, 1, 1, 'mod'),

    // Row 2 (QWERTY + Del/End/PgDn)
    k('Tab', 'Tab', 'Tab', 0, 2.25, 1.5, 1, 'mod'),
    k('Q', 'KeyQ', 'Q', 1.5, 2.25),
    k('W', 'KeyW', 'W', 2.5, 2.25),
    k('E', 'KeyE', 'E', 3.5, 2.25),
    k('R', 'KeyR', 'R', 4.5, 2.25),
    k('T', 'KeyT', 'T', 5.5, 2.25),
    k('Y', 'KeyY', 'Y', 6.5, 2.25),
    k('U', 'KeyU', 'U', 7.5, 2.25),
    k('I', 'KeyI', 'I', 8.5, 2.25),
    k('O', 'KeyO', 'O', 9.5, 2.25),
    k('P', 'KeyP', 'P', 10.5, 2.25),
    k('[', 'BracketLeft', '[', 11.5, 2.25),
    k(']', 'BracketRight', ']', 12.5, 2.25),
    k('\\', 'Backslash', '\\', 13.5, 2.25, 1.5, 1, 'mod'),
    k('Del', 'Delete', 'Del', 15.25, 2.25, 1, 1, 'mod'),
    k('End', 'End', 'End', 16.25, 2.25, 1, 1, 'mod'),
    k('PgDn', 'PageDown', 'PgDn', 17.25, 2.25, 1, 1, 'mod'),

    // Row 3 (Home row)
    k('Caps', 'CapsLock', 'Caps', 0, 3.25, 1.75, 1, 'mod'),
    k('A', 'KeyA', 'A', 1.75, 3.25),
    k('S', 'KeyS', 'S', 2.75, 3.25),
    k('D', 'KeyD', 'D', 3.75, 3.25),
    k('F', 'KeyF', 'F', 4.75, 3.25),
    k('G', 'KeyG', 'G', 5.75, 3.25),
    k('H', 'KeyH', 'H', 6.75, 3.25),
    k('J', 'KeyJ', 'J', 7.75, 3.25),
    k('K', 'KeyK', 'K', 8.75, 3.25),
    k('L', 'KeyL', 'L', 9.75, 3.25),
    k(';', 'Semicolon', ';', 10.75, 3.25),
    k("'", 'Quote', "'", 11.75, 3.25),
    k('Enter', 'Enter', 'Enter', 12.75, 3.25, 2.25, 1, 'accent'),

    // Row 4 (Shift row + Up)
    k('LShift', 'ShiftLeft', 'Shift', 0, 4.25, 2.25, 1, 'mod'),
    k('Z', 'KeyZ', 'Z', 2.25, 4.25),
    k('X', 'KeyX', 'X', 3.25, 4.25),
    k('C', 'KeyC', 'C', 4.25, 4.25),
    k('V', 'KeyV', 'V', 5.25, 4.25),
    k('B', 'KeyB', 'B', 6.25, 4.25),
    k('N', 'KeyN', 'N', 7.25, 4.25),
    k('M', 'KeyM', 'M', 8.25, 4.25),
    k(',', 'Comma', ',', 9.25, 4.25),
    k('.', 'Period', '.', 10.25, 4.25),
    k('/', 'Slash', '/', 11.25, 4.25),
    k('RShift', 'ShiftRight', 'Shift', 12.25, 4.25, 2.75, 1, 'mod'),
    k('Up', 'ArrowUp', '↑', 16.25, 4.25, 1, 1, 'mod'),

    // Row 5 (Bottom mods + Arrows)
    k('LCtrl', 'ControlLeft', 'Ctrl', 0, 5.25, 1.25, 1, 'mod'),
    k('LWin', 'MetaLeft', 'Win', 1.25, 5.25, 1.25, 1, 'mod'),
    k('LAlt', 'AltLeft', 'Alt', 2.5, 5.25, 1.25, 1, 'mod'),
    k('Space', 'Space', '', 3.75, 5.25, 6.25, 1, 'alpha'),
    k('RAlt', 'AltRight', 'Alt', 10, 5.25, 1.25, 1, 'mod'),
    k('RWin', 'MetaRight', 'Win', 11.25, 5.25, 1.25, 1, 'mod'),
    k('App', 'ContextMenu', 'Fn', 12.5, 5.25, 1.25, 1, 'mod'),
    k('RCtrl', 'ControlRight', 'Ctrl', 13.75, 5.25, 1.25, 1, 'mod'),
    k('Left', 'ArrowLeft', '←', 15.25, 5.25, 1, 1, 'mod'),
    k('Down', 'ArrowDown', '↓', 16.25, 5.25, 1, 1, 'mod'),
    k('Right', 'ArrowRight', '→', 17.25, 5.25, 1, 1, 'mod'),
  ];
}

function buildAnsi104Numpad(startSlot: number): KeySpec[] {
  let s = startSlot;
  const k = (id: string, code: string, label: string, x: number, y: number, w = 1, h = 1, tone?: KeyTone) =>
    makeStandardKey(s++, id, code, label, x, y, w, h, tone);

  return [
    k('Num', 'NumLock', 'Num', 18.75, 1.25, 1, 1, 'mod'),
    k('N/', 'NumpadDivide', '/', 19.75, 1.25, 1, 1, 'mod'),
    k('N*', 'NumpadMultiply', '*', 20.75, 1.25, 1, 1, 'mod'),
    k('N-', 'NumpadSubtract', '-', 21.75, 1.25, 1, 1, 'mod'),
    k('N7', 'Numpad7', '7', 18.75, 2.25),
    k('N8', 'Numpad8', '8', 19.75, 2.25),
    k('N9', 'Numpad9', '9', 20.75, 2.25),
    k('N+', 'NumpadAdd', '+', 21.75, 2.25, 1, 2, 'mod'),
    k('N4', 'Numpad4', '4', 18.75, 3.25),
    k('N5', 'Numpad5', '5', 19.75, 3.25),
    k('N6', 'Numpad6', '6', 20.75, 3.25),
    k('N1', 'Numpad1', '1', 18.75, 4.25),
    k('N2', 'Numpad2', '2', 19.75, 4.25),
    k('N3', 'Numpad3', '3', 20.75, 4.25),
    k('NEnter', 'NumpadEnter', 'Ent', 21.75, 4.25, 1, 2, 'accent'),
    k('N0', 'Numpad0', '0', 18.75, 5.25, 2, 1, 'alpha'),
    k('N.', 'NumpadDecimal', '.', 20.75, 5.25),
  ];
}

export const ANSI_87_KEYS: KeySpec[] = buildAnsiTklKeys();
export const ANSI_104_KEYS: KeySpec[] = [
  ...ANSI_87_KEYS,
  ...buildAnsi104Numpad(ANSI_87_KEYS.length),
];
export const ANSI_108_KEYS: KeySpec[] = [
  ...ANSI_104_KEYS,
  makeStandardKey(ANSI_104_KEYS.length, 'Extra1', 'AudioVolumeMute', '静音', 18.75, 0, 1, 1, 'mod'),
  makeStandardKey(ANSI_104_KEYS.length + 1, 'Extra2', 'AudioVolumeDown', '音量-', 19.75, 0, 1, 1, 'mod'),
  makeStandardKey(ANSI_104_KEYS.length + 2, 'Extra3', 'AudioVolumeUp', '音量+', 20.75, 0, 1, 1, 'mod'),
  makeStandardKey(ANSI_104_KEYS.length + 3, 'Extra4', 'LaunchCalculator', '计算器', 21.75, 0, 1, 1, 'mod'),
];

function normalizeCustomKeys(customKeys: KeyboardKeyDef[]): ResolvedKeyboardLayout {
  const maxW = Math.max(...customKeys.map((k) => k.w));
  const isPixelCoords = maxW > 10;
  let step = 1;
  let originX = 0;
  let originY = 0;

  if (isPixelCoords) {
    const heights = customKeys.map((k) => k.h).filter((h) => h > 0);
    const minH = heights.length > 0 ? Math.min(...heights) : 40;
    step = minH + 6;
    originX = Math.min(...customKeys.map((k) => k.x));
    originY = Math.min(...customKeys.map((k) => k.y));
  }

  const keys: KeySpec[] = customKeys.map((k, idx) => {
    const x = isPixelCoords ? (k.x - originX) / step : k.x;
    const y = isPixelCoords ? (k.y - originY) / step : k.y;
    const w = isPixelCoords ? k.w / step : k.w;
    const h = isPixelCoords ? k.h / step : k.h;
    const code = k.code ?? k.label;
    const usage = BC98_CODE_TO_USAGE[code] ?? 0;
    const tone: KeyTone =
      k.tone ??
      (code === 'Escape' || code === 'Enter' || code === 'NumpadEnter'
        ? 'accent'
        : w > 1.05 || h > 1.05
          ? 'mod'
          : 'alpha');
    return {
      id: `${k.label || 'key'}-${k.slot}-${idx}`,
      slot: k.slot,
      code,
      label: k.label,
      sub: k.sub ?? '',
      x,
      y,
      w,
      h,
      tone,
      base: k.base ?? [0, 0, usage, 0],
      fn: k.fn ?? [0, 0, 0, 0],
    };
  });

  const cols = Math.max(1, ...keys.map((k) => k.x + k.w));
  const rows = Math.max(1, ...keys.map((k) => k.y + k.h)) + 0.05;
  return { keys, cols, rows, isBc98: false };
}

export function resolveKeyboardLayout(
  layoutId?: string,
  customKeys?: KeyboardKeyDef[],
): ResolvedKeyboardLayout {
  if (customKeys && customKeys.length > 0) {
    return normalizeCustomKeys(customKeys);
  }
  if (layoutId === 'ansi-87') {
    return { keys: ANSI_87_KEYS, cols: 18.25, rows: 6.25, isBc98: false };
  }
  if (layoutId === 'ansi-104') {
    return { keys: ANSI_104_KEYS, cols: 22.75, rows: 6.25, isBc98: false };
  }
  if (layoutId === 'ansi-108') {
    return { keys: ANSI_108_KEYS, cols: 22.75, rows: 6.25, isBc98: false };
  }
  return { keys: BC98_KEYS, cols: 19.25, rows: 6.3, isBc98: true };
}

export const KB_PICKER_CATEGORIES: Record<KbCatId, PickerCategory> = {
  basic: {
    name: '基础键',
    items: [
      { label: 'Esc', entry: [0, 0, 41, 0] },
      { label: 'Tab', entry: [0, 0, 43, 0] },
      { label: 'Caps', entry: [0, 0, 57, 0] },
      { label: 'L-Shift', entry: [0, 0, 225, 0] },
      { label: 'L-Ctrl', entry: [0, 0, 224, 0] },
      { label: 'L-Alt', entry: [0, 0, 226, 0] },
      { label: 'L-Win', entry: [0, 0, 227, 0] },
      { label: 'Space', entry: [0, 0, 44, 0] },
      { label: 'Enter', entry: [0, 0, 40, 0] },
      { label: '⌫', entry: [0, 0, 42, 0] },
      { label: 'Del', entry: [0, 0, 76, 0] },
      { label: 'Home', entry: [0, 0, 74, 0] },
      { label: 'End', entry: [0, 0, 77, 0] },
      { label: 'PgUp', entry: [0, 0, 75, 0] },
      { label: 'PgDn', entry: [0, 0, 78, 0] },
      { label: '↑', entry: [0, 0, 82, 0] },
      { label: '↓', entry: [0, 0, 81, 0] },
      { label: '←', entry: [0, 0, 80, 0] },
      { label: '→', entry: [0, 0, 79, 0] },
      { label: 'PrtSc', entry: [0, 0, 70, 0] },
      { label: 'ScrLk', entry: [0, 0, 71, 0] },
      { label: 'Pause', entry: [0, 0, 72, 0] },
      { label: 'Ins', entry: [0, 0, 73, 0] },
      { label: 'App', entry: [0, 0, 101, 0] },
      { label: 'R-Ctrl', entry: [0, 0, 228, 0] },
      { label: 'R-Shift', entry: [0, 0, 229, 0] },
      { label: 'R-Alt', entry: [0, 0, 230, 0] },
      { label: 'R-Win', entry: [0, 0, 231, 0] },
      { label: '- _', entry: [0, 0, 45, 0] },
      { label: '= +', entry: [0, 0, 46, 0] },
      { label: '[ {', entry: [0, 0, 47, 0] },
      { label: '] }', entry: [0, 0, 48, 0] },
      { label: '\\ |', entry: [0, 0, 49, 0] },
      { label: '; :', entry: [0, 0, 51, 0] },
      { label: "' \"", entry: [0, 0, 52, 0] },
      { label: '` ~', entry: [0, 0, 53, 0] },
      { label: ', <', entry: [0, 0, 54, 0] },
      { label: '. >', entry: [0, 0, 55, 0] },
      { label: '/ ?', entry: [0, 0, 56, 0] },
    ],
  },
  alpha: {
    name: '字母与功能',
    items: [
      ...Array.from({ length: 26 }, (_, i) => ({
        label: String.fromCharCode(65 + i),
        entry: [0, 0, 4 + i, 0] as KbdKeyEntry,
      })),
      ...Array.from({ length: 9 }, (_, i) => ({
        label: String(i + 1),
        entry: [0, 0, 30 + i, 0] as KbdKeyEntry,
      })),
      { label: '0', entry: [0, 0, 39, 0] },
      ...Array.from({ length: 12 }, (_, i) => ({
        label: `F${i + 1}`,
        entry: [0, 0, 58 + i, 0] as KbdKeyEntry,
      })),
      ...Array.from({ length: 12 }, (_, i) => ({
        label: `F${i + 13}`,
        entry: [0, 0, 104 + i, 0] as KbdKeyEntry,
      })),
    ],
  },
  numpad: {
    name: '小键盘',
    items: [
      { label: 'NumLk', entry: [0, 0, 83, 0] },
      { label: 'Num /', entry: [0, 0, 84, 0] },
      { label: 'Num *', entry: [0, 0, 85, 0] },
      { label: 'Num -', entry: [0, 0, 86, 0] },
      { label: 'Num +', entry: [0, 0, 87, 0] },
      { label: 'Num Enter', entry: [0, 0, 88, 0] },
      { label: 'Num 0', entry: [0, 0, 98, 0] },
      { label: 'Num .', entry: [0, 0, 99, 0] },
      ...Array.from({ length: 9 }, (_, i) => ({
        label: `Num ${i + 1}`,
        entry: [0, 0, 89 + i, 0] as KbdKeyEntry,
      })),
    ],
  },
  combo: {
    name: '组合键',
    items: [
      { label: 'Ctrl+C', entry: [0, 224, 6, 0] },
      { label: 'Ctrl+V', entry: [0, 224, 25, 0] },
      { label: 'Ctrl+X', entry: [0, 224, 27, 0] },
      { label: 'Ctrl+Z', entry: [0, 224, 29, 0] },
      { label: 'Ctrl+S', entry: [0, 224, 22, 0] },
      { label: 'Ctrl+A', entry: [0, 224, 4, 0] },
      { label: 'Alt+Tab', entry: [0, 226, 43, 0] },
      { label: 'Alt+F4', entry: [0, 226, 61, 0] },
      { label: 'Win+D', entry: [0, 227, 7, 0] },
      { label: 'Win+Shift+S', entry: [0, 227, 22, 225] },
    ],
  },
  media: {
    name: '媒体',
    items: [
      { label: '播放/暂停', entry: [3, 0, 205, 0] },
      { label: '音量+', entry: [3, 0, 233, 0] },
      { label: '音量-', entry: [3, 0, 234, 0] },
      { label: '静音', entry: [3, 0, 226, 0] },
      { label: '上一曲', entry: [3, 0, 182, 0] },
      { label: '下一曲', entry: [3, 0, 181, 0] },
      { label: '计算器', entry: [3, 0, 146, 1] },
      { label: '我的电脑', entry: [3, 0, 148, 1] },
    ],
  },
  special: {
    name: '灯光与系统',
    items: [
      { label: '切换灯效', entry: [13, 1, 0, 0] },
      { label: '亮度+', entry: [13, 2, 1, 0] },
      { label: '亮度-', entry: [13, 2, 2, 0] },
      { label: '速度+', entry: [13, 3, 1, 0] },
      { label: '速度-', entry: [13, 3, 2, 0] },
      { label: '主灯开关', entry: [13, 0, 0, 0] },
      { label: '蓝牙 1', entry: [14, 0, 0, 0] },
      { label: '蓝牙 2', entry: [14, 0, 1, 0] },
      { label: '2.4G 模式', entry: [14, 0, 5, 0] },
      { label: 'USB 有线', entry: [14, 0, 6, 0] },
      { label: 'Fn 键', entry: [10, 1, 0, 0] },
      { label: '电量查询', entry: [10, 8, 0, 0] },
      { label: '透传 / 禁用', entry: [0, 0, 0, 0] },
    ],
  },
};

export const BC98_USAGE_TO_LABEL: Record<number, string> = (() => {
  const m: Record<number, string> = {
    4: 'A', 5: 'B', 6: 'C', 7: 'D', 8: 'E', 9: 'F', 10: 'G', 11: 'H',
    12: 'I', 13: 'J', 14: 'K', 15: 'L', 16: 'M', 17: 'N', 18: 'O', 19: 'P',
    20: 'Q', 21: 'R', 22: 'S', 23: 'T', 24: 'U', 25: 'V', 26: 'W', 27: 'X',
    28: 'Y', 29: 'Z',
    30: '1', 31: '2', 32: '3', 33: '4', 34: '5', 35: '6', 36: '7', 37: '8', 38: '9', 39: '0',
    40: 'Enter', 41: 'Esc', 42: '⌫', 43: 'Tab', 44: 'Space',
    45: '-', 46: '=', 47: '[', 48: ']', 49: '\\', 51: ';', 52: "'", 53: '`',
    54: ',', 55: '.', 56: '/', 57: 'Caps',
    70: 'PrtSc', 71: 'ScrLk', 72: 'Pause', 73: 'Ins', 74: 'Home', 75: 'PgUp',
    76: 'Del', 77: 'End', 78: 'PgDn', 79: '→', 80: '←', 81: '↓', 82: '↑',
    83: 'Num', 84: 'N/', 85: 'N*', 86: 'N-', 87: 'N+', 88: 'NEnt',
    89: 'N1', 90: 'N2', 91: 'N3', 92: 'N4', 93: 'N5', 94: 'N6', 95: 'N7', 96: 'N8', 97: 'N9',
    98: 'N0', 99: 'N.', 100: 'ISO\\', 101: 'App',
    224: 'Ctrl', 225: 'Shift', 226: 'Alt', 227: 'Win',
    228: 'RCtrl', 229: 'RShift', 230: 'RAlt', 231: 'RWin',
  };
  for (let i = 1; i <= 12; i++) m[57 + i] = `F${i}`;
  for (let i = 13; i <= 24; i++) m[91 + i] = `F${i}`;
  return m;
})();

const EXACT_ENTRY_LABELS = new Map<string, string>();
for (const cat of Object.values(KB_PICKER_CATEGORIES)) {
  for (const item of cat.items) {
    EXACT_ENTRY_LABELS.set(item.entry.join(','), item.label);
  }
}
EXACT_ENTRY_LABELS.set('10,2,0,0', '长按复位');
EXACT_ENTRY_LABELS.set('10,3,0,0', 'WinLock');
EXACT_ENTRY_LABELS.set('10,9,0,0', '背光开关');
EXACT_ENTRY_LABELS.set('10,10,0,0', 'WASD互换');
EXACT_ENTRY_LABELS.set('10,12,0,0', '省电开关');
EXACT_ENTRY_LABELS.set('10,13,0,0', 'Fn 锁');
EXACT_ENTRY_LABELS.set('10,17,0,0', 'Caps灯');

export function entriesEqual(a: KbdKeyEntry | undefined, b: KbdKeyEntry | undefined): boolean {
  return (
    Array.isArray(a) &&
    Array.isArray(b) &&
    a.length === 4 &&
    b.length === 4 &&
    a[0] === b[0] &&
    a[1] === b[1] &&
    a[2] === b[2] &&
    a[3] === b[3]
  );
}

export function formatRoyuanKeyEntry(entry: KbdKeyEntry | undefined, fnLayer = false): string {
  if (!Array.isArray(entry) || entry.length !== 4) return '—';
  const [tag, a, b, c] = entry;
  if (tag === 0 && a === 0 && b === 0 && c === 0) {
    return fnLayer ? '▽' : '禁用';
  }
  const exact = EXACT_ENTRY_LABELS.get(entry.join(','));
  if (exact) return exact;
  if (tag === 0) {
    const parts = [a, b, c]
      .filter((v) => v > 0)
      .map((u) => BC98_USAGE_TO_LABEL[u] ?? `0x${u.toString(16).toUpperCase()}`);
    return parts.length > 0 ? parts.join('+') : '禁用';
  }
  if (tag === 9) {
    const modeTxt = a === 1 ? '开关' : a === 2 ? '按住' : '次数';
    return `M${b + 1}(${modeTxt})`;
  }
  if (tag === 1) return '鼠标功能';
  if (tag === 3) return '多媒体';
  if (tag === 10) return '系统功能';
  if (tag === 13) return '灯效控制';
  if (tag === 14) return '无线控制';
  return `${tag}:${a}:${b}:${c}`;
}

export function computeKbdMacroStreamBytes(
  events: Array<{ kind: string; delayMs?: number }>,
): number {
  let bytes = 0;
  for (const ev of events) {
    if (ev.kind === 'mouseMove') {
      bytes += 4;
    } else {
      const d = Number(ev.delayMs || 0);
      bytes += d >= 1 && d <= 127 ? 2 : 4;
    }
  }
  return bytes;
}
