import { useEffect, useState } from 'react';
import type { DeviceCapabilities, LedState, MiscState, SensorState } from '../../bridge/types';
import { PrecisionSlider, SegmentedTrack } from '../../ui/PrecisionControls';
import { ledModeDisplay } from './mouse-codec';
import { formatSleepLabel, MOUSE_LED_COLORS } from './mouse-utils';

export function MouseAdvancedTab({
  deviceKey,
  caps,
  loading,
  resetting,
  sensorState,
  miscState,
  ledState,
  debounceMin,
  debounceMax,
  sleepPresets,
  isDiscreteSleep,
  sleepMin,
  sleepMax,
  sleepStep,
  supportsFire,
  supportsScrollReverse,
  supportsReceiverLed,
  mouseLedModes,
  onSetSensor,
  onSetMisc,
  onSetLed,
  onRequestResetAdvanced,
}: {
  deviceKey: string;
  caps: DeviceCapabilities | undefined;
  loading: boolean;
  resetting: boolean;
  sensorState: SensorState | null;
  miscState: MiscState | null;
  ledState: LedState | null;
  debounceMin: number;
  debounceMax: number;
  sleepPresets: number[];
  isDiscreteSleep: boolean;
  sleepMin: number;
  sleepMax: number;
  sleepStep: number;
  supportsFire: boolean;
  supportsScrollReverse: boolean;
  supportsReceiverLed: boolean;
  mouseLedModes: NonNullable<DeviceCapabilities['lighting']>['mouseLed'];
  onSetSensor: (
    patch: Partial<{ lod: number; ripple: boolean; fixline: boolean; motion: boolean; mode: number }>,
    label: string,
  ) => Promise<void>;
  onSetMisc: (
    patch: Partial<{
      debounce: number;
      sleep: number;
      fireLoops: number;
      fireInterval: number;
      scrollReverse: boolean;
    }>,
    label: string,
  ) => Promise<void>;
  onSetLed: (mode: number, color?: string) => Promise<void>;
  onRequestResetAdvanced: () => void;
}) {
  const [liveDebounce, setLiveDebounce] = useState<number | null>(null);
  const [liveSleep, setLiveSleep] = useState<number | null>(null);
  const [liveFireInt, setLiveFireInt] = useState<number | null>(null);

  useEffect(() => {
    setLiveDebounce(null);
    setLiveSleep(null);
    setLiveFireInt(null);
  }, [deviceKey]);

  const sleepIdx =
    miscState?.sleep !== null && miscState?.sleep !== undefined
      ? sleepPresets.indexOf(miscState.sleep)
      : -1;

  const activeMouseLedObj = mouseLedModes.find((m) => m.code === ledState?.mode);
  const mouseLedSupportsColor = Boolean(activeMouseLedObj?.supportsColor);
  const activeMouseLedColor = ledState?.color ?? '#38BDF8';

  return (
    <div className="px-6 sm:px-8 lg:px-14 py-6 space-y-5 flex flex-col justify-center h-full">
      {/* Group 1: Sensor Algorithms & Micro-switch */}
      <div className="space-y-3">
        <div className="text-xs font-medium" style={{ color: 'var(--c-ink-faint)' }}>
          传感器算法与微动
        </div>

        {(
          [
            ['Motion Sync', '同步传感器采样与回报节拍', 'motion', sensorState?.motion ?? null, caps?.sensor?.motion ?? true],
            ['直线修正', '拉直水平与垂直移动轨迹，FPS 建议关闭', 'fixline', sensorState?.fixline ?? null, caps?.sensor?.fixline ?? true],
            ['波纹修正', '平滑高 DPI 下的微小追踪噪声', 'ripple', sensorState?.ripple ?? null, caps?.sensor?.ripple ?? true],
          ] as const
        )
          .filter(([, , , , supported]) => supported)
          .map(([title, sub, field, val]) => (
            <div key={field} className="flex items-center justify-between gap-4">
              <div className="flex items-baseline gap-2.5 min-w-0">
                <span className="text-xs font-medium shrink-0">{title}</span>
                <span className="text-[11px] truncate" style={{ color: 'var(--c-ink-faint)' }}>
                  {sub}
                </span>
              </div>
              <div className="flex items-center gap-2">
                {val === null && (
                  <span className="text-[11px] c-num" style={{ color: 'var(--c-ink-faint)' }}>
                    —
                  </span>
                )}
                <button
                  type="button"
                  role="switch"
                  aria-label={title}
                  aria-checked={Boolean(val)}
                  data-on={String(Boolean(val))}
                  disabled={val === null}
                  onClick={() => void onSetSensor({ [field]: !val }, title)}
                  className="hw-switch"
                />
              </div>
            </div>
          ))}

        <div className="pt-1 space-y-1.5">
          <div className="flex items-center justify-between text-xs">
            <div className="flex items-baseline gap-2.5">
              <span className="font-medium">按键消抖</span>
              <span className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                减少微动抖动造成的双击（{debounceMin}–{debounceMax} ms）
              </span>
            </div>
            <span className="c-num text-sm font-semibold">
              {liveDebounce !== null
                ? `${liveDebounce} ms`
                : miscState?.debounce !== null && miscState?.debounce !== undefined
                  ? `${miscState.debounce} ms`
                  : '—'}
            </span>
          </div>
          <PrecisionSlider
            ariaLabel="按键消抖"
            pct={
              ((liveDebounce ?? miscState?.debounce ?? debounceMin) - debounceMin) /
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
              void onSetMisc({ debounce: v }, '按键消抖');
            }}
          />
        </div>
      </div>

      {/* Group 2: Power, Scroll & Rapid Fire */}
      <div
        className="border-t pt-5 space-y-4"
        style={{ borderColor: 'var(--c-border)' }}
      >
        <div className="text-xs font-medium" style={{ color: 'var(--c-ink-faint)' }}>
          电源{supportsFire ? '与连发' : ''}
        </div>

        {isDiscreteSleep ? (
          <div className="flex items-center justify-between gap-4">
            <span className="text-xs font-medium shrink-0">自动休眠</span>
            <SegmentedTrack
              labels={sleepPresets.map(formatSleepLabel)}
              activeIndex={sleepIdx}
              disabled={loading}
              widthClass="flex-1 max-w-xs"
              itemPaddingClass="py-1"
              onCommit={(idx) => {
                const sec = sleepPresets[idx];
                if (sec !== undefined && sec !== miscState?.sleep) {
                  void onSetMisc({ sleep: sec }, '自动休眠');
                }
              }}
            />
          </div>
        ) : (
          <div className="space-y-2">
            <div className="flex items-center justify-between gap-4">
              <div className="flex items-baseline gap-2.5">
                <span className="text-xs font-medium shrink-0">自动休眠</span>
                <span className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                  {sleepMin}–{sleepMax}s，支持常用档或滑条微调
                </span>
              </div>
              <span className="c-num text-sm font-semibold">
                {liveSleep !== null
                  ? formatSleepLabel(liveSleep)
                  : miscState?.sleep !== null && miscState?.sleep !== undefined
                    ? formatSleepLabel(miscState.sleep)
                    : '—'}
              </span>
            </div>
            <PrecisionSlider
              ariaLabel="自动休眠时间"
              pct={
                ((liveSleep ?? miscState?.sleep ?? sleepMin) - sleepMin) /
                Math.max(1, sleepMax - sleepMin) *
                100
              }
              compact
              disabled={loading}
              onMove={(f) => {
                const raw = sleepMin + f * (sleepMax - sleepMin);
                const snapped = Math.max(
                  sleepMin,
                  Math.min(sleepMax, Math.round(raw / sleepStep) * sleepStep),
                );
                setLiveSleep(snapped);
              }}
              onCommit={(f) => {
                const raw = sleepMin + f * (sleepMax - sleepMin);
                const snapped = Math.max(
                  sleepMin,
                  Math.min(sleepMax, Math.round(raw / sleepStep) * sleepStep),
                );
                setLiveSleep(null);
                void onSetMisc({ sleep: snapped }, '自动休眠');
              }}
            />
            <SegmentedTrack
              labels={sleepPresets.map(formatSleepLabel)}
              activeIndex={sleepIdx}
              disabled={loading}
              itemPaddingClass="py-1"
              onCommit={(idx) => {
                const sec = sleepPresets[idx];
                if (sec !== undefined && sec !== miscState?.sleep) {
                  void onSetMisc({ sleep: sec }, '自动休眠');
                }
              }}
            />
          </div>
        )}

        {supportsScrollReverse && (
          <div className="flex items-center justify-between gap-4">
            <span className="text-xs font-medium shrink-0">滚轮反向</span>
            <button
              type="button"
              role="switch"
              aria-label="滚轮反向"
              aria-checked={Boolean(miscState?.scrollReverse)}
              data-on={String(Boolean(miscState?.scrollReverse))}
              disabled={miscState?.scrollReverse == null}
              onClick={() =>
                void onSetMisc(
                  { scrollReverse: !miscState?.scrollReverse },
                  '滚轮反向',
                )
              }
              className="hw-switch"
            />
          </div>
        )}

        {supportsFire && (
          <>
            <div className="flex items-center justify-between gap-4">
              <div className="flex items-baseline gap-2 min-w-0">
                <span className="text-xs font-medium shrink-0">连发次数</span>
                <span className="text-[11px] truncate" style={{ color: 'var(--c-ink-faint)' }}>
                  需先在按键映射中绑定「火力连发」
                </span>
              </div>
              <SegmentedTrack
                labels={['按住持续', '1 次', '2 次', '3 次']}
                activeIndex={miscState?.fireLoops ?? -1}
                disabled={loading}
                widthClass="w-52"
                itemPaddingClass="py-1"
                textClass="text-[11px]"
                onCommit={(idx) => {
                  if (idx !== miscState?.fireLoops) {
                    void onSetMisc({ fireLoops: idx }, '连发次数');
                  }
                }}
              />
            </div>

            <div className="space-y-1.5">
              <div className="flex items-center justify-between text-xs">
                <span className="font-medium">连发间隔</span>
                <span className="c-num text-sm font-semibold">
                  {liveFireInt !== null
                    ? `${liveFireInt} ms`
                    : miscState?.fireInterval !== null &&
                        miscState?.fireInterval !== undefined
                      ? `${miscState.fireInterval} ms`
                      : '—'}
                </span>
              </div>
              <PrecisionSlider
                ariaLabel="连发间隔"
                pct={((liveFireInt ?? miscState?.fireInterval ?? 0) / 255) * 100}
                compact
                disabled={loading}
                onMove={(f) => setLiveFireInt(Math.round(f * 255))}
                onCommit={(f) => {
                  const v = Math.round(f * 255);
                  setLiveFireInt(null);
                  void onSetMisc({ fireInterval: v }, '连发间隔');
                }}
              />
            </div>
          </>
        )}
      </div>

      {/* Group 3: Lighting & Reset */}
      <div
        className="border-t pt-4 flex flex-wrap items-center justify-between gap-4"
        style={{ borderColor: 'var(--c-border)' }}
      >
        {supportsReceiverLed && (
          <div className="flex items-center gap-3 flex-wrap">
            <span className="text-xs font-medium">8K 接收器灯效</span>
            {ledState?.supported ? (
              <SegmentedTrack
                labels={[0, 1, 2].map(ledModeDisplay)}
                activeIndex={ledState.mode ?? -1}
                disabled={loading}
                widthClass="w-64"
                itemPaddingClass="py-1"
                textClass="text-[11px]"
                onCommit={(idx) => {
                  if (idx !== ledState.mode) {
                    void onSetLed(idx);
                  }
                }}
              />
            ) : (
              <span className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                仅 8K 无线连接可用
              </span>
            )}
          </div>
        )}

        {mouseLedModes.length > 0 && (
          <div className="flex items-center gap-2.5 flex-wrap">
            <span className="text-xs font-medium">机身灯效</span>
            <div className="flex flex-wrap gap-1.5">
              {mouseLedModes.map((m) => {
                const active = ledState?.mode === m.code;
                return (
                  <button
                    key={m.code}
                    type="button"
                    disabled={loading}
                    onClick={() => void onSetLed(m.code)}
                    className="px-2.5 py-1 rounded-md text-[11px] font-medium cursor-pointer disabled:opacity-45"
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
            {mouseLedSupportsColor && activeMouseLedObj && (
              <div className="flex items-center gap-1.5 pl-1">
                {MOUSE_LED_COLORS.map((c) => {
                  const sel = activeMouseLedColor.toUpperCase() === c.toUpperCase();
                  return (
                    <button
                      key={c}
                      type="button"
                      disabled={loading}
                      onClick={() => void onSetLed(activeMouseLedObj.code, c)}
                      className={`w-5 h-5 rounded-full cursor-pointer disabled:opacity-40 ${
                        sel ? 'scale-110' : ''
                      }`}
                      style={{
                        background: c,
                        boxShadow: sel ? `0 0 0 1.5px var(--c-bg), 0 0 0 3px ${c}` : 'none',
                      }}
                      aria-label={`选择灯效颜色 ${c}`}
                    />
                  );
                })}
                <input
                  type="color"
                  value={activeMouseLedColor}
                  disabled={loading}
                  onChange={(e) =>
                    void onSetLed(activeMouseLedObj.code, e.target.value.toUpperCase())
                  }
                  className="w-5 h-5 rounded-full overflow-hidden border cursor-pointer bg-transparent disabled:opacity-40"
                  style={{ borderColor: 'var(--c-border-strong)' }}
                  aria-label="自定义鼠标 RGB 颜色"
                />
              </div>
            )}
          </div>
        )}

        <button
          type="button"
          disabled={loading || resetting}
          onClick={onRequestResetAdvanced}
          className="px-3.5 py-1.5 rounded-lg text-xs font-medium cursor-pointer transition-colors shrink-0 bg-[var(--c-elev)] hover:bg-[var(--c-hover)] border border-[var(--c-border-strong)] text-[var(--c-ink)] disabled:opacity-45 ml-auto"
        >
          恢复默认
        </button>
      </div>
    </div>
  );
}
