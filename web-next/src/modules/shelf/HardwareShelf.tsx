import { type CSSProperties, useEffect, useMemo, useRef, useState } from 'react';
import { ChevronRight, RefreshCw, SlidersHorizontal, Zap } from 'lucide-react';
import type { DeviceEntry } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import {
  formatBatteryText,
  getBatteryTone,
  HardwareSilhouette,
  toneCssColor,
} from '../../ui/DeviceGlyph';
import { deviceHeroTransitionName } from '../../ui/view-transition';

function formatDeviceMeta(device: DeviceEntry): { text: string; isWarn: boolean } {
  if (!device.connected) {
    return { text: '未连接', isWarn: false };
  }
  if (device.asleep) {
    return { text: '浅休眠', isWarn: true };
  }

  const parts: string[] = [];
  if (device.mode === 'wired') {
    parts.push('有线');
  } else if (device.mode === 'bluetooth') {
    parts.push('蓝牙');
  } else if (device.mode === 'wireless') {
    parts.push(device.receiver ? `${device.receiver} 无线` : '2.4G 无线');
  } else if (device.via) {
    parts.push(device.via);
  }

  if (device.sensor) {
    parts.push(device.sensor);
  }

  return { text: parts.join(' · '), isWarn: false };
}

function itemBasisClass(kind: DeviceEntry['kind']): string {
  if (kind === 'keyboard') return 'w-[276px]';
  if (kind === 'gamepad') return 'w-[240px]';
  if (kind === 'headset') return 'w-[196px]';
  return 'w-[168px]';
}

function itemNaturalWidth(kind: DeviceEntry['kind']): number {
  if (kind === 'keyboard') return 276;
  if (kind === 'gamepad') return 240;
  if (kind === 'headset') return 196;
  return 168;
}

function ShelfItem({
  device,
  onSelect,
}: {
  device: DeviceEntry;
  onSelect: () => void;
}) {
  const transitionDeviceKey = useHubStore((s) => s.transitionDeviceKey);
  const isHeroTransition = transitionDeviceKey === device.key;
  const battText = formatBatteryText(device);
  const tone = getBatteryTone(device);
  const barColor = toneCssColor(tone);
  const meta = formatDeviceMeta(device);
  const pct =
    device.mode === 'wired'
      ? 100
      : device.battery >= 0
        ? Math.min(100, Math.max(0, device.battery))
        : 0;

  return (
    <button
      type="button"
      onClick={onSelect}
      className={`group ${itemBasisClass(device.kind)} shrink-0 flex flex-col items-center text-left cursor-pointer rounded-xl`}
    >
      {/* Peripheral Silhouette resting on shelf */}
      <div className="relative flex flex-col items-center justify-end h-[132px] w-full pb-1">
        {/* Always-on soft studio backlight lifting dark hardware silhouettes off the wall */}
        {device.connected && (
          <div
            aria-hidden="true"
            className="pointer-events-none absolute -inset-x-5 -top-3 bottom-0 rounded-full"
            style={{ background: 'var(--c-shelf-ambient)' }}
          />
        )}
        {/* Soft studio spotlight that fades in smoothly on hover/focus */}
        <div
          aria-hidden="true"
          className="c-shelf-spotlight pointer-events-none absolute -inset-x-4 -top-4 bottom-0 rounded-full"
          style={{
            background:
              'radial-gradient(ellipse 68% 60% at 50% 54%, var(--c-accent-soft) 0%, transparent 72%)',
          }}
        />
        <div
          className="c-shelf-object relative z-10 flex items-end justify-center"
          style={
            isHeroTransition
              ? ({
                  viewTransitionName: deviceHeroTransitionName(device.key),
                  viewTransitionClass: 'device-hero-vt',
                } as CSSProperties)
              : undefined
          }
        >
          <HardwareSilhouette kind={device.kind} active={device.connected} size="shelf" />
        </div>
        {/* Ground contact shadow directly on the shelf surface */}
        <div
          className="c-contact-shadow h-3 rounded-full -mt-1.5"
          style={{
            width: device.kind === 'keyboard' ? '82%' : device.kind === 'mouse' ? '54%' : '64%',
          }}
        />
      </div>

      {/* Hardware Nameplate aligned cleanly under the CNC shelf fascia */}
      <div className="w-full pt-7 px-1.5 pb-1 transition-opacity">
        <div className="flex items-center justify-between gap-2">
          <div className="flex items-center gap-0.5 min-w-0">
            <span
              className="font-semibold text-[14px] truncate group-hover:text-[var(--c-accent)] transition-colors"
              style={{ color: 'var(--c-ink)' }}
            >
              {device.name}
            </span>
            <ChevronRight
              className="w-3.5 h-3.5 shrink-0 c-shelf-arrow"
              style={{ color: 'var(--c-ink-muted)' }}
            />
          </div>
          <span
            className="c-num text-[13px] font-semibold shrink-0 inline-flex items-center gap-0.5"
            style={{ color: barColor }}
          >
            {device.charging === 2 && <Zap className="w-3 h-3" />}
            {battText}
          </span>
        </div>

        {/* Thin battery gauge bar embedded in nameplate */}
        <div
          className="h-[3px] w-full rounded-full mt-1.5"
          style={{ backgroundColor: 'var(--c-border)' }}
        >
          <div
            className="h-full rounded-full transition-[width] duration-300"
            style={{
              width: `${device.connected && (device.battery >= 0 || device.mode === 'wired') ? pct : 0}%`,
              backgroundColor: barColor,
              boxShadow:
                device.connected && (device.battery >= 0 || device.mode === 'wired')
                  ? `0 0 8px color-mix(in srgb, ${barColor} var(--c-bar-glow-pct), transparent)`
                  : undefined,
            }}
          />
        </div>

        {meta.text && (
          <div
            className="mt-1.5 text-[13px] truncate"
            style={{ color: meta.isWarn ? 'var(--c-warn)' : 'var(--c-ink-faint)' }}
          >
            {meta.text}
          </div>
        )}
      </div>
    </button>
  );
}

