import { useEffect, useRef } from 'react';
import { Bell, Eye, Monitor, Power, Sparkles, X } from 'lucide-react';
import { bridgeCommands } from '../bridge/commands';
import { useHubStore } from '../state/hub-store';
import { type ThemeMode, useThemeStore } from '../state/theme-store';
import { DeviceKindIcon } from './DeviceGlyph';
import { SegmentedTrack } from './PrecisionControls';

const THRESHOLD_PRESETS = [10, 15, 20, 30];
const POLL_PRESETS = [10, 30, 60, 120];

function nearestPresetIndex(presets: number[], value: number): number {
  let bestIdx = 0;
  let bestDiff = Math.abs(presets[0] - value);
  for (let i = 1; i < presets.length; i++) {
    const diff = Math.abs(presets[i] - value);
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIdx = i;
    }
  }
  return bestIdx;
}

function ToggleSwitch({
  checked,
  ariaLabel,
  onToggle,
}: {
  checked: boolean;
  ariaLabel: string;
  onToggle: () => void;
}) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={ariaLabel}
      data-on={String(checked)}
      onClick={onToggle}
      className="hw-switch"
    />
  );
}

export function SettingsDrawer() {
  const open = useHubStore((s) => s.settingsOpen);
  const setOpen = useHubStore((s) => s.setSettingsOpen);
  const settings = useHubStore((s) => s.hubSettings);
  const devices = useHubStore((s) => s.devices);
  const updateHubSettings = useHubStore((s) => s.updateHubSettings);
  const pushToast = useHubStore((s) => s.pushToast);

  const themeMode = useThemeStore((s) => s.mode);
  const setThemeMode = useThemeStore((s) => s.setMode);

  const dialogRef = useRef<HTMLDialogElement>(null);
  const closeBtnRef = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    const el = dialogRef.current;
    if (!el) return;
    if (open && !el.open) {
      el.showModal();
      closeBtnRef.current?.focus();
    } else if (!open && el.open) {
      el.close();
    }
  }, [open]);

  useEffect(() => {
    const el = dialogRef.current;
    if (!el || !open || 'closedBy' in HTMLDialogElement.prototype) return;
    const isOutside = (e: PointerEvent) => {
      const r = el.getBoundingClientRect();
      return e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom;
    };
    let pressedOutside = false;
    const onDown = (e: PointerEvent) => {
      pressedOutside = isOutside(e);
    };
    const onUp = (e: PointerEvent) => {
      if (pressedOutside && isOutside(e)) {
        el.close();
      }
      pressedOutside = false;
    };
    el.addEventListener('pointerdown', onDown);
    el.addEventListener('pointerup', onUp);
    return () => {
      el.removeEventListener('pointerdown', onDown);
      el.removeEventListener('pointerup', onUp);
    };
  }, [open]);

  const themeOptions: Array<{ value: ThemeMode; label: string }> = [
    { value: 'system', label: '跟随系统' },
    { value: 'light', label: '浅色' },
    { value: 'dark', label: '深色' },
  ];

  return (
    <dialog
      ref={dialogRef}
      aria-label="控制台与任务栏偏好"
      onClose={() => {
        if (open) setOpen(false);
      }}
      className="c-drawer-dialog m-0 ml-auto h-dvh max-h-none w-full max-w-[380px] border-0 border-l p-0 shadow-2xl"
      style={{
        backgroundColor: 'var(--c-panel)',
        borderColor: 'var(--c-border)',
        color: 'var(--c-ink)',
      }}
      {...({ closedby: 'any' } as Record<string, unknown>)}
    >
      {open && (
        <div className="flex flex-col h-full w-full">
          <div
            className="h-[52px] px-4 flex items-center justify-between border-b shrink-0"
            style={{ borderColor: 'var(--c-border)' }}
          >
            <div className="text-[13.5px] font-semibold">设置</div>
            <button
              ref={closeBtnRef}
              type="button"
              onClick={() => setOpen(false)}
              className="w-7 h-7 rounded-md flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)]"
              aria-label="关闭设置"
            >
              <X className="w-4 h-4" />
            </button>
          </div>

        <div className="flex-1 overflow-y-auto p-4 space-y-5">
          {/* Theme */}
          <section className="space-y-2.5">
            <div
              className="flex items-center gap-1.5 text-xs font-semibold tracking-wider"
              style={{ color: 'var(--c-ink-faint)' }}
            >
              <Sparkles className="w-3.5 h-3.5" />
              外观
            </div>
            <div
              className="rounded-xl border p-3.5 space-y-2.5"
              style={{
                backgroundColor: 'var(--c-panel-subtle)',
                borderColor: 'var(--c-border)',
              }}
            >
              <div className="text-xs font-medium">主题模式</div>
              <div
                className="grid grid-cols-3 gap-1 p-0.5 rounded-lg border"
                style={{
                  backgroundColor: 'var(--c-bg)',
                  borderColor: 'var(--c-border)',
                }}
              >
                {themeOptions.map((opt) => {
                  const active = themeMode === opt.value;
                  return (
                    <button
                      key={opt.value}
                      type="button"
                      onClick={() => setThemeMode(opt.value)}
                      className="py-1.5 rounded-md text-xs font-medium cursor-pointer transition-colors"
                      style={{
                        backgroundColor: active ? 'var(--c-panel)' : 'transparent',
                        color: active ? 'var(--c-ink)' : 'var(--c-ink-muted)',
                        boxShadow: active ? 'var(--c-shadow)' : 'none',
                      }}
                    >
                      {opt.label}
                    </button>
                  );
                })}
              </div>
            </div>
          </section>

          {!settings ? (
            <div className="text-xs py-4" style={{ color: 'var(--c-ink-faint)' }}>
              正在读取设置…
            </div>
          ) : (
            <>
              {/* Taskbar Widget */}
              <section className="space-y-2.5">
                <div
                  className="flex items-center gap-1.5 text-xs font-semibold tracking-wider"
                  style={{ color: 'var(--c-ink-faint)' }}
                >
                  <Monitor className="w-3.5 h-3.5" />
                  任务栏显示
                </div>
                <div
                  className="rounded-xl border divide-y"
                  style={{
                    backgroundColor: 'var(--c-panel-subtle)',
                    borderColor: 'var(--c-border)',
                  }}
                >
                  <div className="p-3.5 flex items-center justify-between gap-3" style={{ borderColor: 'var(--c-border)' }}>
                    <div className="text-xs font-medium">显示任务栏电量条</div>
                    <ToggleSwitch
                      checked={settings.taskbarWidget}
                      ariaLabel="显示任务栏电量条"
                      onToggle={() =>
                        void updateHubSettings({ taskbarWidget: !settings.taskbarWidget })
                      }
                    />
                  </div>

                  <div className="p-3.5 flex items-center justify-between gap-3" style={{ borderColor: 'var(--c-border)' }}>
                    <div>
                      <div className="text-xs font-medium">双行显示</div>
                      <div className="text-[11px] mt-0.5" style={{ color: 'var(--c-ink-faint)' }}>
                        设备较多时分两行排开
                      </div>
                    </div>
                    <ToggleSwitch
                      checked={settings.widgetStacked}
                      ariaLabel="双行显示"
                      onToggle={() =>
                        void updateHubSettings({ widgetStacked: !settings.widgetStacked })
                      }
                    />
                  </div>

                  <div className="p-3.5 flex items-center justify-between gap-3" style={{ borderColor: 'var(--c-border)' }}>
                    <div className="text-xs font-medium">显示背景底板</div>
                    <ToggleSwitch
                      checked={settings.widgetBackground}
                      ariaLabel="显示背景底板"
                      onToggle={() =>
                        void updateHubSettings({ widgetBackground: !settings.widgetBackground })
                      }
                    />
                  </div>

                  <div className="p-3.5 flex items-center justify-between gap-3" style={{ borderColor: 'var(--c-border)' }}>
                    <div>
                      <div className="text-xs font-medium">使用品牌色</div>
                      <div className="text-[11px] mt-0.5" style={{ color: 'var(--c-ink-faint)' }}>
                        按设备品牌区分指示条颜色
                      </div>
                    </div>
                    <ToggleSwitch
                      checked={settings.brandAccent}
                      ariaLabel="使用品牌色"
                      onToggle={() =>
                        void updateHubSettings({ brandAccent: !settings.brandAccent })
                      }
                    />
                  </div>

                  {settings.monitors.length > 0 && (
                    <div className="p-3.5 flex items-center justify-between gap-3" style={{ borderColor: 'var(--c-border)' }}>
                      <div className="text-xs font-medium">显示屏幕</div>
                      <select
                        value={settings.widgetMonitor}
                        onChange={(e) =>
                          void updateHubSettings({ widgetMonitor: e.target.value })
                        }
                        className="h-7 px-2 rounded-md border text-xs max-w-[150px] cursor-pointer"
                        style={{
                          backgroundColor: 'var(--c-panel)',
                          borderColor: 'var(--c-border-strong)',
                          color: 'var(--c-ink)',
                        }}
                        aria-label="显示屏幕"
                      >
                        {settings.monitors.map((m) => (
                          <option key={m.id} value={m.id}>
                            {m.label}
                          </option>
                        ))}
                      </select>
                    </div>
                  )}
                </div>
              </section>

              {/* System & Notification */}
              <section className="space-y-2.5">
                <div
                  className="flex items-center gap-1.5 text-xs font-semibold tracking-wider"
                  style={{ color: 'var(--c-ink-faint)' }}
                >
                  <Power className="w-3.5 h-3.5" />
                  系统与提醒
                </div>
                <div
                  className="rounded-xl border divide-y"
                  style={{
                    backgroundColor: 'var(--c-panel-subtle)',
                    borderColor: 'var(--c-border)',
                  }}
                >
                  <div className="p-3.5 flex items-center justify-between gap-3" style={{ borderColor: 'var(--c-border)' }}>
                    <div className="text-xs font-medium">开机自动启动</div>
                    <ToggleSwitch
                      checked={settings.autoStart}
                      ariaLabel="开机自动启动"
                      onToggle={() => void updateHubSettings({ autoStart: !settings.autoStart })}
                    />
                  </div>

                  <div className="p-3.5 space-y-3" style={{ borderColor: 'var(--c-border)' }}>
                    <div className="flex items-center justify-between gap-3">
                      <div className="text-xs font-medium">低电量提醒</div>
                      <ToggleSwitch
                        checked={settings.lowBatteryNotify}
                        ariaLabel="低电量提醒"
                        onToggle={() =>
                          void updateHubSettings({ lowBatteryNotify: !settings.lowBatteryNotify })
                        }
                      />
                    </div>

                    <div className="space-y-1.5">
                      <div className="flex items-center justify-between text-[11px]">
                        <span style={{ color: 'var(--c-ink-faint)' }}>提醒阈值</span>
                        <span className="c-num font-semibold">{settings.lowBatteryThreshold}%</span>
                      </div>
                      <SegmentedTrack
                        ariaLabel="提醒阈值"
                        labels={THRESHOLD_PRESETS.map((v) => `${v}%`)}
                        activeIndex={nearestPresetIndex(THRESHOLD_PRESETS, settings.lowBatteryThreshold)}
                        itemPaddingClass="py-1"
                        textClass="text-[11px]"
                        onCommit={(idx) => {
                          const val = THRESHOLD_PRESETS[idx];
                          if (val !== settings.lowBatteryThreshold) {
                            void updateHubSettings({ lowBatteryThreshold: val });
                          }
                        }}
                      />
                    </div>

                    <div className="space-y-1.5">
                      <div className="flex items-center justify-between text-[11px]">
                        <span style={{ color: 'var(--c-ink-faint)' }}>轮询间隔</span>
                        <span className="c-num font-semibold">{settings.pollIntervalSec}s</span>
                      </div>
                      <SegmentedTrack
                        ariaLabel="轮询间隔"
                        labels={POLL_PRESETS.map((v) => (v >= 60 ? `${v / 60}m` : `${v}s`))}
                        activeIndex={nearestPresetIndex(POLL_PRESETS, settings.pollIntervalSec)}
                        itemPaddingClass="py-1"
                        textClass="text-[11px]"
                        onCommit={(idx) => {
                          const val = POLL_PRESETS[idx];
                          if (val !== settings.pollIntervalSec) {
                            void updateHubSettings({ pollIntervalSec: val });
                          }
                        }}
                      />
                    </div>

                    <button
                      type="button"
                      onClick={() =>
                        void bridgeCommands
                          .testLowBatteryNotify()
                          .then(() => pushToast('ok', '已发送测试通知'))
                          .catch((err) =>
                            pushToast('warn', err instanceof Error ? err.message : String(err)),
                          )
                      }
                      className="w-full h-7 rounded-md border text-xs inline-flex items-center justify-center gap-1.5 cursor-pointer bg-[var(--c-panel)] hover:bg-[var(--c-hover)] border-[var(--c-border)] transition-colors"
                    >
                      <Bell className="w-3.5 h-3.5" />
                      测试通知
                    </button>
                  </div>
                </div>
              </section>

              {/* Per-device taskbar visibility */}
              {devices.length > 0 && (
                <section className="space-y-2.5">
                  <div
                    className="flex items-center gap-1.5 text-xs font-semibold tracking-wider"
                    style={{ color: 'var(--c-ink-faint)' }}
                  >
                    <Eye className="w-3.5 h-3.5" />
                    任务栏设备显示
                  </div>
                  <div
                    className="rounded-xl border divide-y"
                    style={{
                      backgroundColor: 'var(--c-panel-subtle)',
                      borderColor: 'var(--c-border)',
                    }}
                  >
                    {devices.map((dev) => {
                      const visible = !settings.hiddenDevices.includes(dev.key);
                      return (
                        <div
                          key={dev.key}
                          className="p-3.5 flex items-center justify-between gap-3"
                          style={{ borderColor: 'var(--c-border)' }}
                        >
                          <div className="flex items-center gap-2 min-w-0">
                            <DeviceKindIcon
                              kind={dev.kind}
                              className="w-3.5 h-3.5 shrink-0 opacity-75"
                            />
                            <span className="text-xs font-medium truncate">{dev.name}</span>
                          </div>
                          <ToggleSwitch
                            checked={visible}
                            ariaLabel={`在任务栏显示 ${dev.name}`}
                            onToggle={() => {
                              const nextHidden = visible
                                ? [...settings.hiddenDevices, dev.key]
                                : settings.hiddenDevices.filter((k) => k !== dev.key);
                              void updateHubSettings({ hiddenDevices: nextHidden });
                            }}
                          />
                        </div>
                      );
                    })}
                  </div>
                </section>
              )}
            </>
          )}
        </div>
      </div>
      )}
    </dialog>
  );
}
