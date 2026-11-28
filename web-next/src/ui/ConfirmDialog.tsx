import { useEffect, useId, useRef } from 'react';
import { AlertTriangle, X } from 'lucide-react';

export function ConfirmDialog({
  open,
  title,
  description,
  confirmLabel = '确认',
  cancelLabel = '取消',
  danger = false,
  onConfirm,
  onCancel,
}: {
  open: boolean;
  title: string;
  description: string;
  confirmLabel?: string;
  cancelLabel?: string;
  danger?: boolean;
  onConfirm: () => void;
  onCancel: () => void;
}) {
  const dialogRef = useRef<HTMLDialogElement>(null);
  const cancelBtnRef = useRef<HTMLButtonElement>(null);
  const titleId = useId();
  const descId = useId();

  useEffect(() => {
    const el = dialogRef.current;
    if (!el) return;
    if (open && !el.open) {
      el.showModal();
      cancelBtnRef.current?.focus();
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

  return (
    <dialog
      ref={dialogRef}
      aria-labelledby={titleId}
      aria-describedby={descId}
      onClose={() => {
        if (open) onCancel();
      }}
      className="c-confirm-dialog m-auto w-[calc(100%-2rem)] max-w-sm rounded-2xl border p-5 shadow-2xl"
      style={{
        backgroundColor: 'var(--c-panel)',
        borderColor: 'var(--c-border-strong)',
        color: 'var(--c-ink)',
      }}
      {...({ closedby: 'any' } as Record<string, unknown>)}
    >
      {open && (
        <div className="space-y-4">
          <div className="flex items-start justify-between gap-3">
            <div className="flex items-center gap-2.5">
              <span
                className="w-8 h-8 rounded-lg flex items-center justify-center shrink-0"
                style={{
                  backgroundColor: danger ? 'var(--c-danger-soft)' : 'var(--c-warn-soft)',
                  color: danger ? 'var(--c-danger)' : 'var(--c-warn)',
                }}
              >
                <AlertTriangle className="w-4 h-4" />
              </span>
              <h2 id={titleId} className="text-sm font-semibold">
                {title}
              </h2>
            </div>
            <button
              type="button"
              onClick={onCancel}
              className="w-7 h-7 rounded-md flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)]"
              style={{ color: 'var(--c-ink-muted)' }}
              aria-label="关闭"
            >
              <X className="w-3.5 h-3.5" />
            </button>
          </div>

          <p
            id={descId}
            className="text-xs leading-relaxed"
            style={{ color: 'var(--c-ink-muted)' }}
          >
            {description}
          </p>

          <div className="flex items-center justify-end gap-2 pt-1">
            <button
              ref={cancelBtnRef}
              type="button"
              onClick={onCancel}
              className="h-8 px-3.5 rounded-lg text-xs font-medium cursor-pointer bg-[var(--c-elev)] hover:bg-[var(--c-hover)] border border-[var(--c-border)] text-[var(--c-ink-muted)] transition-colors"
            >
              {cancelLabel}
            </button>
            <button
              type="button"
              onClick={onConfirm}
              className="h-8 px-4 rounded-lg text-xs font-semibold cursor-pointer transition-opacity hover:opacity-90"
              style={{
                backgroundColor: danger ? 'var(--c-danger)' : 'var(--c-hi)',
                color: danger ? '#ffffff' : 'var(--c-hi-inv)',
              }}
            >
              {confirmLabel}
            </button>
          </div>
        </div>
      )}
    </dialog>
  );
}