function ShelfFooter() {
  const hubSettings = useHubStore((s) => s.hubSettings);
  const toggleSettings = useHubStore((s) => s.toggleSettings);

  return (
    <footer
      className="px-2 pt-3.5 border-t flex items-center justify-between gap-4 text-[13px]"
      style={{ borderColor: 'var(--c-border)', color: 'var(--c-ink-faint)' }}
    >
      <span className="truncate c-num">
        {hubSettings
          ? `${hubSettings.taskbarWidget ? '任务栏电量条已开启' : '任务栏电量条已关闭'} · 每 ${hubSettings.pollIntervalSec} 秒同步`
          : 'PeriphHub'}
      </span>

      <button
        type="button"
        onClick={toggleSettings}
        className="inline-flex items-center gap-1.5 shrink-0 hover:text-[var(--c-ink)] transition-colors cursor-pointer"
      >
        <SlidersHorizontal className="w-3.5 h-3.5" />
        <span>外观与任务栏设置</span>
      </button>
    </footer>
  );
}

function ShelfTier({
  label,
  devices,
  onSelectDevice,
}: {
  label: string;
  devices: DeviceEntry[];
  onSelectDevice: (key: string) => void;
}) {
  const tierRef = useRef<HTMLElement>(null);
  const [tierWidth, setTierWidth] = useState(960);

  useEffect(() => {
    const el = tierRef.current;
    if (!el || typeof ResizeObserver === 'undefined') return;
    const update = () => {
      if (el.clientWidth > 0) setTierWidth(el.clientWidth);
    };
    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    return () => ro.disconnect();
  }, []);

  const rows = useMemo(() => {
    const isLg = tierWidth >= 900;
    const isSm = tierWidth >= 600;
    const padX = isLg ? 80 : isSm ? 64 : 32;
    const gapX = isLg ? 40 : 24;
    const avail = Math.max(280, tierWidth - padX);

    const result: DeviceEntry[][] = [];
    let current: DeviceEntry[] = [];
    let used = 0;

    for (const dev of devices) {
      const w = itemNaturalWidth(dev.kind);
      const needed = current.length === 0 ? w : gapX + w;
      if (current.length > 0 && used + needed > avail) {
        result.push(current);
        current = [dev];
        used = w;
      } else {
        current.push(dev);
        used += needed;
      }
    }
    if (current.length > 0) result.push(current);
    return result;
  }, [devices, tierWidth]);

  if (devices.length === 0) return null;

  return (
    <section ref={tierRef} className="w-full">
      <div className="px-2 mb-0.5 flex items-center justify-between gap-4">
        <h2 className="text-[13px] font-semibold" style={{ color: 'var(--c-ink-muted)' }}>
          {label}
        </h2>
        <span className="text-[13px] c-num" style={{ color: 'var(--c-ink-faint)' }}>
          {devices.length} 台
        </span>
      </div>

      <div className="space-y-8">
        {rows.map((row, rowIdx) => (
          <div key={rowIdx} className="relative">
            {/* Continuous 2.5D CNC anodized aluminum floating ledge for every row */}
            <div
              aria-hidden="true"
              className="pointer-events-none absolute left-0 right-0 top-[106px] z-0"
            >
              <div className="c-shelf-ledge">
                <div className="c-shelf-deck" />
                <div className="c-shelf-bevel" />
                <div className="c-shelf-fascia" />
                <div className="c-shelf-undershadow" />
              </div>
            </div>

            <div className="relative z-10 flex items-start justify-center gap-x-6 lg:gap-x-10 px-4 sm:px-8 lg:px-10">
              {row.map((device) => (
                <ShelfItem
                  key={device.key}
                  device={device}
                  onSelect={() => onSelectDevice(device.key)}
                />
              ))}
            </div>
          </div>
        ))}
      </div>
    </section>
  );
}

