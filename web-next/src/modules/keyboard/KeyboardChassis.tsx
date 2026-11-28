import { type CSSProperties, useEffect, useRef, useState } from 'react';
import type { KbdKeysState } from '../../bridge/types';
import { hexToRgba } from '../../ui/PrecisionControls';
import { deviceHeroTransitionName } from '../../ui/view-transition';
import { entriesEqual, formatRoyuanKeyEntry, type ResolvedKeyboardLayout } from './bc98-layout';

export function KeyboardChassis({
  deviceKey,
  isHeroTransition,
  layout,
  keysState,
  layer,
  selSlot,
  onSelectSlot,
  isLight,
  backlightLit,
  isMultiColorDisplay,
  effBri,
  activeLedColor,
}: {
  deviceKey: string;
  isHeroTransition: boolean;
  layout: ResolvedKeyboardLayout;
  keysState: KbdKeysState | null;
  layer: 'base' | 'fn';
  selSlot: number;
  onSelectSlot: (slot: number) => void;
  isLight: boolean;
  backlightLit: boolean;
  isMultiColorDisplay: boolean;
  effBri: number;
  activeLedColor: string;
}) {
  const stageContainerRef = useRef<HTMLDivElement>(null);
  const [unitPx, setUnitPx] = useState(50);

  useEffect(() => {
    const el = stageContainerRef.current;
    if (!el || typeof ResizeObserver === 'undefined') return;
    const updateUnit = () => {
      const w = el.clientWidth;
      if (w > 0) {
        const next = Math.max(26, Math.min(52, Math.floor((w - 58) / layout.cols)));
        setUnitPx(next);
      }
    };
    updateUnit();
    const ro = new ResizeObserver(updateUnit);
    ro.observe(el);
    return () => ro.disconnect();
  }, [layout.cols]);

  const UNIT = unitPx;
  const GAP = unitPx < 38 ? 3 : 4;
  const cols = layout.cols;
  const rows = layout.rows;
  const boardW = Math.round(cols * UNIT);
  const boardH = Math.round(rows * UNIT);
  const activeLayerArray = keysState ? (layer === 'fn' ? keysState.fn : keysState.base) : null;

  return (
    <div
      ref={stageContainerRef}
      className="relative z-10 w-full overflow-x-auto py-2 flex justify-center"
    >
      <div
        className={`rounded-[20px] shrink-0 ${unitPx < 40 ? 'p-3' : 'p-4'}`}
        style={
          {
            ...(isHeroTransition
              ? {
                  viewTransitionName: deviceHeroTransitionName(deviceKey),
                  viewTransitionClass: 'device-hero-vt',
                }
              : {}),
            background: 'var(--c-kb-case)',
            border: '1px solid var(--c-kb-bevel)',
            boxShadow: 'inset 0 1px 0 var(--c-kb-bevel), 0 30px 64px -16px rgba(0,0,0,0.62)',
          } as CSSProperties
        }
      >
        <div
          className="relative rounded-xl p-2"
          style={{
            background: 'var(--c-kb-plate)',
            boxShadow: 'inset 0 2px 8px rgba(0,0,0,0.55)',
          }}
        >
          <div className="relative" style={{ width: boardW, height: boardH }}>
            {layout.isBc98 && (
              <div
                className="pointer-events-none absolute flex items-center gap-2"
                style={{ left: 15.45 * UNIT - 6, top: 1.06 * UNIT }}
              >
                <span
                  className="w-1.5 h-1.5 rounded-full"
                  style={{ background: 'var(--c-ok)', boxShadow: '0 0 6px var(--c-ok)' }}
                />
                <span className="w-1.5 h-1.5 rounded-full" style={{ background: 'var(--c-border-strong)' }} />
                <span className="w-1.5 h-1.5 rounded-full" style={{ background: 'var(--c-border-strong)' }} />
              </div>
            )}

            {layout.keys.map((k) => {
              const left = Math.round(k.x * UNIT + GAP / 2);
              const top = Math.round(k.y * UNIT + GAP / 2);
              const width = Math.round(k.w * UNIT - GAP);
              const height = Math.round(k.h * UNIT - GAP);

              const hwEntry = activeLayerArray?.[k.slot];
              const factoryEntry = layer === 'fn' ? k.fn : k.base;
              const isModified = hwEntry ? !entriesEqual(hwEntry, factoryEntry) : false;

              const primary = hwEntry
                ? formatRoyuanKeyEntry(hwEntry, layer === 'fn')
                : layer === 'fn'
                  ? formatRoyuanKeyEntry(k.fn, true)
                  : k.label;
              const sel = !isLight && selSlot === k.slot;
              const fnSub =
                layer === 'base'
                  ? keysState?.fn?.[k.slot]
                    ? formatRoyuanKeyEntry(keysState.fn[k.slot], true)
                    : k.sub
                  : k.label;

              const hasCjk = /[\u4e00-\u9fff]/.test(primary);
              const visualLen = hasCjk ? primary.length * 1.65 : primary.length;
              const compactBoard = UNIT < 42;
              const primarySizeClass = compactBoard
                ? visualLen >= 4.5
                  ? 'text-[7.5px] leading-[1.05] tracking-tighter'
                  : 'text-[9.5px] leading-none'
                : k.w <= 1.05
                  ? visualLen >= 5.5
                    ? 'text-[8.5px] leading-[1.1] tracking-tighter'
                    : visualLen >= 3.8
                      ? 'text-[9.5px] leading-[1.1] tracking-tight'
                      : 'text-[11px] leading-none'
                  : visualLen >= 7
                    ? 'text-[9.5px] leading-[1.1] tracking-tight'
                    : 'text-[11px] leading-none';

              const keyGlowShadow = !isLight
                ? undefined
                : !backlightLit
                  ? 'none'
                  : isMultiColorDisplay
                    ? (() => {
                        const hue = Math.round(((k.x / Math.max(1, cols)) * 300 + 185) % 360);
                        const color = `hsla(${hue}, 82%, 58%, ${(0.25 + 0.55 * effBri).toFixed(2)})`;
                        return `0 0 ${Math.round(5 + 9 * effBri)}px ${color}, 0 2px 0 ${color}`;
                      })()
                    : `0 0 ${Math.round(5 + 9 * effBri)}px ${hexToRgba(activeLedColor, 0.28 + 0.52 * effBri)}, 0 2px 0 ${hexToRgba(activeLedColor, 0.25 + 0.45 * effBri)}`;

              return (
                <button
                  key={k.slot}
                  type="button"
                  data-tone={k.tone}
                  data-r={k.y}
                  data-c={k.x}
                  title={`${k.label || k.id} (Slot ${k.slot}): ${primary}`}
                  onClick={() => !isLight && onSelectSlot(k.slot)}
                  className={`pbt-key text-left ${sel ? 'is-selected' : ''} ${isLight ? 'rgb-key' : ''}`}
                  style={{
                    left,
                    top,
                    width,
                    height,
                    ...(keyGlowShadow !== undefined ? { boxShadow: keyGlowShadow } : {}),
                  }}
                >
                  <div className="pbt-key-cap">
                    {isModified && !isLight && (
                      <span
                        className="pointer-events-none absolute top-1 right-1 w-1.5 h-1.5 rounded-full"
                        style={{ background: 'var(--c-ok)', boxShadow: '0 0 6px var(--c-ok)' }}
                      />
                    )}
                    <span className={`font-semibold break-all line-clamp-2 w-full ${primarySizeClass}`}>
                      {primary}
                    </span>
                    {fnSub && fnSub !== '▽' && (
                      <span className="text-[8.5px] leading-none opacity-45 truncate max-w-full">
                        {fnSub}
                      </span>
                    )}
                  </div>
                </button>
              );
            })}
          </div>
        </div>
      </div>
    </div>
  );
}
