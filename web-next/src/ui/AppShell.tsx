import { type MouseEvent, type ReactNode, useEffect, useRef, useState } from 'react';
import {
  ArrowLeft,
  Copy,
  Minus,
  Moon,
  Pencil,
  RefreshCw,
  RotateCcw,
  Settings,
  Square,
  Sun,
  X,
} from 'lucide-react';
import { bridgeCommands } from '../bridge/commands';
import { isWebViewHost } from '../bridge/transport';
import { resolveDeviceConsole } from '../devices/registry';
import { useHubStore } from '../state/hub-store';
import { useThemeStore } from '../state/theme-store';
import { DeviceKindIcon, formatBatteryText, getBatteryTone, toneCssColor } from './DeviceGlyph';
import { SettingsDrawer } from './SettingsDrawer';

function SyncStatusPill() {
  const syncState = useHubStore((s) => s.syncState);
  const syncMessage = useHubStore((s) => s.syncMessage);

  const label =
    syncState === 'reading'
      ? '读取中'
      : syncState === 'writing'
        ? '正在写入'
        : syncState === 'sleeping'
          ? '浅休眠'
          : syncState === 'error'
            ? '连接异常'
            : syncMessage || '已同步';

  const dotColor =
    syncState === 'reading' || syncState === 'writing'
      ? 'var(--c-accent)'
      : syncState === 'sleeping'
        ? 'var(--c-warn)'
        : syncState === 'error'
          ? 'var(--c-danger)'
          : 'var(--c-ok)';

  return (
    <div
      className="hidden md:flex items-center gap-2 text-xs c-num"
      style={{ color: 'var(--c-ink-muted)' }}
      title={syncMessage ?? label}
    >
      <span
        className={`w-1.5 h-1.5 rounded-full ${
          syncState === 'reading' || syncState === 'writing' ? 'motion-safe:animate-pulse' : ''
        }`}
        style={{ backgroundColor: dotColor }}
      />
      <span>{label}</span>
    </div>
  );
}

function InnerDeviceDock() {
  const devices = useHubStore((s) => s.devices);
  const activeView = useHubStore((s) => s.activeView);
  const selectDevice = useHubStore((s) => s.selectDevice);

  if (activeView.kind !== 'device' || devices.length <= 1) return null;

  return (
    <div
      className="fixed bottom-4 left-1/2 -translate-x-1/2 z-30 max-w-[94vw]"
    >
      <nav
        aria-label="快速切换设备"
        className="hw-dock dock-tabs rounded-full p-1 flex items-center gap-0.5 overflow-x-auto"
      >
        {devices.map((dev) => {
          const isCurrent = dev.key === activeView.key;
          const batt = formatBatteryText(dev);
          const tone = toneCssColor(getBatteryTone(dev));
          return (
            <button
              key={dev.key}
              type="button"
              data-active={isCurrent ? 'true' : undefined}
              aria-current={isCurrent ? 'page' : undefined}
              onClick={() => {
                if (!isCurrent) void selectDevice(dev.key);
              }}
              className={`h-8 px-3 rounded-full inline-flex items-center gap-2 text-xs cursor-pointer transition-colors shrink-0 border border-transparent ${
                isCurrent ? '' : 'hover:bg-[var(--c-hover)]'
              }`}
              style={{
                color: isCurrent ? 'var(--c-ink)' : 'var(--c-ink-muted)',
              }}
            >
              <DeviceKindIcon kind={dev.kind} className="w-3.5 h-3.5 shrink-0 opacity-85" />
              <span className={`truncate max-w-[136px] ${isCurrent ? 'font-semibold' : 'font-medium'}`}>
                {dev.name}
              </span>
              <span className="c-num text-[11.5px] font-semibold shrink-0" style={{ color: tone }}>
                {batt}
              </span>
            </button>
          );
        })}
      </nav>
    </div>
  );
}

