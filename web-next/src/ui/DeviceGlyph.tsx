import {
  Cpu,
  Gamepad2,
  Headphones,
  Keyboard,
  Mic,
  Mouse,
  Smartphone,
} from 'lucide-react';
import gamepadGenericLightUrl from '../assets/gamepad-generic-light.webp';
import gamepadGenericUrl from '../assets/gamepad-generic.webp';
import headsetGenericLightUrl from '../assets/headset-generic-light.webp';
import headsetGenericUrl from '../assets/headset-generic.webp';
import mouseGpwLightUrl from '../assets/mouse-gpw-light.webp';
import mouseGpwUrl from '../assets/mouse-gpw.webp';
import type { DeviceEntry, DeviceKind } from '../bridge/types';
import { useThemeStore } from '../state/theme-store';

export function DeviceKindIcon({
  kind,
  className = 'w-4 h-4',
}: {
  kind: DeviceKind;
  className?: string;
}) {
  switch (kind) {
    case 'mouse':
      return <Mouse className={className} />;
    case 'keyboard':
      return <Keyboard className={className} />;
    case 'headset':
      return <Headphones className={className} />;
    case 'gamepad':
      return <Gamepad2 className={className} />;
    case 'phone':
      return <Smartphone className={className} />;
    case 'mic':
      return <Mic className={className} />;
    default:
      return <Cpu className={className} />;
  }
}

