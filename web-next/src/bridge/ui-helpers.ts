import { BridgeError } from './transport';
import type { SyncState, ToastItem } from '../state/hub-store';

export const MODIFIER_KEY_CODES = new Set([
  'ControlLeft',
  'ControlRight',
  'ShiftLeft',
  'ShiftRight',
  'AltLeft',
  'AltRight',
  'MetaLeft',
  'MetaRight',
]);

export function classifyBridgeError(
  err: unknown,
  sleepHint = '设备休眠中，请唤醒后重试',
): { state: 'sleeping' | 'error'; message: string; isSleeping: boolean } {
  if (err instanceof BridgeError && err.isSleeping) {
    return { state: 'sleeping', message: sleepHint, isSleeping: true };
  }
  const msg = err instanceof Error ? err.message : String(err);
  return { state: 'error', message: msg, isSleeping: false };
}

export function reportBridgeError(
  setSyncStatus: (state: SyncState, message?: string | null) => void,
  pushToast: (tone: ToastItem['tone'], message: string) => void,
  actionLabel: string,
  err: unknown,
  sleepHint = '设备休眠中，请唤醒后重试',
): void {
  const { state, message, isSleeping } = classifyBridgeError(err, sleepHint);
  setSyncStatus(state, message);
  pushToast(isSleeping ? 'warn' : 'error', isSleeping ? message : `${actionLabel}失败：${message}`);
}
