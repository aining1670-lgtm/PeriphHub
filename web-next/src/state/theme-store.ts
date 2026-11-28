import { create } from 'zustand';
import { bridgeCommands } from '../bridge/commands';
import { isWebViewHost } from '../bridge/transport';

export type ThemeMode = 'system' | 'light' | 'dark';

const STORAGE_KEY = 'periphhub.theme';

function readStoredMode(): ThemeMode {
  if (typeof window === 'undefined') return 'system';
  try {
    const val = localStorage.getItem(STORAGE_KEY);
    if (val === 'light' || val === 'dark' || val === 'system') return val;
  } catch {
    // Ignore storage access errors.
  }
  return 'system';
}

function systemPrefersDark(): boolean {
  if (typeof window === 'undefined' || !window.matchMedia) return false;
  return window.matchMedia('(prefers-color-scheme: dark)').matches;
}

function resolveDark(mode: ThemeMode): boolean {
  if (mode === 'dark') return true;
  if (mode === 'light') return false;
  return systemPrefersDark();
}

function applyDomTheme(isDark: boolean): void {
  if (typeof document === 'undefined') return;
  document.documentElement.classList.toggle('dark', isDark);
  if (isWebViewHost()) {
    void bridgeCommands.windowTheme(isDark).catch(() => undefined);
  }
}

export interface ThemeStore {
  mode: ThemeMode;
  isDark: boolean;
  setMode: (mode: ThemeMode) => void;
  toggleLightDark: () => void;
  initSystemListener: () => () => void;
}

const initialMode = readStoredMode();
const initialDark = resolveDark(initialMode);
applyDomTheme(initialDark);

export const useThemeStore = create<ThemeStore>((set, get) => ({
  mode: initialMode,
  isDark: initialDark,

  setMode: (mode) => {
    try {
      localStorage.setItem(STORAGE_KEY, mode);
    } catch {
      // Ignore storage access errors.
    }
    const isDark = resolveDark(mode);
    applyDomTheme(isDark);
    set({ mode, isDark });
  },

  toggleLightDark: () => {
    get().setMode(get().isDark ? 'light' : 'dark');
  },

  initSystemListener: () => {
    if (typeof window === 'undefined' || !window.matchMedia) {
      return () => {};
    }
    const mq = window.matchMedia('(prefers-color-scheme: dark)');
    const onChange = () => {
      if (get().mode === 'system') {
        const isDark = mq.matches;
        applyDomTheme(isDark);
        set({ isDark });
      }
    };
    mq.addEventListener('change', onChange);
    return () => mq.removeEventListener('change', onChange);
  },
}));
