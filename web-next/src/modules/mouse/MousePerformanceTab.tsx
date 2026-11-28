import { useEffect, useRef, useState } from 'react';
import type { DeviceCapabilities, DpiState, SensorState } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import { hexToRgba, PrecisionSlider, SegmentedTrack } from '../../ui/PrecisionControls';
import {
  lodLabelToWire,
  lodWireToLabel,
  sensorModeDisplay,
  sensorModeToWire,
  sensorModeWireToKey,
} from './mouse-codec';
import { dpiToSlider, sliderToDpi, STAGE_COLORS } from './mouse-utils';

export function MousePerformanceTab({
  deviceKey,
  driver,
  caps,
  loading,
  dpiState,
  stageIdx,
  maxStages,
  dpiMin,
  dpiMax,
  dpiStep,
  dpiAnchors,
  supportsXySplit,
  xySplit,
  onToggleXySplit,
  activeAxis,
  onSelectAxis,
  stageXVal,
  stageYVal,
  activeStageColor,
  rateOptions,
  pollingHz,
  lodOptions,
  sensorModes,
  sensorState,
  onCommitDpi,
  onStageSelect,
  onStageCountChange,
  onSetRate,
  onSetSensor,
}: {
  deviceKey: string;
  driver: string;
  caps: DeviceCapabilities | undefined;
  loading: boolean;
  dpiState: DpiState | null;
  stageIdx: number;
  maxStages: number;
  dpiMin: number;
  dpiMax: number;
  dpiStep: number;
  dpiAnchors: number[];
  supportsXySplit: boolean;
  xySplit: boolean;
  onToggleXySplit: () => void;
  activeAxis: 'x' | 'y';
  onSelectAxis: (axis: 'x' | 'y') => void;
  stageXVal: number | null;
  stageYVal: number | null;
  activeStageColor: string;
  rateOptions: number[];
  pollingHz: number | null;
  lodOptions: string[];
  sensorModes: string[];
  sensorState: SensorState | null;
  onCommitDpi: (dpiVal: number) => Promise<void>;
  onStageSelect: (idx: number) => Promise<void>;
  onStageCountChange: (cnt: number) => Promise<void>;
  onSetRate: (hz: number) => Promise<void>;
  onSetSensor: (
    patch: Partial<{ lod: number; ripple: boolean; fixline: boolean; motion: boolean; mode: number }>,
    label: string,
  ) => Promise<void>;
}) {
  const pushToast = useHubStore((s) => s.pushToast);
  const [liveDpiPreview, setLiveDpiPreview] = useState<number | null>(null);
  const [editingDpi, setEditingDpi] = useState(false);
  const [dpiInputDraft, setDpiInputDraft] = useState('');
  const cancelDpiEditRef = useRef(false);

  useEffect(() => {
    setLiveDpiPreview(null);
    setEditingDpi(false);
  }, [deviceKey]);

  const currentLevel = dpiState?.levels?.[stageIdx] ?? null;
  const displayedAxisVal =
    liveDpiPreview !== null ? liveDpiPreview : activeAxis === 'x' ? stageXVal : stageYVal;
  const sliderPct =
    displayedAxisVal !== null ? dpiToSlider(displayedAxisVal, dpiAnchors) / 10 : 0;

  const hzIdx = pollingHz !== null ? rateOptions.indexOf(pollingHz) : -1;
  const isHighHz = pollingHz !== null && pollingHz >= 4000;
  const warnFromIdx = rateOptions.findIndex((r) => r >= 4000);

  const curLodLabel = lodWireToLabel(sensorState?.lod ?? null);
  const lodIdx = curLodLabel ? lodOptions.indexOf(curLodLabel) : -1;

  const curSensorModeKey = sensorModeWireToKey(sensorState?.mode ?? null);
  const sensorModeIdx = curSensorModeKey ? sensorModes.indexOf(curSensorModeKey) : -1;

  const commitDpiDraft = () => {
    if (cancelDpiEditRef.current) {
      cancelDpiEditRef.current = false;
      setEditingDpi(false);
      return;
    }
    cancelDpiEditRef.current = true;
    const trimmed = dpiInputDraft.trim();
    const num = Number(trimmed);
    setEditingDpi(false);
    if (!trimmed || !Number.isInteger(num) || num < dpiMin || num > dpiMax || num % dpiStep !== 0) {
      pushToast('error', `DPI 必须为 ${dpiMin}–${dpiMax} 且为 ${dpiStep} 的倍数`);
      return;
    }
    void onCommitDpi(num);
  };

  return (
    <div className="px-6 sm:px-8 lg:px-14 py-6 space-y-6 flex flex-col justify-center h-full">
      {/* HERO DPI INSTRUMENT */}
      <div className="space-y-4">
        <div className="flex items-end justify-between gap-4">
          <div>
            <div
              className="text-xs flex items-center gap-2"
              style={{ color: 'var(--c-ink-muted)' }}
            >
              <span
                className="w-2.5 h-2.5 rounded-full"
                style={{
                  background: activeStageColor,
                  boxShadow: `0 0 8px ${activeStageColor}`,
                }}
              />
              <span className="font-medium">档位 {stageIdx + 1} 灵敏度</span>
              {supportsXySplit && xySplit && (
                <span className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                  · 点击 X / Y 切换滑轨控制轴
                </span>
              )}
            </div>

            {supportsXySplit && xySplit ? (
              <div className="flex items-center gap-3 mt-2">
                <button
                  type="button"
                  onClick={() => onSelectAxis('x')}
                  className="px-3.5 py-2 rounded-xl text-left cursor-pointer transition-[border-color,background-color,box-shadow]"
                  style={{
                    background: activeAxis === 'x' ? 'var(--c-elev-2)' : 'transparent',
                    border: `1.5px solid ${activeAxis === 'x' ? activeStageColor : 'var(--c-border)'}`,
                  }}
                >
                  <div className="flex items-center gap-1.5 text-[11px]">
                    <span
                      className="font-semibold"
                      style={{
                        color: activeAxis === 'x' ? activeStageColor : 'var(--c-ink-faint)',
                      }}
                    >
                      X 轴
                    </span>
                    {(currentLevel?.unverified?.includes('x') || stageXVal === null) && (
                      <span
                        className="text-[10px] px-1 rounded"
                        style={{ background: 'var(--c-warn-soft)', color: 'var(--c-warn)' }}
                        title="硬件不支持独立回读 X 轴"
                      >
                        无法回读
                      </span>
                    )}
                  </div>
                  <div className="c-display-num text-3xl font-semibold leading-tight mt-0.5">
                    {activeAxis === 'x' && liveDpiPreview !== null
                      ? liveDpiPreview
                      : (stageXVal ?? '—')}
                  </div>
                </button>

                <button
                  type="button"
                  onClick={() => onSelectAxis('y')}
                  className="px-3.5 py-2 rounded-xl text-left cursor-pointer transition-[border-color,background-color,box-shadow]"
                  style={{
                    background: activeAxis === 'y' ? 'var(--c-elev-2)' : 'transparent',
                    border: `1.5px solid ${activeAxis === 'y' ? activeStageColor : 'var(--c-border)'}`,
                  }}
                >
                  <div
                    className="text-[11px] font-semibold"
                    style={{
                      color: activeAxis === 'y' ? activeStageColor : 'var(--c-ink-faint)',
                    }}
                  >
                    Y 轴
                  </div>
                  <div className="c-display-num text-3xl font-semibold leading-tight mt-0.5">
                    {activeAxis === 'y' && liveDpiPreview !== null
                      ? liveDpiPreview
                      : (stageYVal ?? '—')}
                  </div>
                </button>
              </div>
            ) : editingDpi ? (
              <form
                className="flex items-baseline gap-2.5 mt-1"
                onSubmit={(e) => {
                  e.preventDefault();
                  commitDpiDraft();
                }}
              >
                <input
                  type="number"
                  autoFocus
                  min={dpiMin}
                  max={dpiMax}
                  step={dpiStep}
                  value={dpiInputDraft}
                  onChange={(e) => setDpiInputDraft(e.target.value)}
                  onBlur={commitDpiDraft}
                  onKeyDown={(e) => {
                    if (e.key === 'Escape') {
                      cancelDpiEditRef.current = true;
                      setEditingDpi(false);
                    }
                  }}
                  className="c-display-num text-4xl sm:text-5xl font-semibold leading-none w-44 bg-transparent border-b-2"
                  style={{ borderColor: activeStageColor, color: 'var(--c-ink)' }}
                  aria-label="输入精确 DPI 数值"
                />
                <span className="text-sm font-medium" style={{ color: 'var(--c-ink-faint)' }}>
                  Enter 确认
                </span>
              </form>
            ) : loading && !dpiState ? (
              <div className="flex items-baseline gap-2.5 mt-1" aria-hidden="true">
                <span className="c-skeleton inline-block h-12 sm:h-14 w-36 rounded-lg" />
                <span className="text-sm font-medium" style={{ color: 'var(--c-ink-faint)' }}>
                  DPI
                </span>
              </div>
            ) : (
              <button
                type="button"
                disabled={loading}
                onClick={() => {
                  cancelDpiEditRef.current = false;
                  setDpiInputDraft(String(displayedAxisVal ?? ''));
                  setEditingDpi(true);
                }}
                className="flex items-baseline gap-2.5 mt-1 cursor-text hover:opacity-80 transition-opacity text-left"
                title="点击输入精确 DPI 数值"
              >
                <span className="c-display-num text-5xl sm:text-6xl font-semibold leading-none">
                  {displayedAxisVal ?? '—'}
                </span>
                <span className="text-sm font-medium" style={{ color: 'var(--c-ink-faint)' }}>
                  DPI
                </span>
              </button>
            )}
          </div>

          {supportsXySplit && (
            <div className="flex items-center gap-3 pb-1.5 shrink-0">
              <span className="text-xs" style={{ color: 'var(--c-ink-muted)' }}>
                X/Y 独立
              </span>
              <button
                type="button"
                role="switch"
                aria-label="X/Y 独立灵敏度"
                aria-checked={xySplit}
                data-on={String(xySplit)}
                onClick={onToggleXySplit}
                className="hw-switch"
              />
            </div>
          )}
        </div>

        {/* Piecewise DPI Slider */}
        <div className="pt-1 space-y-1.5">
          <PrecisionSlider
            ariaLabel="DPI 灵敏度"
            pct={sliderPct}
            color={activeStageColor}
            disabled={loading}
            onMove={(f) =>
              setLiveDpiPreview(sliderToDpi(Math.round(f * 1000), dpiAnchors, dpiStep))
            }
            onCommit={(f) => {
              setLiveDpiPreview(null);
              void onCommitDpi(sliderToDpi(Math.round(f * 1000), dpiAnchors, dpiStep));
            }}
          />
          <div className="relative h-4 text-[11px] c-num" style={{ color: 'var(--c-ink-faint)' }}>
            {dpiAnchors.map((val, i) => {
              const leftPct = (i / (dpiAnchors.length - 1)) * 100;
              const align =
                i === 0
                  ? 'translateX(0)'
                  : i === dpiAnchors.length - 1
                    ? 'translateX(-100%)'
                    : 'translateX(-50%)';
              return (
                <button
                  key={val}
                  type="button"
                  disabled={loading}
                  onClick={() => {
                    setLiveDpiPreview(null);
                    void onCommitDpi(val);
                  }}
                  className="absolute top-0 cursor-pointer hover:text-[var(--c-ink)] transition-colors disabled:opacity-45"
                  style={{
                    left: `${leftPct}%`,
                    transform: align,
                    color: displayedAxisVal === val ? 'var(--c-ink)' : 'var(--c-ink-faint)',
                    fontWeight: displayedAxisVal === val ? 600 : 400,
                  }}
                >
                  {val}
                </button>
              );
            })}
          </div>
        </div>

        {/* Stage Selector + Active Stage Count */}
        <div className="space-y-2 pt-1">
          <div className="grid grid-cols-6 gap-2">
            {Array.from({ length: maxStages }).map((_, idx) => {
              const color = STAGE_COLORS[idx % STAGE_COLORS.length];
              const lvl = dpiState?.levels?.[idx] ?? null;
              const act = idx === stageIdx;
              const en = idx < (dpiState?.count ?? 0);
              return (
                <button
                  key={idx}
                  type="button"
                  disabled={loading}
                  onClick={() => void onStageSelect(idx)}
                  className="relative py-2.5 px-2 rounded-xl text-center cursor-pointer transition-[border-color,background-color,box-shadow] overflow-hidden disabled:opacity-50"
                  style={{
                    background: act
                      ? hexToRgba(color, 0.14)
                      : en
                        ? 'var(--c-elev)'
                        : 'transparent',
                    border: act
                      ? `1.5px solid ${color}`
                      : en
                        ? '1px solid var(--c-border-strong)'
                        : '1px dashed var(--c-border)',
                    boxShadow: act ? `0 8px 22px -8px ${hexToRgba(color, 0.55)}` : 'none',
                    opacity: en ? 1 : 0.45,
                  }}
                >
                  <div
                    className="absolute top-0 left-0 right-0 h-[3px]"
                    style={{ background: color, opacity: act ? 1 : en ? 0.55 : 0.25 }}
                  />
                  <div className="flex items-center justify-between text-[10px] mb-1 px-0.5">
                    <span
                      style={{
                        color: act ? color : 'var(--c-ink-faint)',
                        fontWeight: act ? 700 : 500,
                      }}
                    >
                      S{idx + 1}
                    </span>
                    {act ? (
                      <span
                        className="w-1.5 h-1.5 rounded-full"
                        style={{ background: color, boxShadow: `0 0 6px ${color}` }}
                      />
                    ) : (
                      <span className="text-[9px]" style={{ color: 'var(--c-ink-faint)' }}>
                        {en ? '' : '关'}
                      </span>
                    )}
                  </div>
                  <div
                    className={`c-num ${act ? 'text-sm font-bold' : 'text-xs font-semibold'}`}
                    style={{ color: act ? 'var(--c-ink)' : 'var(--c-ink-muted)' }}
                  >
                    {lvl?.y ?? '—'}
                  </div>
                </button>
              );
            })}
          </div>

          {caps?.dpi?.stageCount && dpiState && (
            <div className="flex items-center justify-between text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
              <span>启用档位数：{dpiState.count} 档</span>
              <div className="flex items-center gap-1">
                {Array.from({ length: maxStages }, (_, n) => n + 1).map((cnt) => (
                  <button
                    key={cnt}
                    type="button"
                    disabled={loading}
                    onClick={() => void onStageCountChange(cnt)}
                    className="w-5 h-5 rounded text-[10px] c-num cursor-pointer"
                    style={{
                      background: dpiState.count === cnt ? 'var(--c-hi)' : 'var(--c-elev)',
                      color: dpiState.count === cnt ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                    }}
                  >
                    {cnt}
                  </button>
                ))}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* POLLING RATE */}
      <div className="border-t pt-6 space-y-3" style={{ borderColor: 'var(--c-border)' }}>
        <div className="flex items-center justify-between text-xs">
          <div className="flex items-baseline gap-2.5">
            <span className="font-medium" style={{ color: 'var(--c-ink-muted)' }}>
              无线回报率
            </span>
            <span
              className="c-num text-sm font-semibold"
              style={{ color: isHighHz ? 'var(--c-warn)' : 'var(--c-ink)' }}
            >
              {pollingHz !== null
                ? `${pollingHz >= 1000 ? `${pollingHz / 1000}K` : pollingHz} Hz`
                : '—'}
            </span>
          </div>
          <span
            className="c-num"
            style={{ color: isHighHz ? 'var(--c-warn)' : 'var(--c-ink-faint)' }}
          >
            {pollingHz === null ? '未读取' : isHighHz ? '高功耗竞技档' : '均衡续航'}
          </span>
        </div>

        <SegmentedTrack
          labels={rateOptions.map((hz) => (hz >= 1000 ? `${hz / 1000}K` : String(hz)))}
          activeIndex={hzIdx}
          warnFromIndex={warnFromIdx >= 0 ? warnFromIdx : undefined}
          disabled={loading}
          onCommit={(idx) => {
            const newHz = rateOptions[idx];
            if (newHz !== undefined && newHz !== pollingHz) {
              void onSetRate(newHz);
            }
          }}
        />
      </div>

      {/* SENSOR PAIR */}
      <div className="border-t pt-6" style={{ borderColor: 'var(--c-border)' }}>
        <div
          className={`grid grid-cols-1 ${
            sensorModes.length > 0 ? 'sm:grid-cols-2' : ''
          } gap-6`}
        >
          {sensorModes.length > 0 && (
            <div className="flex items-center justify-between gap-3">
              <div className="min-w-0">
                <div className="text-xs font-medium">传感器模式</div>
                <div className="text-[11px] truncate" style={{ color: 'var(--c-ink-faint)' }}>
                  2K+ 建议竞技
                </div>
              </div>
              <SegmentedTrack
                labels={sensorModes.map(sensorModeDisplay)}
                activeIndex={sensorModeIdx}
                disabled={loading}
                widthClass={sensorModes.length >= 3 ? 'w-36' : 'w-28'}
                itemPaddingClass="py-1"
                onCommit={(idx) => {
                  const modeKey = sensorModes[idx];
                  const wire = modeKey ? sensorModeToWire(modeKey) : null;
                  if (wire !== null && wire !== sensorState?.mode) {
                    void onSetSensor({ mode: wire }, '传感器模式');
                  }
                }}
              />
            </div>
          )}

          <div className="flex items-center justify-between gap-3">
            <div className="min-w-0">
              <div className="text-xs font-medium">抬升高度 (LOD)</div>
              <div className="text-[11px] truncate" style={{ color: 'var(--c-ink-faint)' }}>
                抬鼠停追高度
              </div>
            </div>
            <SegmentedTrack
              labels={lodOptions.map((v) => `${v}mm`)}
              activeIndex={lodIdx}
              disabled={loading}
              widthClass={lodOptions.length >= 3 ? 'w-36' : 'w-28'}
              itemPaddingClass="py-1"
              onCommit={(idx) => {
                const label = lodOptions[idx];
                const wire = label ? lodLabelToWire(label, driver) : null;
                if (wire !== null && wire !== sensorState?.lod) {
                  void onSetSensor({ lod: wire }, '抬升高度');
                }
              }}
            />
          </div>
        </div>
      </div>
    </div>
  );
}