export function AppShell({ children }: { children: ReactNode }) {
  const devices = useHubStore((s) => s.devices);
  const deviceInfoMap = useHubStore((s) => s.deviceInfoMap);
  const hubSettings = useHubStore((s) => s.hubSettings);
  const activeView = useHubStore((s) => s.activeView);
  const isMockMode = useHubStore((s) => s.isMockMode);
  const syncState = useHubStore((s) => s.syncState);
  const toasts = useHubStore((s) => s.toasts);
  const windowMaximized = useHubStore((s) => s.windowMaximized);
  const setWindowMaximized = useHubStore((s) => s.setWindowMaximized);
  const selectShelf = useHubStore((s) => s.selectShelf);
  const setActiveMode = useHubStore((s) => s.setActiveMode);
  const toggleSettings = useHubStore((s) => s.toggleSettings);
  const refreshDevices = useHubStore((s) => s.refreshDevices);
  const renameDevice = useHubStore((s) => s.renameDevice);
  const dismissToast = useHubStore((s) => s.dismissToast);

  const [renamingKey, setRenamingKey] = useState<string | null>(null);
  const [nameDraft, setNameDraft] = useState('');
  const toastPopoverRef = useRef<HTMLDivElement>(null);
  const latestToastId = toasts.length > 0 ? toasts[toasts.length - 1].id : null;

  useEffect(() => {
    const el = toastPopoverRef.current;
    if (!el || typeof el.showPopover !== 'function') return;
    try {
      if (latestToastId !== null) {
        if (el.matches(':popover-open')) {
          el.hidePopover();
        }
        el.showPopover();
      } else if (el.matches(':popover-open')) {
        el.hidePopover();
      }
    } catch {
      // Ignore popover state errors in unsupported runtimes.
    }
  }, [latestToastId]);

  const isDark = useThemeStore((s) => s.isDark);
  const toggleLightDark = useThemeStore((s) => s.toggleLightDark);
  const nativeHost = isWebViewHost();

  useEffect(() => {
    if (!nativeHost) return;
    void bridgeCommands
      .windowState()
      .then((res) => {
        if (typeof res.maximized === 'boolean') {
          setWindowMaximized(res.maximized);
        }
      })
      .catch(() => undefined);
  }, [nativeHost, setWindowMaximized]);

  const handleToggleMaximize = () => {
    if (!nativeHost) return;
    void bridgeCommands
      .windowMaximize()
      .then((res) => {
        if (typeof res.maximized === 'boolean') {
          setWindowMaximized(res.maximized);
        }
      })
      .catch(() => undefined);
  };

  const isInteractiveTarget = (target: EventTarget | null): boolean =>
    target instanceof Element && Boolean(target.closest('button, a, input, select, [role="tab"]'));

  const onHeaderMouseDown = (e: MouseEvent<HTMLElement>) => {
    if (!nativeHost || e.button !== 0 || e.detail > 1) return;
    if (isInteractiveTarget(e.target)) return;
    void bridgeCommands.windowDrag().catch(() => undefined);
  };

  const onHeaderDoubleClick = (e: MouseEvent<HTMLElement>) => {
    if (!nativeHost || isInteractiveTarget(e.target)) return;
    handleToggleMaximize();
  };

  const onHeaderContextMenu = (e: MouseEvent<HTMLElement>) => {
    if (!nativeHost) return;
    e.preventDefault();
    void bridgeCommands
      .windowSysMenu(Math.round(e.clientX), Math.round(e.clientY))
      .catch(() => undefined);
  };

  const activeDevice =
    activeView.kind === 'device'
      ? devices.find((d) => d.key === activeView.key) ?? null
      : null;
  const resolvedConsole = activeDevice
    ? resolveDeviceConsole(activeDevice, deviceInfoMap[activeDevice.key])
    : null;
  const currentTabId =
    activeView.kind === 'device'
      ? activeView.mode ?? resolvedConsole?.defaultMode
      : undefined;

  return (
    <div className="h-screen w-screen flex flex-col overflow-hidden select-none c-studio-wall">
      {/* Clean Integrated Transparent Header (matching Titanium Mono v3) */}
      <header
        onMouseDown={onHeaderMouseDown}
        onDoubleClick={onHeaderDoubleClick}
        onContextMenu={onHeaderContextMenu}
        className="relative z-20 h-14 px-5 sm:px-8 shrink-0 grid grid-cols-[minmax(0,1fr)_auto] md:grid-cols-[minmax(0,1fr)_auto_minmax(0,1fr)] items-center gap-3 border-b"
        style={{ borderColor: 'var(--c-border)' }}
      >
        {/* Left: Pure Wordmark on Shelf OR Frameless Breadcrumb inside Device */}
        <div className="flex items-center gap-1.5 min-w-0">
          {activeView.kind === 'shelf' ? (
            <div className="flex items-center gap-2.5">
              <span className="text-[14px] tracking-[-0.02em] flex items-baseline">
                <span className="font-semibold" style={{ color: 'var(--c-ink)' }}>
                  Periph
                </span>
                <span className="font-normal" style={{ color: 'var(--c-ink-muted)' }}>
                  Hub
                </span>
              </span>
              {isMockMode && (
                <span
                  className="text-[10px] px-1.5 py-0.5 rounded font-medium"
                  style={{
                    backgroundColor: 'var(--c-warn-soft)',
                    color: 'var(--c-warn)',
                  }}
                >
                  演示
                </span>
              )}
            </div>
          ) : (
            <>
              <button
                type="button"
                onClick={selectShelf}
                className="h-7 px-2 -ml-2 rounded-md text-xs font-medium inline-flex items-center gap-1.5 cursor-pointer hover:bg-[var(--c-hover)] shrink-0 transition-colors"
                style={{ color: 'var(--c-ink-muted)' }}
              >
                <ArrowLeft className="w-3.5 h-3.5" />
                <span>陈列架</span>
              </button>
              <span className="text-xs select-none" style={{ color: 'var(--c-ink-faint)' }}>
                /
              </span>
              {activeDevice && (
                renamingKey === activeDevice.key ? (
                  <div className="flex items-center gap-1">
                    <input
                      type="text"
                      autoFocus
                      maxLength={40}
                      value={nameDraft}
                      onChange={(e) => setNameDraft(e.target.value)}
                      onBlur={() => {
                        const trimmed = nameDraft.trim();
                        setRenamingKey(null);
                        if (trimmed && trimmed !== activeDevice.name) {
                          void renameDevice(activeDevice.key, trimmed);
                        }
                      }}
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') {
                          if (e.nativeEvent.isComposing || e.keyCode === 229) return;
                          e.preventDefault();
                          e.currentTarget.blur();
                        } else if (e.key === 'Escape') {
                          setRenamingKey(null);
                        }
                      }}
                      className="h-6 w-40 px-2 rounded text-xs font-medium"
                      style={{
                        backgroundColor: 'var(--c-elev-2)',
                        border: '1px solid var(--c-hi)',
                        color: 'var(--c-ink)',
                      }}
                      aria-label="编辑设备名称"
                    />
                  </div>
                ) : (
                  <div className="flex items-center gap-1 min-w-0">
                    <span
                      onDoubleClick={() => {
                        setNameDraft(activeDevice.name);
                        setRenamingKey(activeDevice.key);
                      }}
                      className="text-xs font-medium truncate max-w-[130px] lg:max-w-[180px] cursor-text"
                      style={{ color: 'var(--c-ink)' }}
                      title="双击重命名设备"
                    >
                      {activeDevice.name}
                    </span>
                    <button
                      type="button"
                      onClick={() => {
                        setNameDraft(activeDevice.name);
                        setRenamingKey(activeDevice.key);
                      }}
                      className="w-6 h-6 rounded flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] opacity-60 hover:opacity-100 transition-opacity shrink-0"
                      style={{ color: 'var(--c-ink-muted)' }}
                      title="重命名设备"
                      aria-label="重命名设备"
                    >
                      <Pencil className="w-3 h-3" />
                    </button>
                    {Boolean(hubSettings?.deviceAliases?.[activeDevice.key]) && (
                      <button
                        type="button"
                        onClick={() => void renameDevice(activeDevice.key, '')}
                        className="w-6 h-6 rounded flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] opacity-60 hover:opacity-100 transition-opacity shrink-0"
                        style={{ color: 'var(--c-ink-muted)' }}
                        title="恢复原名"
                        aria-label="恢复原名"
                      >
                        <RotateCcw className="w-3 h-3" />
                      </button>
                    )}
                  </div>
                )
              )}
            </>
          )}
        </div>

        {/* Center: Geometrically Centered Underline Mode Tabs */}
        {activeView.kind === 'device' && resolvedConsole ? (
          <nav
            aria-label="设备功能切换"
            className="top-mode-tabs relative hidden md:flex items-center gap-4 lg:gap-7 h-full justify-self-center"
          >
            {resolvedConsole.tabs.length > 1 ? (
              resolvedConsole.tabs.map((tab) => {
                const active = tab.id === currentTabId;
                return (
                  <button
                    key={tab.id}
                    type="button"
                    data-active={active ? 'true' : undefined}
                    aria-current={active ? 'page' : undefined}
                    onClick={() => setActiveMode(tab.id)}
                    className="h-full text-xs font-medium cursor-pointer relative px-1 transition-colors"
                    style={{
                      color: active ? 'var(--c-ink)' : 'var(--c-ink-faint)',
                    }}
                  >
                    {tab.label}
                  </button>
                );
              })
            ) : (
              <span className="text-xs font-medium" style={{ color: 'var(--c-ink-faint)' }}>
                电量监控
              </span>
            )}
          </nav>
        ) : (
          <div className="hidden md:block" aria-hidden="true" />
        )}

        {/* Right: Quiet Sync Status + Frameless Utility Icon Group */}
        <div className="flex items-center gap-3 shrink-0 justify-self-end">
          <SyncStatusPill />

          <span
            className="hidden md:block w-px h-3.5"
            style={{ backgroundColor: 'var(--c-border-strong)' }}
          />

          <div className="flex items-center gap-0.5">
            <button
              type="button"
              onClick={() => void refreshDevices(true)}
              disabled={syncState === 'reading'}
              className="w-8 h-8 rounded-lg flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-40 transition-colors"
              style={{ color: 'var(--c-ink-muted)' }}
              title="重新扫描设备"
              aria-label="重新扫描设备"
            >
              <RefreshCw className={`w-3.5 h-3.5 ${syncState === 'reading' ? 'motion-safe:animate-spin' : ''}`} />
            </button>

            <button
              type="button"
              onClick={toggleLightDark}
              className="w-8 h-8 rounded-lg flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
              style={{ color: 'var(--c-ink-muted)' }}
              title={isDark ? '切换为浅色模式' : '切换为深色模式'}
              aria-label={isDark ? '切换为浅色模式' : '切换为深色模式'}
            >
              {isDark ? <Sun className="w-3.5 h-3.5" /> : <Moon className="w-3.5 h-3.5" />}
            </button>

            <button
              type="button"
              onClick={toggleSettings}
              className="w-8 h-8 rounded-lg flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
              style={{ color: 'var(--c-ink-muted)' }}
              title="设置"
              aria-label="设置"
            >
              <Settings className="w-3.5 h-3.5" />
            </button>
          </div>

          {nativeHost && (
            <>
              <span
                className="w-px h-3.5"
                style={{ backgroundColor: 'var(--c-border-strong)' }}
              />
              <div className="flex items-center gap-0.5 -mr-2">
                <button
                  type="button"
                  onClick={() => void bridgeCommands.windowMinimize().catch(() => undefined)}
                  className="w-8 h-8 rounded-lg flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
                  style={{ color: 'var(--c-ink-muted)' }}
                  title="最小化"
                  aria-label="最小化"
                >
                  <Minus className="w-3.5 h-3.5" />
                </button>
                <button
                  type="button"
                  onClick={handleToggleMaximize}
                  className="w-8 h-8 rounded-lg flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
                  style={{ color: 'var(--c-ink-muted)' }}
                  title={windowMaximized ? '向下还原' : '最大化'}
                  aria-label={windowMaximized ? '向下还原' : '最大化'}
                >
                  {windowMaximized ? (
                    <Copy className="w-3 h-3" />
                  ) : (
                    <Square className="w-3 h-3" />
                  )}
                </button>
                <button
                  type="button"
                  onClick={() => void bridgeCommands.windowClose().catch(() => undefined)}
                  className="w-8 h-8 rounded-lg flex items-center justify-center cursor-pointer hover:bg-[#e81123] hover:text-white transition-colors"
                  style={{ color: 'var(--c-ink-muted)' }}
                  title="关闭"
                  aria-label="关闭"
                >
                  <X className="w-3.5 h-3.5" />
                </button>
              </div>
            </>
          )}
        </div>
      </header>

      {/* Full-Width Stage */}
      <main
        id="app-stage-root"
        tabIndex={-1}
        className={`relative z-10 flex-1 flex flex-col overflow-y-auto px-4 sm:px-8 focus:outline-none ${
          activeView.kind === 'device' ? 'pb-20' : ''
        }`}
        style={{ scrollbarGutter: 'stable' }}
      >
        {children}
      </main>

      <InnerDeviceDock />
      <SettingsDrawer />

      {/* Toast Stack (manual popover so toasts render above modal <dialog> in Top Layer) */}
      <div
        ref={toastPopoverRef}
        role="region"
        aria-label="系统通知"
        aria-live="polite"
        className={`m-0 p-0 border-0 bg-transparent overflow-visible inset-auto fixed ${
          activeView.kind === 'device' && devices.length > 1 ? 'bottom-16 right-4' : 'bottom-4 right-4'
        } z-50 max-w-sm`}
        {...({ popover: 'manual' } as Record<string, unknown>)}
      >
        {toasts.length > 0 && (
          <div className="flex flex-col gap-2">
            {toasts.map((toast) => {
              const color =
                toast.tone === 'ok'
                  ? 'var(--c-ok)'
                  : toast.tone === 'warn'
                    ? 'var(--c-warn)'
                    : toast.tone === 'error'
                      ? 'var(--c-danger)'
                      : 'var(--c-accent)';
              return (
                <div
                  key={toast.id}
                  onClick={() => dismissToast(toast.id)}
                  className="c-card rounded-lg px-3.5 py-2.5 text-xs flex items-center gap-2.5 cursor-pointer"
                  style={{ borderLeftWidth: '3px', borderLeftColor: color }}
                >
                  <span className="w-2 h-2 rounded-full shrink-0" style={{ backgroundColor: color }} />
                  <span style={{ color: 'var(--c-ink)' }}>{toast.message}</span>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* 8-Direction Frameless Window Resize Handles */}
      {nativeHost && !windowMaximized && (
        <>
          {(
            [
              ['left', 'fixed left-0 top-2.5 bottom-2.5 w-1 z-50 cursor-ew-resize'],
              ['right', 'fixed right-0 top-2.5 bottom-2.5 w-1 z-50 cursor-ew-resize'],
              ['top', 'fixed top-0 left-2.5 right-2.5 h-1 z-50 cursor-ns-resize'],
              ['bottom', 'fixed bottom-0 left-2.5 right-2.5 h-1 z-50 cursor-ns-resize'],
              ['topLeft', 'fixed top-0 left-0 w-2.5 h-2.5 z-50 cursor-nwse-resize'],
              ['topRight', 'fixed top-0 right-0 w-2.5 h-2.5 z-50 cursor-nesw-resize'],
              ['bottomLeft', 'fixed bottom-0 left-0 w-2.5 h-2.5 z-50 cursor-nesw-resize'],
              ['bottomRight', 'fixed bottom-0 right-0 w-2.5 h-2.5 z-50 cursor-nwse-resize'],
            ] as const
          ).map(([edge, cls]) => (
            <div
              key={edge}
              aria-hidden="true"
              onMouseDown={(e) => {
                if (e.button !== 0) return;
                e.preventDefault();
                void bridgeCommands.windowResize(edge).catch(() => undefined);
              }}
              className={cls}
            />
          ))}
        </>
      )}
    </div>
  );
}