export function HardwareSilhouette({
  kind,
  active = true,
  size = 'shelf',
}: {
  kind: DeviceKind;
  active?: boolean;
  size?: 'shelf' | 'stage';
}) {
  const isDark = useThemeStore((s) => s.isDark);
  const accent = active ? 'var(--c-accent)' : 'var(--c-ink-faint)';
  const bodyFill = active ? 'var(--c-panel)' : 'var(--c-panel-subtle)';
  const bodyStroke = active ? 'var(--c-border-strong)' : 'var(--c-border)';
  const mouseShadow = isDark
    ? 'drop-shadow(0 12px 16px rgba(0, 0, 0, 0.55))'
    : 'drop-shadow(0 10px 14px rgba(15, 23, 42, 0.22))';
  const gearShadow = isDark
    ? 'drop-shadow(0 18px 22px rgba(0, 0, 0, 0.55))'
    : 'drop-shadow(0 14px 18px rgba(15, 23, 42, 0.22))';

  if (kind === 'keyboard') {
    const cls = size === 'stage' ? 'w-72 h-28' : 'w-[258px] h-[104px]';
    return (
      <div
        className={`${cls} rounded-[14px] p-2 flex items-center justify-center transition-opacity`}
        style={{
          background: 'var(--c-kb-case)',
          border: '1px solid var(--c-kb-bevel)',
          boxShadow: 'inset 0 1px 0 var(--c-kb-bevel), 0 12px 24px -10px rgba(0, 0, 0, 0.55)',
          opacity: active ? 1 : 0.55,
        }}
        aria-hidden="true"
      >
        <div
          className="w-full h-full rounded-[9px] p-1.5 flex flex-col justify-between gap-[3px]"
          style={{
            background: 'var(--c-kb-plate)',
            boxShadow: 'inset 0 1px 4px rgba(0, 0, 0, 0.5)',
          }}
        >
          {[0, 1, 2, 3].map((row) => (
            <div key={row} className="grid grid-cols-14 gap-[3px] flex-1">
              {Array.from({ length: 14 }).map((_, col) => {
                const isAccent = row === 0 && col === 0;
                const isMod = col === 0 || col === 13;
                return (
                  <span
                    key={col}
                    className="rounded-[2.5px] block"
                    style={{
                      background: isAccent
                        ? 'var(--c-key-accent-top)'
                        : isMod
                          ? 'var(--c-key-mod-top)'
                          : 'var(--c-key-alpha-top)',
                      boxShadow: 'var(--c-key-inset)',
                    }}
                  />
                );
              })}
            </div>
          ))}
          <div className="flex gap-[3px] h-[13px]">
            <span className="w-[10%] rounded-[2.5px]" style={{ background: 'var(--c-key-mod-top)', boxShadow: 'var(--c-key-inset)' }} />
            <span className="w-[10%] rounded-[2.5px]" style={{ background: 'var(--c-key-mod-top)', boxShadow: 'var(--c-key-inset)' }} />
            <span className="flex-1 rounded-[2.5px]" style={{ background: 'var(--c-key-alpha-top)', boxShadow: 'var(--c-key-inset)' }} />
            <span className="w-[9%] rounded-[2.5px]" style={{ background: 'var(--c-key-mod-top)', boxShadow: 'var(--c-key-inset)' }} />
            <span className="w-[9%] rounded-[2.5px]" style={{ background: 'var(--c-key-accent-top)', boxShadow: 'var(--c-key-inset)' }} />
            <span className="w-[9%] rounded-[2.5px]" style={{ background: 'var(--c-key-accent-top)', boxShadow: 'var(--c-key-inset)' }} />
            <span className="w-[9%] rounded-[2.5px]" style={{ background: 'var(--c-key-accent-top)', boxShadow: 'var(--c-key-inset)' }} />
          </div>
        </div>
      </div>
    );
  }

  if (kind === 'mouse') {
    const cls = size === 'stage' ? 'w-[96px] h-[168px]' : 'w-[74px] h-[132px]';
    return (
      <div className={`relative ${cls} flex items-center justify-center select-none`} aria-hidden="true">
        <img
          src={isDark ? mouseGpwUrl : mouseGpwLightUrl}
          alt=""
          draggable={false}
          className="w-full h-full object-contain pointer-events-none"
          style={{
            filter: active ? mouseShadow : 'grayscale(0.8) opacity(0.55)',
          }}
        />
      </div>
    );
  }

  if (kind === 'headset') {
    const cls = size === 'stage' ? 'w-[186px] h-[220px]' : 'w-[104px] h-[126px]';
    return (
      <div className={`relative ${cls} flex items-center justify-center select-none`} aria-hidden="true">
        <img
          src={isDark ? headsetGenericUrl : headsetGenericLightUrl}
          alt=""
          draggable={false}
          className="w-full h-full object-contain pointer-events-none"
          style={{
            filter: active ? gearShadow : 'grayscale(0.8) opacity(0.55)',
          }}
        />
      </div>
    );
  }

  if (kind === 'gamepad') {
    const cls = size === 'stage' ? 'w-[248px] h-[176px]' : 'w-[152px] h-[108px]';
    return (
      <div className={`relative ${cls} flex items-center justify-center select-none`} aria-hidden="true">
        <img
          src={isDark ? gamepadGenericUrl : gamepadGenericLightUrl}
          alt=""
          draggable={false}
          className="w-full h-full object-contain pointer-events-none"
          style={{
            filter: active ? gearShadow : 'grayscale(0.8) opacity(0.55)',
          }}
        />
      </div>
    );
  }

  return (
    <div
      className="w-20 h-20 rounded-2xl flex items-center justify-center border"
      style={{ backgroundColor: bodyFill, borderColor: bodyStroke, color: accent }}
    >
      <DeviceKindIcon kind={kind} className="w-9 h-9" />
    </div>
  );
}

export function formatBatteryText(device: DeviceEntry): string {
  if (!device.connected) return '离线';
  if (device.mode === 'wired') return '有线';
  if (device.battery < 0) return '—';
  return `${device.battery}%`;
}

export function getBatteryTone(device: DeviceEntry): 'ok' | 'warn' | 'danger' | 'accent' | 'muted' {
  if (!device.connected) return 'muted';
  if (device.mode === 'wired') return 'accent';
  if (device.battery < 0) return 'muted';
  if (device.battery <= 20) return 'danger';
  if (device.battery <= 40) return 'warn';
  return 'ok';
}

export function toneCssColor(tone: ReturnType<typeof getBatteryTone>): string {
  switch (tone) {
    case 'ok':
      return 'var(--c-ok)';
    case 'warn':
      return 'var(--c-warn)';
    case 'danger':
      return 'var(--c-danger)';
    case 'accent':
      return 'var(--c-accent)';
    default:
      return 'var(--c-ink-faint)';
  }
}
