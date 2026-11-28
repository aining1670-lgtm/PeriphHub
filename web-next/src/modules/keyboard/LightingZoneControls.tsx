import { useEffect, useRef } from 'react';
import type { KbdLedState, KbdState } from '../../bridge/types';
import { PrecisionSlider } from '../../ui/PrecisionControls';
import { LIGHT_COLORS } from './kbd-state-helpers';

export function LightingZoneControls({
  zone,
  resetKey,
  led,
  ledOn,
  modes,
  brightnessMax,
  speedMax,
  liveBrightness,
  liveSpeed,
  loading,
  onLiveBrightness,
  onLiveSpeed,
  onPatch,
  onOptimisticColor,
}: {
  zone: 'mainLed' | 'sideLed';
  resetKey: string;
  led: KbdLedState | null;
  ledOn: boolean;
  modes: ReadonlyArray<{ code: number; label: string; supportsColor?: boolean }>;
  brightnessMax: number;
  speedMax: number;
  liveBrightness: number | null;
  liveSpeed: number | null;
  loading: boolean;
  onLiveBrightness: (v: number | null) => void;
  onLiveSpeed: (v: number | null) => void;
  onPatch: (
    patch: Record<string, unknown>,
    label: string,
    fieldKey: string,
  ) => Promise<KbdState | null>;
  onOptimisticColor: (hex: string) => void;
}) {
  const colorTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => {
    return () => {
      if (colorTimerRef.current) clearTimeout(colorTimerRef.current);
    };
  }, [resetKey]);

  const isSide = zone === 'sideLed';
  const activeMode = led?.mode ?? null;
  const activeColor = led?.color ?? '#38BDF8';
  const activeBrightness = liveBrightness ?? led?.brightness ?? 0;
  const activeSpeed = liveSpeed ?? led?.speed ?? 0;
  const isColorful = led?.colorful ?? false;
  const activeModeObj = modes.find((m) => m.code === activeMode);
  const supportsColor = activeModeObj ? Boolean(activeModeObj.supportsColor) : true;

  const modeTitle = isSide ? '侧翼灯效模式' : '主背光模式';
  const switchLabel = isSide ? '侧翼灯效开关' : '主背光开关';
  const modePrefix = isSide ? '侧翼灯效' : '灯效';
  const brightnessLabel = isSide ? '侧翼亮度' : '亮度等级';
  const brightnessAria = isSide ? '侧翼灯效亮度' : '背光亮度';
  const speedLabel = isSide ? '侧翼速度' : '动画速度';
  const speedAria = isSide ? '侧翼灯效速度' : '背光速度';
  const colorHeading = supportsColor
    ? isSide
      ? '侧翼色调'
      : '灯效色调'
    : isSide
      ? '当前侧翼灯效为固定混色'
      : '当前灯效为固定混色';

  return (
    <>
      <div className="md:col-span-6 space-y-3">
        <div className="flex items-center justify-between">
          <span className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
            {modeTitle}
          </span>
          <div className="flex items-center gap-2">
            <span className="text-[11px]" style={{ color: 'var(--c-ink-muted)' }}>
              {ledOn ? '开启' : '关闭'}
            </span>
            <button
              type="button"
              role="switch"
              aria-label={switchLabel}
              aria-checked={ledOn}
              data-on={String(ledOn)}
              disabled={loading || !led}
              onClick={() => void onPatch({ [zone]: { on: !ledOn } }, switchLabel, zone)}
              className="hw-switch"
            />
          </div>
        </div>
        <div className="flex flex-wrap gap-1.5">
          {modes.map((m) => {
            const active = activeMode === m.code;
            return (
              <button
                key={m.code}
                type="button"
                disabled={loading}
                onClick={() =>
                  void onPatch(
                    { [zone]: { mode: m.code, on: true } },
                    `${modePrefix} · ${m.label}`,
                    zone,
                  )
                }
                className="px-2.5 py-1.5 rounded-lg text-xs font-medium cursor-pointer disabled:opacity-45"
                style={{
                  background: active ? 'var(--c-hi)' : 'var(--c-elev)',
                  color: active ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                }}
              >
                {m.label}
              </button>
            );
          })}
        </div>
      </div>

      <div className="md:col-span-3 space-y-4">
        <div className="space-y-1.5">
          <div className="flex justify-between text-xs">
            <span style={{ color: 'var(--c-ink-muted)' }}>{brightnessLabel}</span>
            <span className="c-num font-medium">
              {led ? `${activeBrightness} / ${brightnessMax}` : '—'}
            </span>
          </div>
          <PrecisionSlider
            ariaLabel={brightnessAria}
            pct={(activeBrightness / Math.max(1, brightnessMax)) * 100}
            color={isColorful ? undefined : activeColor}
            compact
            disabled={loading || !led}
            onMove={(f) => onLiveBrightness(Math.round(f * brightnessMax))}
            onCommit={(f) => {
              const v = Math.round(f * brightnessMax);
              onLiveBrightness(null);
              void onPatch({ [zone]: { brightness: v } }, brightnessAria, zone);
            }}
          />
        </div>

        <div className="space-y-1.5">
          <div className="flex justify-between text-xs">
            <span style={{ color: 'var(--c-ink-muted)' }}>{speedLabel}</span>
            <span className="c-num font-medium">
              {led ? `${activeSpeed} / ${speedMax}` : '—'}
            </span>
          </div>
          <PrecisionSlider
            ariaLabel={speedAria}
            pct={(activeSpeed / Math.max(1, speedMax)) * 100}
            compact
            disabled={loading || !led}
            onMove={(f) => onLiveSpeed(Math.round(f * speedMax))}
            onCommit={(f) => {
              const v = Math.round(f * speedMax);
              onLiveSpeed(null);
              void onPatch({ [zone]: { speed: v } }, speedAria, zone);
            }}
          />
        </div>
      </div>

      <div className="md:col-span-3 space-y-3">
        <div className="flex items-center justify-between">
          <span className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
            {colorHeading}
          </span>
          <button
            type="button"
            disabled={loading || !led || !supportsColor}
            onClick={() =>
              void onPatch(
                { [zone]: { colorful: !isColorful } },
                isSide ? '侧翼炫彩混色' : '炫彩混色',
                zone,
              )
            }
            className="px-2.5 py-1 rounded-md text-[11px] font-medium cursor-pointer disabled:opacity-40"
            style={{
              background: isColorful ? 'var(--c-hi)' : 'var(--c-elev)',
              color: isColorful ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
            }}
          >
            炫彩混色
          </button>
        </div>
        <div className="flex flex-wrap gap-2.5 pt-1 items-center">
          {LIGHT_COLORS.map((c) => {
            const selected = !isColorful && activeColor.toUpperCase() === c.toUpperCase();
            return (
              <button
                key={c}
                type="button"
                disabled={loading || !led || !supportsColor}
                onClick={() =>
                  void onPatch(
                    { [zone]: { colorful: false, color: c } },
                    isSide ? '侧翼灯效颜色' : '背光颜色',
                    zone,
                  )
                }
                className={`w-7 h-7 rounded-full cursor-pointer transition-transform disabled:opacity-35 ${
                  selected ? 'scale-110' : ''
                }`}
                style={{
                  background: c,
                  boxShadow: selected ? `0 0 0 2px var(--c-bg), 0 0 0 4px ${c}` : 'none',
                }}
                aria-label={`${isSide ? '选择侧翼颜色' : '选择颜色'} ${c}`}
              />
            );
          })}
          <input
            type="color"
            value={activeColor}
            disabled={loading || !led || !supportsColor}
            onChange={(e) => {
              const hex = e.target.value.toUpperCase();
              onOptimisticColor(hex);
              if (colorTimerRef.current) clearTimeout(colorTimerRef.current);
              colorTimerRef.current = setTimeout(() => {
                void onPatch(
                  { [zone]: { colorful: false, color: hex } },
                  isSide ? '侧翼自定义颜色' : '自定义颜色',
                  zone,
                );
              }, 120);
            }}
            className="w-7 h-7 rounded-full overflow-hidden border cursor-pointer bg-transparent disabled:opacity-35"
            style={{ borderColor: 'var(--c-border-strong)' }}
            aria-label={isSide ? '自定义侧翼 RGB 颜色' : '自定义 RGB 颜色'}
          />
        </div>
      </div>
    </>
  );
}
