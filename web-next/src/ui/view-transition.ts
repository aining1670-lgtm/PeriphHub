import { flushSync } from 'react-dom';

type DocumentWithViewTransition = Document & {
  startViewTransition?: (updateCallback: () => void) => {
    ready: Promise<void>;
    finished: Promise<void>;
  };
};

/**
 * Produces a deterministic, CSS-valid view-transition-name unique to a device key.
 */
export function deviceHeroTransitionName(deviceKey: string): string {
  const safe = deviceKey.replace(/[^a-zA-Z0-9_-]/g, '-');
  return `dev-hero-${safe}`;
}

/**
 * Wraps a React state update in the View Transitions API with graceful fallback,
 * post-transition accessibility focus routing, and cleanup hook.
 */
export function runViewTransition(
  updateState: () => void,
  focusTargetId: string | null = 'app-stage-root',
  onFinished?: () => void,
): void {
  const doc = document as DocumentWithViewTransition;

  const finish = () => {
    if (focusTargetId) {
      const el = document.getElementById(focusTargetId);
      if (el) {
        el.focus({ preventScroll: true });
      }
    }
    onFinished?.();
  };

  if (typeof doc.startViewTransition !== 'function') {
    updateState();
    queueMicrotask(finish);
    return;
  }

  try {
    const transition = doc.startViewTransition(() => {
      flushSync(() => {
        updateState();
      });
    });
    transition.ready?.catch(() => {});
    void transition.finished.finally(finish);
  } catch {
    updateState();
    queueMicrotask(finish);
  }
}
