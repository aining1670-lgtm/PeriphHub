import { useEffect } from 'react';
import { runSelftestIfRequested } from './bridge/selftest';
import { subscribeHostEvents } from './bridge/transport';
import { DeviceConsoleShell } from './modules/device-stage/DeviceConsoleShell';
import { HardwareShelf } from './modules/shelf/HardwareShelf';
import { useHubStore } from './state/hub-store';
import { useThemeStore } from './state/theme-store';
import { AppShell } from './ui/AppShell';

export function App() {
  const activeView = useHubStore((s) => s.activeView);
  const bootstrapped = useHubStore((s) => s.bootstrapped);
  const bootstrap = useHubStore((s) => s.bootstrap);
  const handleHostEvent = useHubStore((s) => s.handleHostEvent);
  const initThemeListener = useThemeStore((s) => s.initSystemListener);

  useEffect(() => {
    const unsubTheme = initThemeListener();
    const unsubEvents = subscribeHostEvents(handleHostEvent);
    void bootstrap().then(() =>
      runSelftestIfRequested(() => {
        const state = useHubStore.getState();
        return {
          rendered: true,
          deviceCount: state.devices.length,
          activeKey: state.activeView.kind === 'device' ? state.activeView.key : null,
        };
      }),
    );

    return () => {
      unsubTheme();
      unsubEvents();
    };
  }, [bootstrap, handleHostEvent, initThemeListener]);

  return (
    <AppShell>
      {!bootstrapped ? (
        <div className="relative flex-1 w-full max-w-5xl mx-auto flex flex-col justify-between py-4 sm:py-6">
          <div className="flex-1 flex flex-col justify-center py-2 sm:py-4">
            <div className="px-2 mb-0.5 flex items-center justify-between">
              <div className="c-skeleton h-3.5 w-20 rounded" />
              <div className="c-skeleton h-3.5 w-8 rounded" />
            </div>
            <div className="relative">
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
                <div className="w-[168px] shrink-0 flex flex-col items-center">
                  <div className="h-[132px] w-full pb-1 flex items-end justify-center">
                    <div className="c-skeleton w-20 h-24 rounded-2xl" />
                  </div>
                  <div className="w-full pt-7 px-1.5 pb-1 space-y-2">
                    <div className="c-skeleton h-3.5 w-24 rounded" />
                    <div className="c-skeleton h-[3px] w-full rounded-full" />
                  </div>
                </div>
                <div className="w-[276px] shrink-0 flex flex-col items-center">
                  <div className="h-[132px] w-full pb-1 flex items-end justify-center">
                    <div className="c-skeleton w-56 h-20 rounded-xl" />
                  </div>
                  <div className="w-full pt-7 px-1.5 pb-1 space-y-2">
                    <div className="c-skeleton h-3.5 w-32 rounded" />
                    <div className="c-skeleton h-[3px] w-full rounded-full" />
                  </div>
                </div>
              </div>
            </div>
            <p className="mt-8 text-center text-[13px]" style={{ color: 'var(--c-ink-faint)' }}>
              正在读取设备…
            </p>
          </div>

          <div
            className="px-2 pt-3.5 border-t flex items-center justify-between gap-4"
            style={{ borderColor: 'var(--c-border)' }}
          >
            <div className="c-skeleton h-3.5 w-44 rounded" />
            <div className="c-skeleton h-3.5 w-28 rounded" />
          </div>
        </div>
      ) : activeView.kind === 'shelf' ? (
        <HardwareShelf />
      ) : (
        <DeviceConsoleShell deviceKey={activeView.key} activeMode={activeView.mode} />
      )}
    </AppShell>
  );
}

export default App;
