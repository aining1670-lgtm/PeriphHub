import { type CSSProperties, useEffect, useState } from 'react';
import type { DeviceEntry, KbdLedState } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import { ConfirmDialog } from '../../ui/ConfirmDialog';
import { formatBatteryText, getBatteryTone, toneCssColor } from '../../ui/DeviceGlyph';
import { hexToRgba } from '../../ui/PrecisionControls';
import { formatRoyuanKeyEntry } from './bc98-layout';
import { applyKbdPatchOptimistic } from './kbd-state-helpers';
import { KeyboardChassis } from './KeyboardChassis';
import { KeyboardKeymapPanel } from './KeyboardKeymapPanel';
import { KeyboardPowerView } from './KeyboardPowerView';
import { LightingZoneControls } from './LightingZoneControls';
import { useKeyboardHardware } from './useKeyboardHardware';

export function KeyboardWorkbench({
  device,
  activeMode,
}: {
  device: DeviceEntry;
  activeMode: string;
}) {
  const transitionDeviceKey = useHubStore((s) => s.transitionDeviceKey);
  const isHeroTransition = transitionDeviceKey === device.key;

  const hw = useKeyboardHardware(device);
  const [layer, setLayer] = useState<'base' | 'fn'>('base');
  const [selSlot, setSelSlot] = useState<number>(3); // CapsLock slot 3
  const [confirmLayerReset, setConfirmLayerReset] = useState(false);

  const [liveBright, setLiveBright] = useState<number | null>(null);
  const [liveSpeed, setLiveSpeed] = useState<number | null>(null);
  const [liveSideBright, setLiveSideBright] = useState<number | null>(null);
  const [liveSideSpeed, setLiveSideSpeed] = useState<number | null>(null);

  const isLight = activeMode === 'lighting';
  const isPower = activeMode === 'power';

  useEffect(() => {
    if (hw.layout.keys.length > 0 && !hw.layout.keys.some((k) => k.slot === selSlot)) {
      setSelSlot(hw.layout.keys[0].slot);
    }
  }, [hw.layout.keys, selSlot]);

  useEffect(() => {
    if (!hw.fnLayerEnabled && layer === 'fn') {
      setLayer('base');
    }
  }, [hw.fnLayerEnabled, layer]);

  if (isPower) {
    return (
      <KeyboardPowerView
        device={device}
        isHeroTransition={isHeroTransition}
        loading={hw.loading}
        kbdState={hw.kbdState}
        supportedOptions={hw.supportedOptions}
        debounceMin={hw.debounceMin}
        debounceMax={hw.debounceMax}
        shallowPresets={hw.shallowPresets}
        deepPresets={hw.deepPresets}
        onPatch={hw.patchKbdState}
      />
    );
  }

  const battText = formatBatteryText(device);
  const battColor = toneCssColor(getBatteryTone(device));

  const mainLed: KbdLedState | null = hw.kbdState?.mainLed ?? null;
  const ledOn = mainLed?.on ?? !(hw.kbdState?.options?.backlightOff ?? false);
  const activeLedMode = mainLed?.mode ?? null;
  const activeLedColor = mainLed?.color ?? '#38BDF8';
  const activeBrightness = liveBright ?? mainLed?.brightness ?? 0;
  const isColorful = mainLed?.colorful ?? false;

  const activeModeObj = hw.mainModes.find((m) => m.code === activeLedMode);
  const supportsColor = activeModeObj ? Boolean(activeModeObj.supportsColor) : true;

  const sideLed: KbdLedState | null = hw.kbdState?.sideLed ?? null;
  const sideOn = sideLed?.on ?? !(hw.kbdState?.options?.sidelightOff ?? false);

  // Hardware-truthful static backlight state (no fabricated per-mode wave animations).
  const effBri = activeBrightness / Math.max(1, hw.brightnessMax);
  const backlightLit = isLight && ledOn && activeLedMode !== null && effBri > 0;
  const isMultiColorDisplay = isColorful || !supportsColor;

  const selKeySpec = hw.layout.keys.find((k) => k.slot === selSlot) ?? hw.layout.keys[0];
  const activeLayerArray = hw.keysState
    ? layer === 'fn'
      ? hw.keysState.fn
      : hw.keysState.base
    : null;
  const selHardwareEntry = activeLayerArray?.[selSlot];
  const selMappedLabel = formatRoyuanKeyEntry(selHardwareEntry, layer === 'fn');

  const handleResetSelectedKey = () => {
    const spec = hw.layout.keys.find((k) => k.slot === selSlot);
    if (!spec) return;
    const factory = layer === 'fn' ? spec.fn : spec.base;
    void hw.handleWriteKeyEntry(selSlot, factory, layer === 'fn', `${spec.label} (出厂默认)`);
  };

  return (
    <div className="relative flex-1 flex flex-col justify-start w-full max-w-[1160px] mx-auto px-4 sm:px-6 pt-6 pb-4 space-y-5">
      <div
        aria-hidden="true"
        className={`pointer-events-none fixed inset-0 z-0 transition-opacity duration-200 ${
          backlightLit ? 'opacity-100' : 'opacity-0'
        }`}
        style={{
          background: isMultiColorDisplay
            ? `linear-gradient(90deg, hsla(198,85%,56%,${(0.06 * effBri).toFixed(3)}), hsla(280,80%,60%,${(0.06 * effBri).toFixed(3)}), hsla(348,85%,58%,${(0.06 * effBri).toFixed(3)}), hsla(42,90%,55%,${(0.06 * effBri).toFixed(3)}))`
            : `radial-gradient(ellipse 64% 44% at 50% 34%, ${hexToRgba(
                activeLedColor,
                0.09 * effBri,
              )}, transparent 72%)`,
          maskImage: isMultiColorDisplay
            ? 'radial-gradient(ellipse 64% 44% at 50% 34%, black, transparent 72%)'
            : undefined,
          WebkitMaskImage: isMultiColorDisplay
            ? 'radial-gradient(ellipse 64% 44% at 50% 34%, black, transparent 72%)'
            : undefined,
        }}
      />

      {/* Top Controls: Onboard Profile + Layer + Selected Key Summary */}
      <div className="relative z-10 min-h-[34px] flex flex-wrap items-center justify-between gap-3">
        <div className="flex flex-wrap items-center gap-2.5">
          <div
            className="sliding-pill-tabs flex p-0.5 rounded-lg"
            style={
              {
                background: 'var(--c-elev)',
                border: '1px solid var(--c-border)',
                '--pill-radius': '0.375rem',
              } as CSSProperties
            }
            title="切换板载配置层"
          >
            {hw.profiles.map((p) => {
              const active = hw.kbdState?.profile === p;
              return (
                <button
                  key={p}
                  type="button"
                  data-active={active ? 'true' : undefined}
                  aria-pressed={active}
                  disabled={hw.loading || hw.switchingProfile}
                  onClick={() => void hw.handleProfileChange(p)}
                  className="px-2.5 py-1 rounded-md text-xs font-medium c-num cursor-pointer transition-colors disabled:opacity-45"
                  style={{
                    color: active ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                  }}
                >
                  P{p + 1}
                </button>
              );
            })}
          </div>

          {!isLight && hw.fnLayerEnabled && (
            <div
              className="sliding-pill-tabs flex p-0.5 rounded-lg"
              style={
                {
                  background: 'var(--c-elev)',
                  border: '1px solid var(--c-border)',
                  '--pill-radius': '0.375rem',
                  '--pill-bg': 'var(--c-elev-2)',
                } as CSSProperties
              }
            >
              {(
                [
                  ['base', 'Base 主层'],
                  ['fn', 'Fn 功能层'],
                ] as Array<['base' | 'fn', string]>
              ).map(([k, l]) => {
                const active = layer === k;
                return (
                  <button
                    key={k}
                    type="button"
                    data-active={active ? 'true' : undefined}
                    aria-pressed={active}
                    onClick={() => setLayer(k)}
                    className="px-3 py-1 rounded-md text-xs font-medium cursor-pointer transition-colors"
                    style={{
                      color: active ? 'var(--c-ink)' : 'var(--c-ink-faint)',
                    }}
                  >
                    {l}
                  </button>
                );
              })}
            </div>
          )}
        </div>

        <div
          className="text-xs c-num"
          style={{ color: 'var(--c-ink-muted)' }}
          title={!isLight && selKeySpec ? `Slot ${selSlot}` : undefined}
        >
          {!isLight && selKeySpec ? (
            <>
              <strong style={{ color: 'var(--c-ink)' }}>{selKeySpec.label || selKeySpec.id}</strong>{' '}
              → <strong style={{ color: 'var(--c-ink)' }}>{selMappedLabel}</strong>
            </>
          ) : (
            <>
              电量 <strong style={{ color: battColor }}>{battText}</strong>
              {hw.kbdState?.firmware && (
                <span className="ml-3" style={{ color: 'var(--c-ink-faint)' }}>
                  FW {hw.kbdState.firmware}
                </span>
              )}
            </>
          )}
        </div>
      </div>

      {/* CNC Anodized Aluminum Keyboard Chassis */}
      <KeyboardChassis
        deviceKey={device.key}
        isHeroTransition={isHeroTransition}
        layout={hw.layout}
        keysState={hw.keysState}
        layer={layer}
        selSlot={selSlot}
        onSelectSlot={setSelSlot}
        isLight={isLight}
        backlightLit={backlightLit}
        isMultiColorDisplay={isMultiColorDisplay}
        effBri={effBri}
        activeLedColor={activeLedColor}
      />

      {/* Bottom Unified Controls */}
      {isLight ? (
        <div
          key="lighting"
          className="relative z-10 border-t pt-5 grid grid-cols-1 md:grid-cols-12 gap-6 tab-enter"
          style={{ borderColor: 'var(--c-border)' }}
        >
          <LightingZoneControls
            zone="mainLed"
            resetKey={device.key}
            led={mainLed}
            ledOn={ledOn}
            modes={hw.mainModes}
            brightnessMax={hw.brightnessMax}
            speedMax={hw.speedMax}
            liveBrightness={liveBright}
            liveSpeed={liveSpeed}
            loading={hw.loading}
            onLiveBrightness={setLiveBright}
            onLiveSpeed={setLiveSpeed}
            onPatch={hw.patchKbdState}
            onOptimisticColor={(hex) =>
              hw.setKbdState((prev) =>
                applyKbdPatchOptimistic(prev, { mainLed: { colorful: false, color: hex } }),
              )
            }
          />
          {hw.hasSidelight && (
            <div
              className="md:col-span-12 border-t pt-5 grid grid-cols-1 md:grid-cols-12 gap-6"
              style={{ borderColor: 'var(--c-border)' }}
            >
              <LightingZoneControls
                zone="sideLed"
                resetKey={device.key}
                led={sideLed}
                ledOn={sideOn}
                modes={hw.sideModes}
                brightnessMax={hw.brightnessMax}
                speedMax={hw.speedMax}
                liveBrightness={liveSideBright}
                liveSpeed={liveSideSpeed}
                loading={hw.loading}
                onLiveBrightness={setLiveSideBright}
                onLiveSpeed={setLiveSideSpeed}
                onPatch={hw.patchKbdState}
                onOptimisticColor={(hex) =>
                  hw.setKbdState((prev) =>
                    applyKbdPatchOptimistic(prev, { sideLed: { colorful: false, color: hex } }),
                  )
                }
              />
            </div>
          )}
        </div>
      ) : (
        <KeyboardKeymapPanel
          deviceKey={device.key}
          loading={hw.loading}
          resettingLayer={hw.resettingLayer}
          selSlot={selSlot}
          layer={layer}
          onWriteKeyEntry={hw.handleWriteKeyEntry}
          onResetSelectedKey={handleResetSelectedKey}
          onRequestLayerReset={() => setConfirmLayerReset(true)}
        />
      )}

      <ConfirmDialog
        open={confirmLayerReset}
        title={`恢复${layer === 'fn' ? ' Fn 功能层' : ' Base 主层'}出厂默认键位？`}
        description={`将把配置层 P${(hw.kbdState?.profile ?? 0) + 1} 的「${
          layer === 'fn' ? 'Fn 功能层' : 'Base 主层'
        }」全部按键恢复为出厂默认映射并逐字节回读校验。`}
        confirmLabel="确认恢复"
        danger
        onCancel={() => setConfirmLayerReset(false)}
        onConfirm={() => {
          setConfirmLayerReset(false);
          void hw.handleResetCurrentLayer(layer === 'fn');
        }}
      />
    </div>
  );
}
