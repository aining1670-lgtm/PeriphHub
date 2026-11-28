import { useEffect, useState } from 'react';
import type { DeviceEntry } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import { useThemeStore } from '../../state/theme-store';
import { ConfirmDialog } from '../../ui/ConfirmDialog';
import { hexToRgba } from '../../ui/PrecisionControls';
import { MouseAdvancedTab } from './MouseAdvancedTab';
import { MouseKeymapTab } from './MouseKeymapTab';
import { MousePerformanceTab } from './MousePerformanceTab';
import { MouseStage } from './MouseStage';
import { STAGE_COLORS } from './mouse-utils';
import { useMouseHardware } from './useMouseHardware';

export function MouseWorkbench({
  device,
  activeMode,
}: {
  device: DeviceEntry;
  activeMode: string;
}) {
  const setActiveMode = useHubStore((s) => s.setActiveMode);
  const transitionDeviceKey = useHubStore((s) => s.transitionDeviceKey);
  const isHeroTransition = transitionDeviceKey === device.key;
  const isDark = useThemeStore((s) => s.isDark);

  const hw = useMouseHardware(device);
  const [selBtnIdx, setSelBtnIdx] = useState<number>(3); // side_fwd
  const [confirmAction, setConfirmAction] = useState<'keys' | 'advanced' | null>(null);

  useEffect(() => {
    setSelBtnIdx(3);
  }, [device.key]);

  const isBtn = activeMode === 'keymap';
  const isAdv = activeMode === 'advanced';
  const activeStageColor = STAGE_COLORS[hw.stageIdx % STAGE_COLORS.length];

  return (
    <div className="relative flex-1 w-full max-w-[1360px] mx-auto grid grid-cols-1 md:grid-cols-12 items-center">
      {/* Subtle Dynamic Ambient Glow Following Active Hardware DPI LED */}
      <div
        aria-hidden="true"
        className="pointer-events-none fixed inset-0 z-0 transition-opacity duration-700"
        style={{
          background: `radial-gradient(circle 460px at 26% 50%, ${hexToRgba(activeStageColor, 0.1)}, transparent 72%)`,
        }}
      />

      {/* Left 50/50 Stage: Real GPW Top-Down Render + Optical Reticle */}
      <MouseStage
        device={device}
        isDark={isDark}
        isHeroTransition={isHeroTransition}
        isBtn={isBtn}
        activeStageColor={activeStageColor}
        selBtnIdx={selBtnIdx}
        onSelectBtn={setSelBtnIdx}
        buttonDecodedByIndex={hw.buttonDecodedByIndex}
        extraBindings={hw.extraBindings}
        pollingHz={hw.rateState?.hz ?? null}
      />

      {/* Right 50/50 Spacious Borderless Controls */}
      <section key={activeMode} className="relative z-10 md:col-span-7 tab-enter">
        {isBtn ? (
          <MouseKeymapTab
            deviceKey={device.key}
            loading={hw.loading}
            resetting={hw.resetting}
            selBtnIdx={selBtnIdx}
            bindings={hw.bindings}
            buttonDecodedByIndex={hw.buttonDecodedByIndex}
            supportsFire={hw.supportsFire}
            supportsMacros={hw.supportsMacros}
            onApplyBinding={hw.applyButtonBinding}
            onOpenMacros={() => setActiveMode('macros')}
            onRequestResetKeys={() => setConfirmAction('keys')}
          />
        ) : isAdv ? (
          <MouseAdvancedTab
            deviceKey={device.key}
            caps={hw.caps}
            loading={hw.loading}
            resetting={hw.resetting}
            sensorState={hw.sensorState}
            miscState={hw.miscState}
            ledState={hw.ledState}
            debounceMin={hw.debounceMin}
            debounceMax={hw.debounceMax}
            sleepPresets={hw.sleepPresets}
            isDiscreteSleep={hw.isDiscreteSleep}
            sleepMin={hw.sleepMin}
            sleepMax={hw.sleepMax}
            sleepStep={hw.sleepStep}
            supportsFire={hw.supportsFire}
            supportsScrollReverse={hw.supportsScrollReverse}
            supportsReceiverLed={hw.supportsReceiverLed}
            mouseLedModes={hw.mouseLedModes}
            onSetSensor={hw.handleSetSensor}
            onSetMisc={hw.handleSetMisc}
            onSetLed={hw.handleSetLed}
            onRequestResetAdvanced={() => setConfirmAction('advanced')}
          />
        ) : (
          <MousePerformanceTab
            deviceKey={device.key}
            driver={hw.driver}
            caps={hw.caps}
            loading={hw.loading}
            dpiState={hw.dpiState}
            stageIdx={hw.stageIdx}
            maxStages={hw.maxStages}
            dpiMin={hw.dpiMin}
            dpiMax={hw.dpiMax}
            dpiStep={hw.dpiStep}
            dpiAnchors={hw.dpiAnchors}
            supportsXySplit={hw.supportsXySplit}
            xySplit={hw.xySplit}
            onToggleXySplit={() => hw.setXySplit((v) => !v)}
            activeAxis={hw.activeAxis}
            onSelectAxis={hw.setEditAxis}
            stageXVal={hw.stageXVal}
            stageYVal={hw.stageYVal}
            activeStageColor={activeStageColor}
            rateOptions={hw.rateOptions}
            pollingHz={hw.rateState?.hz ?? null}
            lodOptions={hw.lodOptions}
            sensorModes={hw.sensorModes}
            sensorState={hw.sensorState}
            onCommitDpi={hw.commitDpi}
            onStageSelect={hw.handleStageSelect}
            onStageCountChange={hw.handleStageCountChange}
            onSetRate={hw.handleSetRate}
            onSetSensor={hw.handleSetSensor}
          />
        )}
      </section>

      <ConfirmDialog
        open={confirmAction !== null}
        title={confirmAction === 'keys' ? '恢复默认鼠标键位？' : '恢复出厂默认参数？'}
        description={
          confirmAction === 'keys'
            ? '将把全部 6 个鼠标按键恢复为默认功能映射并逐个回读校验，当前的自定义改键将被覆盖。'
            : '将恢复回报率（1000 Hz）、传感器默认算法与消抖/休眠参数并重新回读校验。'
        }
        confirmLabel="确认恢复"
        danger
        onCancel={() => setConfirmAction(null)}
        onConfirm={() => {
          const act = confirmAction;
          setConfirmAction(null);
          if (act === 'keys') void hw.handleResetDefaultKeys();
          else if (act === 'advanced') void hw.handleResetDefaultAdvanced();
        }}
      />
    </div>
  );
}