export function HardwareShelf() {
  const devices = useHubStore((s) => s.devices);
  const selectDevice = useHubStore((s) => s.selectDevice);
  const refreshDevices = useHubStore((s) => s.refreshDevices);

  const primaryDevices = devices.filter((d) => d.driverLevel === 'full');
  const companionDevices = devices.filter((d) => d.driverLevel !== 'full');
  const isCompactLayout = devices.length <= 6;

  return (
    <div className="relative flex-1 w-full max-w-5xl mx-auto flex flex-col justify-between py-4 sm:py-6">
      <div
        aria-hidden="true"
        className="c-shelf-backdrop absolute inset-0 pointer-events-none z-0"
      />

      {/* Monumental 2.5D architectural studio wall watermark behind physical shelves */}
      <div
        aria-hidden="true"
        className="pointer-events-none select-none absolute inset-x-0 top-[46%] -translate-y-1/2 flex justify-center overflow-hidden z-0"
      >
        <span className="c-wall-watermark c-display-num text-[clamp(5.5rem,13vw,10.5rem)] font-bold tracking-[-0.06em] leading-none whitespace-nowrap">
          PERIPHHUB
        </span>
      </div>

      <div className="relative z-10 flex-1 flex flex-col justify-between">
        {devices.length === 0 ? (
          <div className="flex-1 flex flex-col items-center justify-center py-8">
            <div className="text-[14px] font-medium" style={{ color: 'var(--c-ink-muted)' }}>
              还没检测到外设
            </div>
            <p className="text-[13px] mt-1 mb-6" style={{ color: 'var(--c-ink-faint)' }}>
              插好接收器或唤醒蓝牙设备后再扫一次
            </p>
            <div className="w-full max-w-lg mb-6">
              <div className="c-shelf-ledge">
                <div className="c-shelf-deck" />
                <div className="c-shelf-bevel" />
                <div className="c-shelf-fascia" />
                <div className="c-shelf-undershadow" />
              </div>
            </div>
            <button
              type="button"
              onClick={() => void refreshDevices(true)}
              className="h-8 px-3.5 rounded-lg text-[13px] font-medium inline-flex items-center gap-1.5 cursor-pointer"
              style={{ backgroundColor: 'var(--c-hi)', color: 'var(--c-hi-inv)' }}
            >
              <RefreshCw className="w-3.5 h-3.5" />
              重新扫描
            </button>
          </div>
        ) : (
          <div
            className={`flex-1 flex flex-col space-y-9 ${
              isCompactLayout ? 'justify-center py-2 sm:py-4' : 'justify-start pt-4 pb-6'
            }`}
          >
            <ShelfTier
              label="已适配设备"
              devices={primaryDevices}
              onSelectDevice={(key) => void selectDevice(key)}
            />

            <ShelfTier
              label={primaryDevices.length > 0 ? '电量监控' : '已连接设备'}
              devices={companionDevices}
              onSelectDevice={(key) => void selectDevice(key)}
            />
          </div>
        )}

        <ShelfFooter />
      </div>
    </div>
  );
}
