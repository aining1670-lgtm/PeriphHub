import { type CSSProperties, useEffect, useState } from 'react';
import { BatteryCharging, PlugZap, RefreshCw, RotateCcw } from 'lucide-react';
import type { DeviceEntry } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import {
  formatBatteryText,
  getBatteryTone,
  HardwareSilhouette,
  toneCssColor,
} from '../../ui/DeviceGlyph';
import { deviceHeroTransitionName } from '../../ui/view-transition';

const GAUGE_SEGMENTS = 24;

function getPowerLabel(device: DeviceEntry): string {
  if (!device.connected) return '未连接';
  if (device.mode === 'wired') return '有线直供';
  if (device.asleep) return '浅休眠';
  if (device.charging === 2) return '充电中';
  if (device.battery < 0) return '等待上报';
  return '电池供电';
}

function formatConnectionMode(mode: string): string {
  switch (mode) {
    case 'bluetooth':
      return 'Bluetooth 无线';
    case 'wireless':
      return '2.4 GHz / 无线';
    case 'wired':
      return 'USB 有线';
    default:
      return '标准通道';
  }
}

function getAmbientGlowColor(tone: ReturnType<typeof getBatteryTone>): string {
  switch (tone) {
    case 'ok':
      return 'var(--c-ok-soft)';
    case 'warn':
      return 'var(--c-warn-soft)';
    case 'danger':
      return 'var(--c-danger-soft)';
    default:
      return 'var(--c-accent-soft)';
  }
}

function SegmentedEnergyGauge({
  device,
  threshold,
  toneColor,
}: {
  device: DeviceEntry;
  threshold: number;
  toneColor: string;
}) {
  const isWired = device.connected && device.mode === 'wired';
  const hasPct = device.connected && !isWired && device.battery >= 0;
  const pct = isWired ? 100 : hasPct ? Math.min(100, Math.max(0, device.battery)) : 0;
  const filledCount = Math.round((pct / 100) * GAUGE_SEGMENTS);
  const clampedThreshold = Math.min(50, Math.max(5, threshold));

  return (
    <div className="space-y-2.5">
      <div
        className="grid gap-1.5"
        style={{ gridTemplateColumns: `repeat(${GAUGE_SEGMENTS}, minmax(0, 1fr))` }}
        role="meter"
        aria-label="设备电量刻度"
        aria-valuemin={0}
        aria-valuemax={100}
        aria-valuenow={hasPct ? pct : undefined}
      >
        {Array.from({ length: GAUGE_SEGMENTS }).map((_, idx) => {
          const active = idx < filledCount;
          const isLowZone = ((idx + 1) / GAUGE_SEGMENTS) * 100 <= clampedThreshold;
          return (
            <div
              key={idx}
              className="h-3.5 rounded-[3px] transition-colors duration-300"
              style={{
                backgroundColor: active
                  ? toneColor
                  : isLowZone && hasPct
                    ? 'var(--c-danger-soft)'
                    : 'var(--c-elev-2)',
                opacity: active ? (device.asleep ? 0.55 : 1) : 1,
              }}
            />
          );
        })}
      </div>

      <div className="relative h-4 text-[11px] c-num" style={{ color: 'var(--c-ink-faint)' }}>
        <span className="absolute left-0 top-0">0%</span>
        <span className="absolute left-1/2 -translate-x-1/2 top-0">50%</span>
        <span className="absolute right-0 top-0">100%</span>
        {!isWired && (
          <span
            className="absolute top-0 -translate-x-1/2 whitespace-nowrap"
            style={{ left: `${clampedThreshold}%`, color: 'var(--c-warn)' }}
            title={`低电提醒阈值：${clampedThreshold}%`}
          >
            ▲ {clampedThreshold}%
          </span>
        )}
      </div>
    </div>
  );
}

