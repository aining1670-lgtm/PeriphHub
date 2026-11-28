import type { DeviceCapabilities, DeviceEntry, DeviceInfoResponse } from '../bridge/types';

export type ConsoleCategory = 'mouse-pro' | 'keyboard-pro' | 'generic';

export interface ConsoleTab {
  id: string;
  label: string;
}

export interface ResolvedConsole {
  category: ConsoleCategory;
  driverLabel: string;
  capabilities: DeviceCapabilities | null;
  tabs: ConsoleTab[];
  defaultMode: string;
}

function formatDriverLabel(driver: string, suffix: string): string {
  if (driver === 'royuan') return 'ROYUAN 键盘驱动';
  if (driver === 'incott') return 'Incott 驱动';
  if (driver === 'yjx') return 'YJX 驱动';
  if (!driver || driver === 'none') return suffix;
  return `${driver.toUpperCase()} ${suffix}`;
}

export function resolveDeviceConsole(
  device: DeviceEntry,
  info?: DeviceInfoResponse,
): ResolvedConsole {
  const driver = info?.driver ?? device.driver;
  const caps = info?.capabilities ?? null;

  const isKeyboard = Boolean(caps?.keyboard || (!caps && driver === 'royuan'));
  if (isKeyboard) {
    const kbd = caps?.keyboard;
    const tabs: ConsoleTab[] = [];
    if (!kbd || kbd.keymap !== false) {
      tabs.push({ id: 'keymap', label: '按键映射' });
    }
    if (!kbd || kbd.mainModes.length > 0 || kbd.sideModes.length > 0) {
      tabs.push({ id: 'lighting', label: '背光系统' });
    }
    if (!kbd || (kbd.macros && kbd.macros.maxSlots > 0)) {
      tabs.push({ id: 'macros', label: '宏' });
    }
    if (!kbd || kbd.debounce || kbd.sleep || kbd.options.length > 0) {
      tabs.push({ id: 'power', label: '高级设置' });
    }
    if (tabs.length === 0) {
      tabs.push({ id: 'keymap', label: '按键映射' });
    }

    return {
      category: 'keyboard-pro',
      driverLabel: formatDriverLabel(driver, '键盘驱动'),
      capabilities: caps,
      tabs,
      defaultMode: tabs[0].id,
    };
  }

  const hasMouseCaps = Boolean(
    caps &&
      (caps.dpi ||
        caps.rates.length > 0 ||
        caps.sensor ||
        caps.misc ||
        caps.lighting ||
        caps.keys ||
        caps.macros),
  );
  const isMouse = Boolean(
    hasMouseCaps ||
      (!caps &&
        (driver === 'incott' ||
          driver === 'yjx' ||
          (device.driverLevel === 'full' && device.kind === 'mouse'))),
  );

  if (isMouse) {
    const tabs: ConsoleTab[] = [];
    if (!caps || caps.dpi || caps.rates.length > 0 || caps.sensor) {
      tabs.push({ id: 'performance', label: '性能调校' });
    }
    if (!caps || caps.keys) {
      tabs.push({ id: 'keymap', label: '按键映射' });
    }
    if (!caps || caps.macros) {
      tabs.push({ id: 'macros', label: '宏' });
    }
    if (
      !caps ||
      caps.misc ||
      caps.lighting ||
      (caps.sensor && (caps.sensor.motion || caps.sensor.fixline || caps.sensor.ripple))
    ) {
      tabs.push({ id: 'advanced', label: '高级设置' });
    }
    if (tabs.length === 0) {
      tabs.push({ id: 'performance', label: '性能调校' });
    }

    return {
      category: 'mouse-pro',
      driverLabel: formatDriverLabel(driver, '驱动'),
      capabilities: caps,
      tabs,
      defaultMode: tabs[0].id,
    };
  }

  return {
    category: 'generic',
    driverLabel: '通用电量遥测',
    capabilities: caps,
    tabs: [{ id: 'telemetry', label: '遥测与连接' }],
    defaultMode: 'telemetry',
  };
}
