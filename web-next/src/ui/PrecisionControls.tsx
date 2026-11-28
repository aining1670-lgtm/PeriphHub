import { useRef, useState } from 'react';

export function hexToRgba(hex: string, alpha: number): string {
  const h = hex.replace('#', '');
  const r = parseInt(h.slice(0, 2), 16);
  const g = parseInt(h.slice(2, 4), 16);
  const b = parseInt(h.slice(4, 6), 16);
  return `rgba(${r}, ${g}, ${b}, ${alpha})`;
}

export function PrecisionSlider({
  pct,
  color,
  compact = false,
  disabled = false,
  ariaLabel,
  onMove,
  onCommit,
}: {
  pct: number;
  color?: string;
  compact?: boolean;
  disabled?: boolean;
  ariaLabel?: string;
  onMove?: (frac: number) => void;
  onCommit: (frac: number) => void;
}) {
  const trackRef = useRef<HTMLDivElement>(null);
  const [dragPct, setDragPct] = useState<number | null>(null);
  const draggingRef = useRef(false);
  const lastFracRef = useRef(pct / 100);

  const clampedPct = Math.max(0, Math.min(100, dragPct ?? pct));
  const accent = color || 'var(--c-hi)';

  const clientToFrac = (clientX: number) => {
    const rect = trackRef.current?.getBoundingClientRect();
    if (!rect || rect.width <= 0) return 0;
    return Math.max(0, Math.min(1, (clientX - rect.left) / rect.width));
  };

  return (
    <div
      ref={trackRef}
      role="slider"
      tabIndex={disabled ? -1 : 0}
      aria-label={ariaLabel}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={Math.round(clampedPct)}
      aria-disabled={disabled}
      onPointerDown={(e) => {
        if (disabled) return;
        draggingRef.current = true;
        e.currentTarget.setPointerCapture(e.pointerId);
        const f = clientToFrac(e.clientX);
        lastFracRef.current = f;
        setDragPct(f * 100);
        onMove?.(f);
      }}
      onPointerMove={(e) => {
        if (!draggingRef.current || disabled) return;
        const f = clientToFrac(e.clientX);
        lastFracRef.current = f;
        setDragPct(f * 100);
        onMove?.(f);
      }}
      onPointerUp={() => {
        if (!draggingRef.current) return;
        draggingRef.current = false;
        if (!disabled) onCommit(lastFracRef.current);
        setDragPct(null);
      }}
      onPointerCancel={() => {
        if (!draggingRef.current) return;
        draggingRef.current = false;
        setDragPct(null);
      }}
      onKeyDown={(e) => {
        if (disabled) return;
        const step = e.shiftKey ? 0.1 : 0.02;
        if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') {
          e.preventDefault();
          onCommit(Math.max(0, clampedPct / 100 - step));
        } else if (e.key === 'ArrowRight' || e.key === 'ArrowUp') {
          e.preventDefault();
          onCommit(Math.min(1, clampedPct / 100 + step));
        }
      }}
      className={`relative ${compact ? 'h-6' : 'h-7'} w-full flex items-center select-none touch-none group ${
        disabled ? 'opacity-45 cursor-not-allowed' : 'cursor-ew-resize'
      }`}
    >
      <div
        className={`relative w-full ${compact ? 'h-[4px]' : 'h-[5px]'} rounded-full overflow-hidden`}
        style={{ background: 'var(--c-border-strong)' }}
      >
        <div
          className="h-full rounded-full"
          style={{ width: `${clampedPct.toFixed(2)}%`, background: accent }}
        />
      </div>
      <div
        className={`pointer-events-none absolute top-1/2 -translate-x-1/2 -translate-y-1/2 ${
          compact ? 'w-[12px] h-[18px]' : 'w-[14px] h-[20px]'
        } rounded-[3px] transition-transform duration-100 group-hover:scale-105`}
        style={{
          left: `${clampedPct.toFixed(2)}%`,
          background: 'var(--c-hi)',
          border: '2px solid var(--c-bg)',
          boxShadow: '0 2px 10px rgba(0, 0, 0, 0.45)',
        }}
      />
    </div>
  );
}

