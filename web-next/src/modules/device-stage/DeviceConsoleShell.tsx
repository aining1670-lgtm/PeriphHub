import { useEffect, useState } from 'react';
import { resolveDeviceConsole } from '../../devices/registry';
import { useHubStore } from '../../state/hub-store';
import { KeyboardWorkbench } from '../keyboard/KeyboardWorkbench';
import { MacroStudio } from '../macros/MacroStudio';
import { MouseWorkbench } from '../mouse/MouseWorkbench';
import { GenericDeviceBench } from './GenericDeviceBench';

export function DeviceConsoleShell({
  deviceKey,
  activeMode,
}: {
  deviceKey: string;
  activeMode?: string;
}) {
  const devices = useHubStore((s) => s.devices);
  const deviceInfoMap = useHubStore((s) => s.deviceInfoMap);
  const setActiveMode = useHubStore((s) => s.setActiveMode);
  const selectShelf = useHubStore((s) => s.selectShelf);
  const refreshDevices = useHubStore((s) => s.refreshDevices);
  const [mountedMacroKeys, setMountedMacroKeys] = useState<Record<string, boolean>>({});

  const device = devices.find((d) => d.key === deviceKey);
  const info = device ? deviceInfoMap[device.key] : undefined;
  const resolved = device ? resolveDeviceConsole(device, info) : null;
  const currentTab = resolved
    ? (resolved.tabs.find((t) => t.id === activeMode) ?? resolved.tabs[0])
    : null;
  const isMacros = currentTab?.id === 'macros';

  useEffect(() => {
    if (device && isMacros) {
      setMountedMacroKeys((prev) => (prev[device.key] ? prev : { ...prev, [device.key]: true }));
    }
  }, [device, isMacros]);

  if (!device || !resolved || !currentTab) {
    return (
      <div className="max-w-md mx-auto flex-1 flex flex-col items-center justify-center py-16 text-center space-y-4">
        <p className="text-xs" style={{ color: 'var(--c-ink-muted)' }}>
          未找到该设备，可能已断开连接。
        </p>
        <div className="flex items-center gap-2.5">
          <button
            type="button"
            onClick={selectShelf}
            className="h-8 px-3.5 rounded-lg text-xs font-medium cursor-pointer bg-[var(--c-elev)] hover:bg-[var(--c-hover)] border border-[var(--c-border)] transition-colors"
            style={{ color: 'var(--c-ink)' }}
          >
            返回陈列架
          </button>
          <button
            type="button"
            onClick={() => void refreshDevices(true)}
            className="h-8 px-3.5 rounded-lg text-xs font-medium cursor-pointer transition-colors"
            style={{ backgroundColor: 'var(--c-hi)', color: 'var(--c-hi-inv)' }}
          >
            重新扫描
          </button>
        </div>
      </div>
    );
  }

  if (resolved.category === 'generic') {
    return <GenericDeviceBench device={device} />;
  }

  const macroMounted = Boolean(mountedMacroKeys[device.key]) || isMacros;

  return (
    <div className="w-full flex-1 flex flex-col">
      {/* Compact mobile mode switcher (desktop uses top header underline tabs) */}
      {resolved.tabs.length > 1 && (
        <nav
          className="flex md:hidden flex-wrap gap-1 p-1 mt-3 rounded-xl border"
          style={{ backgroundColor: 'var(--c-elev)', borderColor: 'var(--c-border)' }}
        >
          {resolved.tabs.map((tab) => {
            const active = tab.id === currentTab.id;
            return (
              <button
                key={tab.id}
                type="button"
                onClick={() => setActiveMode(tab.id)}
                className="px-3 py-1.5 rounded-lg text-xs font-medium cursor-pointer"
                style={{
                  backgroundColor: active ? 'var(--c-hi)' : 'transparent',
                  color: active ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                }}
              >
                {tab.label}
              </button>
            );
          })}
        </nav>
      )}

      <div className={isMacros ? 'hidden' : 'flex-1 flex flex-col'}>
        {resolved.category === 'mouse-pro' ? (
          <MouseWorkbench
            key={device.key}
            device={device}
            activeMode={isMacros ? '__hidden__' : currentTab.id}
          />
        ) : (
          <KeyboardWorkbench
            key={device.key}
            device={device}
            activeMode={isMacros ? '__hidden__' : currentTab.id}
          />
        )}
      </div>

      {macroMounted && (
        <div className={isMacros ? 'flex-1 flex flex-col tab-enter' : 'hidden'}>
          <MacroStudio
            key={device.key}
            deviceKey={device.key}
            mode={resolved.category === 'mouse-pro' ? 'mouse' : 'keyboard'}
          />
        </div>
      )}
    </div>
  );
}
