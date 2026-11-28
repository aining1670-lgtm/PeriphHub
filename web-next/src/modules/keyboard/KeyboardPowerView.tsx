import { type CSSProperties, useEffect, useState } from 'react';
import type { DeviceEntry, KbdState } from '../../bridge/types';
import {
  formatBatteryText,
  getBatteryTone,
  HardwareSilhouette,
  toneCssColor,
} from '../../ui/DeviceGlyph';
import { PrecisionSlider, SegmentedTrack } from '../../ui/PrecisionControls';
import { deviceHeroTransitionName } from '../../ui/view-transition';
import { formatMinutesLabel } from './kbd-state-helpers';

export function KeyboardPowerView({
  device,
  isHeroTransition,
  loading,
  kbdState,
  supportedOptions,
  debounceMin,
  debounceMax,
  shallowPresets,
  deepPresets,
  onPatch,
}: {
  device: DeviceEntry;
  isHeroTransition: boolean;
  loading: boolean;
  kbdState: KbdState | null;
  supportedOptions: ReadonlyArray<string> | undefined;
  debounceMin: number;
  debounceMax: number;
  shallowPresets: number[];
  deepPresets: number[];
  onPatch: (
    patch: Record<string, unknown>,
    label: string,
    fieldKey: string,
  ) => Promise<KbdState | null>;
}) {
  const [liveDebounce, setLiveDebounce] = useState<number | null>(null);

  useEffect(() => {
    setLiveDebounce(null);
  }, [device.key]);

  const battText = formatBatteryText(device);
  const battColor = toneCssColor(getBatteryTone(device));
  const opt = kbdState?.options ?? null;
  const sleep = kbdState?.sleep ?? null;
  const curDebounce = liveDebounce ?? kbdState?.debounce ?? null;

  return (
    <div className="relative z-10 w-full max-w-[1360px] mx-auto flex-1 grid grid-cols-1 md:grid-cols-12 items-center">
      {/* Left 50/50 Hardware Stage */}
      <section className="md:col-span-5 relative flex flex-col items-center justify-center py-10 px-6">
        <div className="relative w-[320px] sm:w-[360px] h-[260px] sm:h-[290px] flex items-center justify-center">
          <svg
            viewBox="0 0 360 360"
            aria-hidden="true"
            className="pointer-events-none absolute inset-0 w-full h-full opacity-45"
          >
            <circle cx="180" cy="180" r="118" fill="none" stroke="var(--c-border)" strokeWidth="1" />
            <circle
              cx="180"
              cy="180"
              r="164"
              fill="none"
              stroke="var(--c-border)"
              strokeWidth="1"
              strokeDasharray="3 7"
            />
          </svg>

          <div className="relative z-10 flex flex-col items-center">
            <div
              style={
                isHeroTransition
                  ? ({
                      viewTransitionName: deviceHeroTransitionName(device.key),
                      viewTransitionClass: 'device-hero-vt',
                    } as CSSProperties)
                  : undefined
              }
            >
              <HardwareSilhouette kind="keyboard" active={device.connected} size="stage" />
            </div>
            <div className="c-contact-shadow h-3.5 w-[240px] rounded-full mt-5" />
          </div>
        </div>

        <div
          className="mt-4 flex items-center gap-6 text-xs c-num"
          style={{ color: 'var(--c-ink-muted)' }}
        >
          <span>
            <span style={{ color: 'var(--c-ink-faint)' }}>电量 </span>
            <strong className="font-semibold" style={{ color: battColor }}>
              {battText}
            </strong>
          </span>
          <span className="w-px h-3.5" style={{ background: 'var(--c-border-strong)' }} />
          <span className="font-medium" style={{ color: 'var(--c-ink)' }}>
            {kbdState?.profile !== null && kbdState?.profile !== undefined
              ? `Profile ${kbdState.profile + 1}`
              : 'Profile —'}
          </span>
          <span className="w-px h-3.5" style={{ background: 'var(--c-border-strong)' }} />
          <span className="font-medium" style={{ color: 'var(--c-ink)' }}>
            {opt ? (opt.macMode ? 'macOS' : 'Windows') : '—'}
          </span>
        </div>
      </section>

      {/* Right 50/50 Borderless Advanced & Power Controls */}
      <section className="md:col-span-7 px-6 sm:px-8 lg:px-14 py-6 space-y-6 tab-enter">
        <div className="space-y-4">
          <div className="text-xs font-medium" style={{ color: 'var(--c-ink-faint)' }}>
            按键行为与微动去抖
          </div>

          {(
            [
              ['macMode', 'macOS 键位模式', '互换 Alt 与 Win/Option 修饰键映射', opt?.macMode ?? null],
              ['winLock', '锁定 Win 键 (WinLock)', '游戏时防止误按弹出开始菜单', opt?.winLock ?? null],
              ['wasdSwap', 'WASD 与方向键互换', '将 W/A/S/D 与 ↑/←/↓/→ 物理位置互换', opt?.wasdSwap ?? null],
            ] as const
          )
            .filter(([field]) => !supportedOptions?.length || supportedOptions.includes(field))
            .map(([field, title, sub, val]) => (
              <div key={field} className="flex items-center justify-between gap-4">
                <div className="flex items-baseline gap-2.5 min-w-0">
                  <span className="text-xs font-medium shrink-0">{title}</span>
                  <span className="text-[11px] truncate" style={{ color: 'var(--c-ink-faint)' }}>
                    {sub}
                  </span>
                </div>
                <button
                  type="button"
                  role="switch"
                  aria-label={title}
                  aria-checked={Boolean(val)}
                  data-on={String(Boolean(val))}
                  disabled={loading || val === null}
                  onClick={() => void onPatch({ [field]: !val }, title, `options.${field}`)}
                  className="hw-switch"
                />
              </div>
            ))}

          <div className="pt-1 space-y-2">
            <div className="flex items-center justify-between text-xs">
              <div className="flex items-baseline gap-2.5">
                <span className="font-medium">按键消抖</span>
                <span className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                  {debounceMin}–{debounceMax} ms，越小响应越快
                </span>
              </div>
              <span className="c-num text-sm font-semibold">
                {curDebounce !== null ? `${curDebounce} ms` : '—'}
              </span>
            </div>
            <PrecisionSlider
              ariaLabel="键盘按键消抖"
              pct={
                ((curDebounce ?? debounceMin) - debounceMin) /
                Math.max(1, debounceMax - debounceMin) *
                100
              }
              compact
              disabled={loading}
              onMove={(f) =>
                setLiveDebounce(Math.round(debounceMin + f * (debounceMax - debounceMin)))
              }
              onCommit={(f) => {
                const v = Math.round(debounceMin + f * (debounceMax - debounceMin));
                setLiveDebounce(null);
                void onPatch({ debounce: v }, '按键去抖', 'debounce');
              }}
            />
          </div>
        </div>

        <div className="border-t pt-5 space-y-4" style={{ borderColor: 'var(--c-border)' }}>
          <div className="text-xs font-medium" style={{ color: 'var(--c-ink-faint)' }}>
            无线休眠策略
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 gap-5">
            {(
              [
                ['sleep24g', '2.4 GHz 浅休眠', shallowPresets, sleep?.sleep24g ?? null],
                ['deep24g', '2.4 GHz 深休眠', deepPresets, sleep?.deep24g ?? null],
                ['sleepBt', '蓝牙浅休眠', shallowPresets, sleep?.sleepBt ?? null],
                ['deepBt', '蓝牙深休眠', deepPresets, sleep?.deepBt ?? null],
              ] as const
            ).map(([field, label, presets, val]) => (
              <div key={field} className="space-y-1.5">
                <div className="flex items-center justify-between text-xs">
                  <span className="font-medium">{label}</span>
                  <span className="c-num" style={{ color: 'var(--c-ink-muted)' }}>
                    {val !== null ? formatMinutesLabel(val) : '—'}
                  </span>
                </div>
                <SegmentedTrack
                  labels={presets.map(formatMinutesLabel)}
                  activeIndex={val !== null ? presets.indexOf(val) : -1}
                  disabled={loading || !sleep}
                  itemPaddingClass="py-1"
                  onCommit={(idx) => {
                    const sec = presets[idx];
                    if (sec !== undefined && sec !== val && sleep) {
                      void onPatch(
                        { sleep: { ...sleep, [field]: sec } },
                        label,
                        `sleep.${field}`,
                      );
                    }
                  }}
                />
              </div>
            ))}
          </div>
        </div>
      </section>
    </div>
  );
}