export function SegmentedTrack({
  labels,
  activeIndex,
  onCommit,
  warnFromIndex,
  disabled = false,
  ariaLabel,
  widthClass = 'w-full',
  itemPaddingClass = 'py-2',
  textClass = 'text-xs',
}: {
  labels: string[];
  activeIndex: number;
  onCommit: (index: number) => void;
  warnFromIndex?: number;
  disabled?: boolean;
  ariaLabel?: string;
  widthClass?: string;
  itemPaddingClass?: string;
  textClass?: string;
}) {
  const trackRef = useRef<HTMLDivElement>(null);
  const [dragPos, setDragPos] = useState<number | null>(null);
  const draggingRef = useRef(false);
  const liveIdxRef = useRef(activeIndex);

  const count = Math.max(1, labels.length);
  const safeActiveIdx = activeIndex >= 0 && activeIndex < count ? activeIndex : -1;
  const displayIdx = dragPos !== null ? Math.round(dragPos) : safeActiveIdx;
  const thumbPos = dragPos !== null ? dragPos * 100 : safeActiveIdx * 100;
  const isWarn = warnFromIndex !== undefined && displayIdx >= warnFromIndex;

  const clientToPos = (clientX: number) => {
    const rect = trackRef.current?.getBoundingClientRect();
    if (!rect || rect.width <= 8) return { clamped: 0, idx: 0 };
    const segW = (rect.width - 8) / count;
    const raw = (clientX - rect.left - 4 - segW / 2) / segW;
    const clamped = Math.max(0, Math.min(count - 1, raw));
    return { clamped, idx: Math.round(clamped) };
  };

  return (
    <div
      ref={trackRef}
      role="radiogroup"
      tabIndex={disabled ? -1 : 0}
      aria-label={ariaLabel}
      aria-disabled={disabled}
      onKeyDown={(e) => {
        if (disabled || labels.length === 0) return;
        const base = safeActiveIdx >= 0 ? safeActiveIdx : 0;
        let nextIdx: number | null = null;
        if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') {
          nextIdx = Math.max(0, base - 1);
        } else if (e.key === 'ArrowRight' || e.key === 'ArrowDown') {
          nextIdx = Math.min(labels.length - 1, base + 1);
        } else if (e.key === 'Home') {
          nextIdx = 0;
        } else if (e.key === 'End') {
          nextIdx = labels.length - 1;
        }
        if (nextIdx !== null) {
          e.preventDefault();
          if (nextIdx !== safeActiveIdx) {
            onCommit(nextIdx);
          }
        }
      }}
      onPointerDown={(e) => {
        if (disabled || labels.length === 0) return;
        draggingRef.current = true;
        e.currentTarget.setPointerCapture(e.pointerId);
        const { idx } = clientToPos(e.clientX);
        liveIdxRef.current = idx;
        setDragPos(idx);
      }}
      onPointerMove={(e) => {
        if (!draggingRef.current || disabled) return;
        const { clamped, idx } = clientToPos(e.clientX);
        liveIdxRef.current = idx;
        setDragPos(clamped);
      }}
      onPointerUp={() => {
        if (!draggingRef.current) return;
        draggingRef.current = false;
        if (!disabled) onCommit(liveIdxRef.current);
        setDragPos(null);
      }}
      onPointerCancel={() => {
        if (!draggingRef.current) return;
        draggingRef.current = false;
        setDragPos(null);
      }}
      className={`relative grid p-1 rounded-xl ${widthClass} shrink-0 select-none touch-none ${
        disabled ? 'opacity-45 cursor-not-allowed' : 'cursor-grab active:cursor-grabbing'
      }`}
      style={{
        gridTemplateColumns: `repeat(${count}, minmax(0, 1fr))`,
        background: 'var(--c-elev)',
        border: '1px solid var(--c-border)',
      }}
    >
      {displayIdx >= 0 && (
        <div
          className="pointer-events-none absolute top-1 bottom-1 left-1 rounded-lg shadow-sm"
          style={{
            width: `calc((100% - 8px) / ${count})`,
            transform: `translateX(${thumbPos}%)`,
            transition:
              dragPos !== null
                ? 'background-color 180ms ease'
                : 'transform 200ms cubic-bezier(0.22, 1, 0.36, 1), background-color 180ms ease',
            background: isWarn ? 'var(--c-warn)' : 'var(--c-hi)',
          }}
        />
      )}
      {labels.map((label, i) => (
        <div
          key={label}
          role="radio"
          aria-checked={i === displayIdx}
          className={`relative z-10 ${itemPaddingClass} text-center ${textClass} font-semibold c-num transition-colors duration-150 pointer-events-none`}
          style={{ color: i === displayIdx ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)' }}
        >
          {label}
        </div>
      ))}
    </div>
  );
}
