import type { CSSProperties } from 'react';
import mouseGpwLightUrl from '../../assets/mouse-gpw-light.webp';
import mouseGpwUrl from '../../assets/mouse-gpw.webp';
import type { DeviceEntry, KeyBindingItem } from '../../bridge/types';
import { formatBatteryText, getBatteryTone, toneCssColor } from '../../ui/DeviceGlyph';
import { deviceHeroTransitionName } from '../../ui/view-transition';
import {
  BUTTON_INDEX_BY_ID,
  decodeHardwareBinding,
  MOUSE_BUTTONS,
  type MouseButtonId,
} from './mouse-codec';
import { HOTSPOTS } from './mouse-utils';

export function MouseStage({
  device,
  isDark,
  isHeroTransition,
  isBtn,
  activeStageColor,
  selBtnIdx,
  onSelectBtn,
  buttonDecodedByIndex,
  extraBindings,
  pollingHz,
}: {
  device: DeviceEntry;
  isDark: boolean;
  isHeroTransition: boolean;
  isBtn: boolean;
  activeStageColor: string;
  selBtnIdx: number;
  onSelectBtn: (idx: number) => void;
  buttonDecodedByIndex: Record<number, ReturnType<typeof decodeHardwareBinding>>;
  extraBindings: KeyBindingItem[];
  pollingHz: number | null;
}) {
  const battText = formatBatteryText(device);
  const battColor = toneCssColor(getBatteryTone(device));

  return (
    <section className="relative z-10 md:col-span-5 flex items-center justify-center">
      <div className="relative flex flex-col items-center justify-center w-full h-full min-h-[320px] md:min-h-[420px] lg:min-h-[480px] py-4 md:py-6 lg:py-8 px-4 sm:px-6">
        <svg
          viewBox="0 0 420 420"
          aria-hidden="true"
          className="pointer-events-none absolute w-[360px] sm:w-[420px] h-auto opacity-45"
        >
          <circle cx="210" cy="210" r="132" fill="none" stroke="var(--c-border)" strokeWidth="1" />
          <circle
            cx="210"
            cy="210"
            r="186"
            fill="none"
            stroke="var(--c-border)"
            strokeWidth="1"
            strokeDasharray="3 7"
          />
        </svg>

        <div className="relative z-10 flex flex-col items-center w-full">
          <div
            className={`relative ${
              isBtn ? 'w-[172px] lg:w-[196px]' : 'w-[228px] sm:w-[264px]'
            } aspect-[480/915] transition-[width] duration-300 select-none`}
            style={
              isHeroTransition
                ? ({
                    viewTransitionName: deviceHeroTransitionName(device.key),
                    viewTransitionClass: 'device-hero-vt',
                  } as CSSProperties)
                : undefined
            }
          >
            <img
              src={isDark ? mouseGpwUrl : mouseGpwLightUrl}
              alt={device.name}
              draggable={false}
              decoding="async"
              className="w-full h-full object-contain pointer-events-none"
              style={{
                filter: isDark
                  ? 'drop-shadow(0 18px 24px rgba(0,0,0,0.45))'
                  : 'drop-shadow(0 16px 22px rgba(15,23,42,0.22))',
              }}
            />

            {/* Precision Pin Hotspots in Buttons Mode */}
            {isBtn &&
              HOTSPOTS.map((pt) => {
                const idx = BUTTON_INDEX_BY_ID[pt.id];
                const act = selBtnIdx === idx;
                const meta = MOUSE_BUTTONS.find((m) => m.id === pt.id)!;
                return (
                  <button
                    key={pt.id}
                    type="button"
                    title={meta.name}
                    onClick={() => onSelectBtn(idx)}
                    className="group absolute -translate-x-1/2 -translate-y-1/2 w-7 h-7 rounded-full flex items-center justify-center cursor-pointer"
                    style={{ left: `${pt.x}%`, top: `${pt.y}%` }}
                  >
                    {act && (
                      <span
                        className="pin-ripple pointer-events-none absolute left-1/2 top-1/2 w-3.5 h-3.5 rounded-full"
                        style={{ border: `1.5px solid ${activeStageColor}` }}
                      />
                    )}
                    <span
                      className="w-3 h-3 rounded-full transition-transform duration-150 group-hover:scale-125"
                      style={{
                        background: act ? activeStageColor : 'rgba(255,255,255,0.85)',
                        border: '2px solid rgba(11,12,16,0.85)',
                        boxShadow: act
                          ? `0 0 12px ${activeStageColor}`
                          : '0 2px 6px rgba(0,0,0,0.6)',
                      }}
                    />
                  </button>
                );
              })}
          </div>

          {isBtn && (
            <div className="grid grid-cols-2 sm:grid-cols-3 gap-2 w-full max-w-[380px] mt-4">
              {(['left', 'middle', 'right', 'side_fwd', 'side_back', 'dpi'] as MouseButtonId[]).map(
                (id) => {
                  const meta = MOUSE_BUTTONS.find((m) => m.id === id)!;
                  const idx = BUTTON_INDEX_BY_ID[id];
                  const decoded = buttonDecodedByIndex[idx];
                  const act = selBtnIdx === idx;
                  return (
                    <button
                      key={id}
                      type="button"
                      onClick={() => onSelectBtn(idx)}
                      className={`text-left px-3 py-2 rounded-lg cursor-pointer transition-[border-color,background-color,box-shadow] border ${
                        act
                          ? 'bg-[var(--c-elev-2)] border-[var(--c-hi)]'
                          : 'bg-transparent border-[var(--c-border)] hover:bg-[var(--c-hover)]'
                      }`}
                    >
                      <div className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                        {meta.name}
                      </div>
                      <div className="text-xs font-semibold truncate mt-0.5">
                        {decoded?.label ?? '—'}
                      </div>
                    </button>
                  );
                },
              )}
            </div>
          )}
        </div>

        {isBtn && extraBindings.length > 0 && (
          <div className="relative z-10 mt-5 w-full max-w-md flex flex-wrap justify-center gap-2">
            {extraBindings.map((row) => {
              const act = selBtnIdx === row.index;
              const decoded = buttonDecodedByIndex[row.index];
              return (
                <button
                  key={row.index}
                  type="button"
                  onClick={() => onSelectBtn(row.index)}
                  className={`px-3 py-1.5 rounded-lg text-left cursor-pointer transition-[border-color,background-color,box-shadow] border ${
                    act
                      ? 'bg-[var(--c-elev-2)] border-[var(--c-hi)]'
                      : 'bg-[var(--c-elev)] border-[var(--c-border)] hover:bg-[var(--c-hover)]'
                  }`}
                >
                  <div className="text-[10px]" style={{ color: 'var(--c-ink-faint)' }}>
                    {row.slot || `按键 ${row.index + 1}`}
                  </div>
                  <div className="text-xs font-semibold truncate max-w-[96px]">
                    {decoded?.label ?? '—'}
                  </div>
                </button>
              );
            })}
          </div>
        )}

        {/* Spec Readout Under Mouse */}
        <div
          className="relative z-10 mt-7 flex items-center gap-7 text-sm c-num"
          style={{ color: 'var(--c-ink-muted)' }}
        >
          <span className="inline-flex items-baseline gap-1.5">
            <span style={{ color: 'var(--c-ink-faint)' }}>电量</span>
            <strong className="text-[15px] font-semibold" style={{ color: battColor }}>
              {battText}
            </strong>
          </span>
          <span className="w-px h-3.5" style={{ background: 'var(--c-border-strong)' }} />
          <span className="font-medium" style={{ color: 'var(--c-ink)' }}>
            {device.sensor || '—'}
          </span>
          <span className="w-px h-3.5" style={{ background: 'var(--c-border-strong)' }} />
          <span className="font-medium" style={{ color: 'var(--c-ink)' }}>
            {pollingHz !== null
              ? `${pollingHz >= 1000 ? `${pollingHz / 1000}K` : pollingHz} Hz`
              : '—'}
          </span>
        </div>
      </div>
    </section>
  );
}