export function GenericDeviceBench({ device }: { device: DeviceEntry }) {
  const hubSettings = useHubStore((s) => s.hubSettings);
  const transitionDeviceKey = useHubStore((s) => s.transitionDeviceKey);
  const isHeroTransition = transitionDeviceKey === device.key;
  const syncState = useHubStore((s) => s.syncState);
  const refreshDevices = useHubStore((s) => s.refreshDevices);
  const renameDevice = useHubStore((s) => s.renameDevice);
  const updateHubSettings = useHubStore((s) => s.updateHubSettings);

  const [nameDraft, setNameDraft] = useState(device.name);

  useEffect(() => {
    setNameDraft(device.name);
  }, [device.key, device.name]);

  const battText = formatBatteryText(device);
  const tone = getBatteryTone(device);
  const toneColor = toneCssColor(tone);
  const ambientGlow = getAmbientGlowColor(tone);
  const powerLabel = getPowerLabel(device);

  const threshold = hubSettings?.lowBatteryThreshold ?? 20;
  const hiddenList = hubSettings?.hiddenDevices ?? [];
  const isHiddenInTaskbar = hiddenList.includes(device.key);
  const hasCustomAlias = Boolean(hubSettings?.deviceAliases?.[device.key]);
  const isBusy = syncState === 'reading' || syncState === 'writing';

  const commitRename = async () => {
    const trimmed = nameDraft.trim();
    if (!trimmed || trimmed === device.name) {
      setNameDraft(device.name);
      return;
    }
    await renameDevice(device.key, trimmed);
  };

  const resetAlias = async () => {
    await renameDevice(device.key, '');
  };

  const toggleTaskbarVisibility = async () => {
    const nextHidden = isHiddenInTaskbar
      ? hiddenList.filter((k) => k !== device.key)
      : [...hiddenList, device.key];
    await updateHubSettings({ hiddenDevices: nextHidden });
  };

  return (
    <div className="w-full max-w-[1360px] mx-auto flex-1 grid grid-cols-1 md:grid-cols-12 items-center">
      {/* Left 50/50 Spacious Hardware Stage (matching Mouse workbench) */}
      <section className="md:col-span-5 relative flex flex-col items-center justify-center py-10 px-6">
        {/* Soft hardware-status ambient wash */}
        <div
          aria-hidden="true"
          className="pointer-events-none absolute inset-0"
          style={{
            backgroundImage: `radial-gradient(circle 220px at 50% 46%, ${ambientGlow} 0%, transparent 74%)`,
          }}
        />

        {/* Optical Reticle identical to Mouse workbench */}
        <div className="relative w-[300px] sm:w-[340px] h-[280px] sm:h-[310px] flex items-center justify-center">
          <svg
            viewBox="0 0 360 360"
            aria-hidden="true"
            className="pointer-events-none absolute inset-0 w-full h-full opacity-50"
          >
            <circle cx="180" cy="180" r="112" fill="none" stroke="var(--c-border)" strokeWidth="1" />
            <circle
              cx="180"
              cy="180"
              r="158"
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
              <HardwareSilhouette kind={device.kind} active={device.connected} size="stage" />
            </div>
            <div
              className="c-contact-shadow h-3.5 rounded-full mt-5"
              style={{
                width:
                  device.kind === 'keyboard'
                    ? '240px'
                    : device.kind === 'gamepad'
                      ? '168px'
                      : '128px',
              }}
            />
          </div>
        </div>

        {/* Minimal 3-stat baseline strip directly under hardware (matches Mouse stage footer) */}
        <div className="mt-4 flex items-center justify-center gap-6 text-xs c-num">
          <span>
            <span style={{ color: 'var(--c-ink-faint)' }}>电量 </span>
            <span className="font-semibold" style={{ color: toneColor }}>
              {battText}
            </span>
          </span>
          <span className="font-medium" style={{ color: 'var(--c-ink)' }}>
            {device.via || 'HID'}
          </span>
          <span style={{ color: 'var(--c-ink-muted)' }}>{powerLabel}</span>
        </div>
      </section>

      {/* Right: Borderless Workbench Controls directly on studio canvas */}
      <section className="md:col-span-7 px-4 sm:px-8 lg:px-14 py-8 space-y-8">
        {/* Block 1: Hero Battery Readout + Calibrated Gauge (parallels Mouse 800 DPI block) */}
        <div className="space-y-4">
          <div className="flex items-center justify-between gap-4">
            <div className="flex items-center gap-2 text-xs" style={{ color: 'var(--c-ink-muted)' }}>
              <span
                className="w-2 h-2 rounded-full"
                style={{ backgroundColor: toneColor }}
              />
              <span>{device.name} · 实时电量</span>
            </div>

            <button
              type="button"
              onClick={() => void refreshDevices(true)}
              disabled={isBusy}
              className="h-7 px-3 rounded-md text-xs font-medium inline-flex items-center gap-1.5 cursor-pointer bg-[var(--c-elev)] hover:bg-[var(--c-hover)] border border-[var(--c-border)] text-[var(--c-ink-muted)] disabled:opacity-50 transition-colors"
            >
              <RefreshCw className={`w-3 h-3 ${syncState === 'reading' ? 'animate-spin' : ''}`} />
              刷新电量
            </button>
          </div>

          <div className="flex items-baseline gap-3">
            <span
              className="c-display-num text-5xl sm:text-[56px] font-semibold leading-none"
              style={{ color: tone === 'ok' ? 'var(--c-ink)' : toneColor }}
            >
              {battText}
            </span>
            <span
              className="inline-flex items-center gap-1.5 text-xs font-medium"
              style={{ color: 'var(--c-ink-muted)' }}
            >
              {device.charging === 2 && <BatteryCharging className="w-3.5 h-3.5" style={{ color: toneColor }} />}
              {device.mode === 'wired' && <PlugZap className="w-3.5 h-3.5" style={{ color: toneColor }} />}
              {powerLabel}
            </span>
          </div>

          <SegmentedEnergyGauge device={device} threshold={threshold} toneColor={toneColor} />
        </div>

        {/* Block 2: Display Name & Taskbar Integration (hairline divider, no card box) */}
        <div
          className="pt-7 border-t space-y-5"
          style={{ borderColor: 'var(--c-border)' }}
        >
          <div className="flex items-center justify-between gap-4 flex-wrap">
            <div>
              <div className="text-xs font-medium">设备名称</div>
              <div className="text-[11px] mt-0.5" style={{ color: 'var(--c-ink-faint)' }}>
                同步显示在任务栏电量条
              </div>
            </div>

            <div className="flex items-center gap-2">
              <input
                type="text"
                value={nameDraft}
                maxLength={40}
                disabled={isBusy}
                onChange={(e) => setNameDraft(e.target.value)}
                onBlur={() => void commitRename()}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') {
                    if (e.nativeEvent.isComposing || e.keyCode === 229) return;
                    e.preventDefault();
                    e.currentTarget.blur();
                  } else if (e.key === 'Escape') {
                    setNameDraft(device.name);
                    e.currentTarget.blur();
                  }
                }}
                className="w-48 h-8 px-3 rounded-lg text-xs font-medium transition-colors"
                style={{
                  backgroundColor: 'var(--c-elev)',
                  border: '1px solid var(--c-border-strong)',
                  color: 'var(--c-ink)',
                }}
                aria-label="自定义显示名称"
              />
              {hasCustomAlias && (
                <button
                  type="button"
                  onClick={() => void resetAlias()}
                  disabled={isBusy}
                  className="h-8 w-8 rounded-lg inline-flex items-center justify-center cursor-pointer bg-[var(--c-elev)] hover:bg-[var(--c-hover)] border border-[var(--c-border)] text-[var(--c-ink-muted)] disabled:opacity-50"
                  title="恢复原名"
                  aria-label="恢复原名"
                >
                  <RotateCcw className="w-3.5 h-3.5" />
                </button>
              )}
            </div>
          </div>

          <div className="flex items-center justify-between gap-4">
            <div>
              <div className="text-xs font-medium">在任务栏显示</div>
              <div className="text-[11px] mt-0.5" style={{ color: 'var(--c-ink-faint)' }}>
                关闭后仅在应用内查看
              </div>
            </div>
            <button
              type="button"
              role="switch"
              aria-label="在任务栏显示"
              aria-checked={!isHiddenInTaskbar}
              data-on={String(!isHiddenInTaskbar)}
              disabled={isBusy}
              onClick={() => void toggleTaskbarVisibility()}
              className="hw-switch"
            />
          </div>
        </div>

        {/* Block 3: Borderless Telemetry Channel Specs (hairline divider, 4-col tabular) */}
        <div
          className="pt-7 border-t"
          style={{ borderColor: 'var(--c-border)' }}
        >
          <dl className="grid grid-cols-2 sm:grid-cols-4 gap-5 text-xs">
            <div>
              <dt style={{ color: 'var(--c-ink-faint)' }}>连接方式</dt>
              <dd className="font-medium mt-1">{formatConnectionMode(device.mode)}</dd>
            </div>
            <div>
              <dt style={{ color: 'var(--c-ink-faint)' }}>电量档位</dt>
              <dd className="font-medium mt-1">{device.level || '—'}</dd>
            </div>
            <div>
              <dt style={{ color: 'var(--c-ink-faint)' }}>连接详情</dt>
              <dd className="font-medium mt-1 truncate" title={device.detail || ''}>
                {device.detail || '—'}
              </dd>
            </div>
            <div>
              <dt style={{ color: 'var(--c-ink-faint)' }}>设备 ID</dt>
              <dd className="c-mono text-[11px] mt-1 truncate" style={{ color: 'var(--c-ink-muted)' }} title={device.key}>
                {device.key}
              </dd>
            </div>
          </dl>
        </div>
      </section>
    </div>
  );
}
