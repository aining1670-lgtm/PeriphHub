import type { BridgeRequest, BridgeResponse, HostEvent } from './types';

interface WebView2Global {
  postMessage: (message: unknown) => void;
  addEventListener: (type: 'message', listener: (ev: MessageEvent) => void) => void;
  removeEventListener: (type: 'message', listener: (ev: MessageEvent) => void) => void;
}

declare global {
  interface Chrome {
    webview?: WebView2Global;
  }
  interface Window {
    chrome?: Chrome;
  }
}

export interface InvokeOptions {
  /** Serialize mutating HID calls so Feature/Output reports never interleave. */
  serialized?: boolean;
  /** Override request timeout in milliseconds (default 15000). */
  timeoutMs?: number;
}

export class BridgeError extends Error {
  readonly cmd: string;
  readonly isSleeping: boolean;

  constructor(cmd: string, message: string) {
    super(message);
    this.name = 'BridgeError';
    this.cmd = cmd;
    this.isSleeping = /sleeping|wake/i.test(message);
  }
}

type EventListener = (event: HostEvent) => void;

const DEFAULT_TIMEOUT_MS = 15_000;

let nextId = 1;
let initialized = false;
let writeChain: Promise<unknown> = Promise.resolve();

const pending = new Map<
  number,
  {
    cmd: string;
    resolve: (data: unknown) => void;
    reject: (err: Error) => void;
    timer: ReturnType<typeof setTimeout>;
  }
>();

const eventListeners = new Set<EventListener>();

export function isWebViewHost(): boolean {
  return typeof window !== 'undefined' && typeof window.chrome?.webview?.postMessage === 'function';
}

function handleIncomingMessage(raw: unknown): void {
  let msg = raw;
  if (typeof msg === 'string') {
    try {
      msg = JSON.parse(msg);
    } catch {
      return;
    }
  }
  if (!msg || typeof msg !== 'object') return;
  const record = msg as Record<string, unknown>;

  if (typeof record.event === 'string') {
    const evt = record as unknown as HostEvent;
    for (const listener of eventListeners) {
      try {
        listener(evt);
      } catch {
        // Isolate subscriber errors from IPC dispatch.
      }
    }
    return;
  }

  if (typeof record.id === 'number') {
    const waiter = pending.get(record.id);
    if (!waiter) return;
    pending.delete(record.id);
    clearTimeout(waiter.timer);

    const resp = record as unknown as BridgeResponse;
    if (resp.ok) {
      waiter.resolve(resp.data);
    } else {
      waiter.reject(new BridgeError(waiter.cmd, resp.error || 'unknown bridge error'));
    }
  }
}

export function initBridgeTransport(): void {
  if (initialized) return;
  initialized = true;
  if (isWebViewHost()) {
    window.chrome!.webview!.addEventListener('message', (ev) => {
      handleIncomingMessage(ev.data);
    });
  }
}

export function subscribeHostEvents(listener: EventListener): () => void {
  initBridgeTransport();
  eventListeners.add(listener);
  return () => {
    eventListeners.delete(listener);
  };
}

function sendImmediate<T>(
  cmd: string,
  args?: Record<string, unknown>,
  timeoutMs = DEFAULT_TIMEOUT_MS,
): Promise<T> {
  initBridgeTransport();
  const id = nextId++;
  const payload: BridgeRequest = args ? { id, cmd, args } : { id, cmd };

  if (!isWebViewHost()) {
    if (import.meta.env.DEV) {
      return import('./mock-host').then(({ handleMockRequest }) =>
        handleMockRequest(payload).then((resp) => {
          if (!resp.ok) {
            throw new BridgeError(cmd, resp.error);
          }
          return resp.data as T;
        }),
      );
    }
    return Promise.reject(new BridgeError(cmd, 'WebView2 host not available'));
  }

  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new BridgeError(cmd, `IPC timeout after ${timeoutMs}ms (${cmd})`));
    }, timeoutMs);

    pending.set(id, {
      cmd,
      resolve: (data) => resolve(data as T),
      reject,
      timer,
    });

    try {
      window.chrome!.webview!.postMessage(payload);
    } catch (err) {
      pending.delete(id);
      clearTimeout(timer);
      reject(err instanceof Error ? err : new BridgeError(cmd, String(err)));
    }
  });
}

export function invoke<T>(
  cmd: string,
  args?: Record<string, unknown>,
  options: InvokeOptions = {},
): Promise<T> {
  const { serialized = false, timeoutMs = DEFAULT_TIMEOUT_MS } = options;
  if (!serialized) {
    return sendImmediate<T>(cmd, args, timeoutMs);
  }
  const queued = writeChain.then(
    () => sendImmediate<T>(cmd, args, timeoutMs),
    () => sendImmediate<T>(cmd, args, timeoutMs),
  );
  writeChain = queued.catch(() => undefined);
  return queued;
}
