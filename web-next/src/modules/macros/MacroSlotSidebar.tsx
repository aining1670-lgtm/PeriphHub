import { type ChangeEvent, useRef } from 'react';
import { Download, Trash2, Upload } from 'lucide-react';
import {
  MAX_KBD_SLOTS,
  MOUSE_BUTTON_NAMES,
  type StudioMacroSlot,
} from './macro-codec';

export function MacroSlotSidebar({
  isMouse,
  isBusy,
  loading,
  slots,
  activeSlotIdx,
  current,
  dirtySlots,
  kbdSlotCount,
  onSelectSlot,
  onRequestDelete,
  onExpandKbdSlots,
  onExportJson,
  onImportFile,
}: {
  isMouse: boolean;
  isBusy: boolean;
  loading: boolean;
  slots: StudioMacroSlot[];
  activeSlotIdx: number;
  current: StudioMacroSlot;
  dirtySlots: Record<number, boolean>;
  kbdSlotCount: number;
  onSelectSlot: (idx: number) => void;
  onRequestDelete: () => void;
  onExpandKbdSlots: () => void;
  onExportJson: () => void;
  onImportFile: (e: ChangeEvent<HTMLInputElement>) => void;
}) {
  const fileInputRef = useRef<HTMLInputElement>(null);

  return (
    <aside className="md:col-span-4 lg:col-span-3 space-y-4">
      <div className="flex items-center justify-between">
        <span className="text-xs font-semibold" style={{ color: 'var(--c-ink-muted)' }}>
          {isMouse ? '本地宏库 (0–9)' : `板载槽位 (M01–M${kbdSlotCount})`}
        </span>
        {!current.empty && (
          <button
            type="button"
            disabled={isBusy}
            onClick={onRequestDelete}
            className="h-7 px-2 rounded-md text-xs font-medium inline-flex items-center gap-1 cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-45 transition-colors"
            style={{ color: 'var(--c-danger)' }}
          >
            <Trash2 className="w-3 h-3" />
            删除
          </button>
        )}
      </div>

      <div className="space-y-1 max-h-[calc(100vh-240px)] min-h-[200px] overflow-y-auto pr-1">
        {slots.map((slot, idx) => {
          const active = idx === activeSlotIdx;
          const dirty = Boolean(dirtySlots[slot.slot]);
          return (
            <button
              key={slot.slot}
              type="button"
              onClick={() => onSelectSlot(idx)}
              className={`w-full px-3 py-2 rounded-lg text-left flex items-center justify-between gap-2 cursor-pointer transition-colors relative overflow-hidden ${
                active ? 'bg-[var(--c-elev-2)]' : 'hover:bg-[var(--c-hover)]'
              }`}
            >
              {active && (
                <span
                  className="absolute left-0 top-2 bottom-2 w-[2.5px] rounded-full"
                  style={{ background: 'var(--c-hi)' }}
                />
              )}
              <div className="min-w-0">
                <div className="flex items-center gap-2">
                  <span className="c-mono text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
                    M{String(slot.slot + 1).padStart(2, '0')}
                  </span>
                  <span
                    className="text-[13px] truncate"
                    style={{
                      fontWeight: active ? 600 : 400,
                      color: slot.empty
                        ? 'var(--c-ink-faint)'
                        : active
                          ? 'var(--c-ink)'
                          : 'var(--c-ink-muted)',
                    }}
                  >
                    {slot.name}
                  </span>
                  {dirty && (
                    <span
                      className="w-1.5 h-1.5 rounded-full shrink-0"
                      style={{ background: 'var(--c-warn)' }}
                      title="有未写入的修改"
                    />
                  )}
                </div>
                {!slot.empty && isMouse && slot.boundButtons.length > 0 && (
                  <div className="text-[11px] mt-0.5 pl-7" style={{ color: 'var(--c-ok)' }}>
                    已绑：{slot.boundButtons.map((b) => MOUSE_BUTTON_NAMES[b] ?? `#${b}`).join(', ')}
                  </div>
                )}
              </div>
            </button>
          );
        })}
      </div>

      {!isMouse && kbdSlotCount < MAX_KBD_SLOTS && (
        <button
          type="button"
          disabled={isBusy || loading}
          onClick={onExpandKbdSlots}
          className="w-full h-7 rounded-md text-xs font-medium cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-45 transition-colors"
          style={{
            border: '1px dashed var(--c-border-strong)',
            color: 'var(--c-ink-muted)',
          }}
        >
          展开全部 50 个板载槽位
        </button>
      )}

      {isMouse && (
        <div
          className="pt-4 border-t flex items-center gap-2"
          style={{ borderColor: 'var(--c-border)' }}
        >
          <input
            ref={fileInputRef}
            type="file"
            accept=".json,application/json"
            onChange={onImportFile}
            className="hidden"
          />
          <button
            type="button"
            disabled={isBusy}
            onClick={onExportJson}
            className="flex-1 h-7 rounded-md text-xs inline-flex items-center justify-center gap-1 cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-45 transition-colors"
            style={{ color: 'var(--c-ink-muted)' }}
          >
            <Download className="w-3 h-3" />
            导出
          </button>
          <span className="w-px h-3.5" style={{ background: 'var(--c-border)' }} />
          <button
            type="button"
            disabled={isBusy}
            onClick={() => fileInputRef.current?.click()}
            className="flex-1 h-7 rounded-md text-xs inline-flex items-center justify-center gap-1 cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-45 transition-colors"
            style={{ color: 'var(--c-ink-muted)' }}
          >
            <Upload className="w-3 h-3" />
            导入
          </button>
        </div>
      )}
    </aside>
  );
}
