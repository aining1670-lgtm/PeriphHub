import { useEffect, useRef, useState } from 'react';
import { MODIFIER_KEY_CODES } from '../../bridge/ui-helpers';
import type { KeyBindingItem } from '../../bridge/types';
import {
  BUTTON_ID_BY_INDEX,
  decodeHardwareBinding,
  formatKbDisplay,
  MOUSE_BINDING_CHOICES,
  MOUSE_BUTTONS,
  type MouseButtonId,
  type MouseCatId,
} from './mouse-codec';

export function MouseKeymapTab({
  deviceKey,
  loading,
  resetting,
  selBtnIdx,
  bindings,
  buttonDecodedByIndex,
  supportsFire,
  supportsMacros,
  onApplyBinding,
  onOpenMacros,
  onRequestResetKeys,
}: {
  deviceKey: string;
  loading: boolean;
  resetting: boolean;
  selBtnIdx: number;
  bindings: KeyBindingItem[];
  buttonDecodedByIndex: Record<number, ReturnType<typeof decodeHardwareBinding>>;
  supportsFire: boolean;
  supportsMacros: boolean;
  onApplyBinding: (targetIdx: number, hwBinding: string, label: string) => Promise<void>;
  onOpenMacros: () => void;
  onRequestResetKeys: () => void;
}) {
  const [btnCat, setBtnCat] = useState<MouseCatId>('mouse');
  const [capturingKb, setCapturingKb] = useState(false);

  const applyRef = useRef(onApplyBinding);
  applyRef.current = onApplyBinding;
  const selBtnIdxRef = useRef(selBtnIdx);
  selBtnIdxRef.current = selBtnIdx;

  useEffect(() => {
    setBtnCat('mouse');
    setCapturingKb(false);
  }, [deviceKey]);

  useEffect(() => {
    if (!capturingKb) return;
    const onKeyDown = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();
      if (e.key === 'Escape' && !e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey) {
        setCapturingKb(false);
        return;
      }
      if (MODIFIER_KEY_CODES.has(e.code) || !e.code) return;
      const parts: string[] = [];
      if (e.ctrlKey) parts.push('Ctrl');
      if (e.shiftKey) parts.push('Shift');
      if (e.altKey) parts.push('Alt');
      if (e.metaKey) parts.push('Meta');
      parts.push(e.code);
      const combo = parts.join('+');
      setCapturingKb(false);
      void applyRef.current(selBtnIdxRef.current, combo, formatKbDisplay(combo));
    };
    window.addEventListener('keydown', onKeyDown, true);
    return () => window.removeEventListener('keydown', onKeyDown, true);
  }, [capturingKb]);

  const selRow = bindings.find((b) => b.index === selBtnIdx);
  const selBtnId = BUTTON_ID_BY_INDEX[selBtnIdx] as MouseButtonId | undefined;
  const selButtonName =
    (selBtnId ? MOUSE_BUTTONS.find((b) => b.id === selBtnId)?.name : undefined) ??
    selRow?.slot ??
    `按键 ${selBtnIdx + 1}`;
  const selDecoded = buttonDecodedByIndex[selBtnIdx];

  return (
    <div className="px-6 sm:px-8 lg:px-14 py-9 flex flex-col justify-between h-full">
      <div className="space-y-7">
        <div
          className="border-b pb-5 flex items-end justify-between"
          style={{ borderColor: 'var(--c-border)' }}
        >
          <div>
            <div className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
              正在编辑按键
            </div>
            <div className="text-2xl font-semibold mt-1">{selButtonName}</div>
          </div>
          <div className="text-right">
            <div className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
              当前硬件映射
            </div>
            <div className="text-sm font-semibold mt-1">{selDecoded?.label ?? '—'}</div>
          </div>
        </div>

        <div className="flex flex-wrap gap-2">
          {(
            [
              ['mouse', '鼠标'],
              ['dpi', 'DPI'],
              ['kb', '键盘组合'],
              ['media', '媒体'],
              ...(supportsFire ? ([['rapid', '火力连发']] as Array<[MouseCatId, string]>) : []),
              ['disable', '禁用'],
            ] as Array<[MouseCatId, string]>
          ).map(([k, l]) => (
            <button
              key={k}
              type="button"
              onClick={() => setBtnCat(k)}
              className="px-3.5 py-1.5 rounded-full text-xs font-medium cursor-pointer transition-colors"
              style={{
                background: btnCat === k ? 'var(--c-hi)' : 'var(--c-elev)',
                color: btnCat === k ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
              }}
            >
              {l}
            </button>
          ))}
          {supportsMacros && (
            <button
              type="button"
              onClick={onOpenMacros}
              className="px-3.5 py-1.5 rounded-full text-xs font-medium cursor-pointer transition-colors"
              style={{
                background: 'var(--c-elev)',
                color: 'var(--c-ink-muted)',
                border: '1px dashed var(--c-border-strong)',
              }}
            >
              绑定宏 →
            </button>
          )}
        </div>

        <div className="grid grid-cols-2 sm:grid-cols-3 gap-2.5">
          {MOUSE_BINDING_CHOICES[btnCat].map((item) => {
            const cur = selDecoded?.raw === item.hwBinding;
            return (
              <button
                key={item.hwBinding}
                type="button"
                disabled={loading}
                onClick={() => void onApplyBinding(selBtnIdx, item.hwBinding, item.label)}
                className="px-4 py-3 rounded-lg text-left text-xs font-medium cursor-pointer transition-[border-color,background-color,box-shadow] disabled:opacity-45"
                style={{
                  background: cur ? 'var(--c-elev-2)' : 'var(--c-elev)',
                  border: `1px solid ${cur ? 'var(--c-hi)' : 'var(--c-border)'}`,
                  color: cur ? 'var(--c-ink)' : 'var(--c-ink-muted)',
                }}
              >
                {item.label}
              </button>
            );
          })}
          {btnCat === 'kb' && (
            <button
              type="button"
              disabled={loading}
              onClick={() => setCapturingKb((c) => !c)}
              className="px-4 py-3 rounded-lg text-left text-xs font-medium cursor-pointer transition-[border-color,background-color,box-shadow] disabled:opacity-45"
              style={{
                background: capturingKb ? 'var(--c-danger-soft)' : 'var(--c-elev)',
                border: `1px dashed ${capturingKb ? 'var(--c-danger)' : 'var(--c-border-strong)'}`,
                color: capturingKb ? 'var(--c-danger)' : 'var(--c-ink)',
              }}
            >
              {capturingKb ? '按下组合键… (Esc 取消)' : '+ 录制自定义快捷键'}
            </button>
          )}
        </div>
      </div>

      <div
        className="pt-6 border-t mt-8 flex items-center justify-between text-xs"
        style={{ borderColor: 'var(--c-border)', color: 'var(--c-ink-faint)' }}
      >
        <span>至少需保留一个鼠标左键 · 宏请前往「宏」页面绑定</span>
        <button
          type="button"
          disabled={loading || resetting}
          onClick={onRequestResetKeys}
          className="px-3 py-1.5 rounded-lg text-xs font-medium cursor-pointer transition-colors bg-[var(--c-elev)] hover:bg-[var(--c-hover)] border border-[var(--c-border-strong)] text-[var(--c-ink)] disabled:opacity-45"
        >
          恢复默认键位
        </button>
      </div>
    </div>
  );
}
