import type { MouseButtonId } from './mouse-codec';

export const STAGE_COLORS = ['#f43f5e', '#38bdf8', '#34d399', '#f5b027', '#a78bfa', '#2dd4bf'];
export const MOUSE_LED_COLORS = ['#38BDF8', '#34D399', '#F43F5E', '#F5B027', '#A78BFA', '#F8FAFC'];

export const HOTSPOTS: Array<{ id: MouseButtonId; x: number; y: number }> = [
  { id: 'left', x: 28, y: 22 },
  { id: 'right', x: 72, y: 22 },
  { id: 'middle', x: 50, y: 19.5 },
  { id: 'dpi', x: 50, y: 38.2 },
  { id: 'side_fwd', x: 5.5, y: 37 },
  { id: 'side_back', x: 6.5, y: 51 },
];

export function buildDpiAnchors(min: number, max: number): number[] {
  const mids = [800, 1600, 3200, 16000].filter((v) => v > min && v < max);
  return [min, ...mids, max];
}

export function dpiToSlider(dpi: number, anchors: number[]): number {
  const min = anchors[0];
  const max = anchors[anchors.length - 1];
  const v = Math.max(min, Math.min(max, dpi));
  for (let i = 0; i < anchors.length - 1; i++) {
    const a = anchors[i];
    const b = anchors[i + 1];
    if (v <= b) {
      const segFrac = b === a ? 0 : (v - a) / (b - a);
      return Math.round(((i + segFrac) / (anchors.length - 1)) * 1000);
    }
  }
  return 1000;
}

export function sliderToDpi(pos: number, anchors: number[], step: number): number {
  const min = anchors[0];
  const max = anchors[anchors.length - 1];
  const p = Math.max(0, Math.min(1000, Number(pos)));
  const scaled = (p / 1000) * (anchors.length - 1);
  const i = Math.min(anchors.length - 2, Math.floor(scaled));
  const segFrac = scaled - i;
  const raw = anchors[i] + segFrac * (anchors[i + 1] - anchors[i]);
  const snapped = Math.round(raw / step) * step;
  return Math.max(min, Math.min(max, snapped));
}

export function formatSleepLabel(sec: number): string {
  if (sec >= 60 && sec % 60 === 0) return `${sec / 60}m`;
  return `${sec}s`;
}
