import { type CSSProperties, useEffect, useMemo, useRef, useState } from 'react';
import { MODIFIER_KEY_CODES } from '../../bridge/ui-helpers';
import type { KbdKeyEntry } from '../../bridge/types';
import { useHubStore } from '../../state/hub-store';
import {
  BC98_CODE_TO_USAGE,
  KB_PICKER_CATEGORIES,
  type KbCatId,
} from './bc98-layout';

export function KeyboardKeymapPanel({
  deviceKey,
  loading,
  resettingLayer,
  selSlot,
  layer,
  onWriteKeyEntry,
  onResetSelectedKey,
  onRequestLayerReset,
}: {
  deviceKey: string;
  loading: boolean;
  resettingLayer: boolean;
  selSlot: number;
  layer: 'base' | 'fn';
  onWriteKeyEntry: (slot: number, entry: KbdKeyEntry, fnLayer: boolean, label?: string) => Promise<void>;
  onResetSelectedKey: () => void;
  onRequestLayerReset: () => void;
}) {
  const pushToast = useHubStore((s) => s.pushToast);

  const [cat, setCat] = useState<KbCatId | 'macro'>('basic');
  const [macroBindMode, setMacroBindMode] = useState<0 | 1 | 2>(0); // 0=cycles, 1=toggle, 2=hold
  const [keySearch, setKeySearch] = useState('');
  const [capturingKey, setCapturingKey] = useState(false);

  const selSlotRef = useRef(selSlot);
  selSlotRef.current = selSlot;
  const layerRef = useRef(layer);
  layerRef.current = layer;
  const writeRef = useRef(onWriteKeyEntry);
  writeRef.current = onWriteKeyEntry;

  useEffect(() => {
    setCapturingKey(false);
  }, [deviceKey]);

  useEffect(() => {
    if (!capturingKey) return;
    const onKeyDown = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();
      if (e.key === 'Escape' && !e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey) {
        setCapturingKey(false);
        return;
      }
      if (MODIFIER_KEY_CODES.has(e.code) || !e.code) return;
      const usage = BC98_CODE_TO_USAGE[e.code];
      if (!usage) {
        pushToast('warn', `暂不支持捕获键码：${e.code}`);
        return;
      }
      const mods: number[] = [];
      if (e.ctrlKey) mods.push(224);
      if (e.shiftKey) mods.push(225);
      if (e.altKey) mods.push(226);
      if (e.metaKey) mods.push(227);
      const entry: KbdKeyEntry = [0, mods[0] ?? 0, usage, mods[1] ?? 0];
      setCapturingKey(false);
      void writeRef.current(selSlotRef.current, entry, layerRef.current === 'fn');
    };
    window.addEventListener('keydown', onKeyDown, true);
    return () => window.removeEventListener('keydown', onKeyDown, true);
  }, [capturingKey, pushToast]);

  const trimmedSearch = keySearch.trim().toLowerCase();
  const filteredItems = useMemo(
    () =>
      trimmedSearch
        ? Object.values(KB_PICKER_CATEGORIES)
            .flatMap((c) => c.items)
            .filter((item) => item.label.toLowerCase().includes(trimmedSearch))
        : [],
    [trimmedSearch],
  );

  const isFn = layer === 'fn';

  return (
    <div
      key="keymap"
      className="relative z-10 border-t pt-5 space-y-4 tab-enter"
      style={{ borderColor: 'var(--c-border)' }}
    >
      <div className="flex flex-wrap items-center justify-between gap-2">
        <div
          role="tablist"
          aria-label="键码分类"
          className="sliding-pill-tabs flex flex-wrap p-1 rounded-xl gap-0.5"
          style={
            {
              background: 'var(--c-elev)',
              border: '1px solid var(--c-border)',
              '--pill-radius': '0.5rem',
            } as CSSProperties
          }
        >
          {(Object.entries(KB_PICKER_CATEGORIES) as Array<[KbCatId, { name: string }]>).map(
            ([k, v]) => {
              const active = cat === k;
              return (
                <button
                  key={k}
                  type="button"
                  role="tab"
                  aria-selected={active}
                  data-active={active ? 'true' : undefined}
                  onClick={() => setCat(k)}
                  className="px-3 py-1.5 rounded-lg text-xs font-medium cursor-pointer transition-colors"
                  style={{
                    color: active ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                  }}
                >
                  {v.name}
                </button>
              );
            },
          )}
          <button
            type="button"
            role="tab"
            aria-selected={cat === 'macro'}
            data-active={cat === 'macro' ? 'true' : undefined}
            onClick={() => setCat('macro')}
            className="px-3 py-1.5 rounded-lg text-xs font-medium cursor-pointer transition-colors"
            style={{
              color: cat === 'macro' ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
            }}
          >
            绑定宏
          </button>
        </div>
        <div className="flex flex-wrap items-center gap-3">
          <input
            type="search"
            value={keySearch}
            onChange={(e) => setKeySearch(e.target.value)}
            placeholder="搜索键码…"
            className="h-7 w-32 px-2.5 rounded-md text-xs"
            style={{
              background: 'var(--c-elev)',
              border: '1px solid var(--c-border)',
              color: 'var(--c-ink)',
            }}
            aria-label="搜索键码"
          />
          <button
            type="button"
            disabled={loading}
            onClick={() => setCapturingKey((v) => !v)}
            className="h-7 px-2.5 rounded-md text-xs font-medium cursor-pointer disabled:opacity-45"
            style={{
              background: capturingKey ? 'var(--c-danger-soft)' : 'var(--c-elev)',
              border: `1px solid ${capturingKey ? 'var(--c-danger)' : 'var(--c-border-strong)'}`,
              color: capturingKey ? 'var(--c-danger)' : 'var(--c-ink)',
            }}
          >
            {capturingKey ? '按下键位… (Esc取消)' : '按键捕获'}
          </button>
          <button
            type="button"
            disabled={loading}
            onClick={onResetSelectedKey}
            className="text-xs cursor-pointer hover:underline disabled:opacity-45"
            style={{ color: 'var(--c-ink-muted)' }}
          >
            恢复单键默认
          </button>
          <button
            type="button"
            disabled={loading || resettingLayer}
            onClick={onRequestLayerReset}
            className="text-xs cursor-pointer hover:underline disabled:opacity-45"
            style={{ color: 'var(--c-ink-muted)' }}
          >
            恢复当前层默认
          </button>
        </div>
      </div>

      {keySearch.trim() ? (
        filteredItems.length === 0 ? (
          <div
            className="flex items-center gap-3 py-2 text-xs"
            style={{ color: 'var(--c-ink-faint)' }}
          >
            <span>未找到匹配键码</span>
            <button
              type="button"
              onClick={() => setKeySearch('')}
              className="px-2.5 py-1 rounded-md font-medium cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
              style={{
                background: 'var(--c-elev)',
                border: '1px solid var(--c-border)',
                color: 'var(--c-ink)',
              }}
            >
              清空搜索
            </button>
          </div>
        ) : (
          <div className="flex flex-wrap gap-2 max-h-[160px] overflow-y-auto pr-1">
            {filteredItems.map((item) => (
              <button
                key={`${item.label}-${item.entry.join(',')}`}
                type="button"
                disabled={loading}
                onClick={() => void onWriteKeyEntry(selSlot, item.entry, isFn, item.label)}
                className="h-9 px-3.5 rounded-lg text-xs font-medium cursor-pointer transition-colors hover:border-[var(--c-hi)] disabled:opacity-45"
                style={{
                  background: 'var(--c-elev)',
                  border: '1px solid var(--c-border)',
                  color: 'var(--c-ink)',
                }}
              >
                {item.label}
              </button>
            ))}
          </div>
        )
      ) : cat === 'macro' ? (
        <div className="space-y-3">
          <div className="flex items-center gap-2 text-xs">
            <span style={{ color: 'var(--c-ink-faint)' }}>触发方式：</span>
            <div
              className="sliding-pill-tabs flex p-0.5 rounded-lg"
              style={
                {
                  background: 'var(--c-elev)',
                  border: '1px solid var(--c-border)',
                  '--pill-radius': '0.375rem',
                } as CSSProperties
              }
            >
              {(
                [
                  [0, '按次数'],
                  [2, '按住循环'],
                  [1, '再次按停止'],
                ] as Array<[0 | 1 | 2, string]>
              ).map(([m, label]) => {
                const active = macroBindMode === m;
                return (
                  <button
                    key={m}
                    type="button"
                    data-active={active ? 'true' : undefined}
                    aria-pressed={active}
                    onClick={() => setMacroBindMode(m)}
                    className="px-2.5 py-1 rounded-md text-xs font-medium cursor-pointer transition-colors"
                    style={{
                      color: active ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                    }}
                  >
                    {label}
                  </button>
                );
              })}
            </div>
          </div>
          <div className="flex flex-wrap gap-2">
            {Array.from({ length: 16 }, (_, slot) => (
              <button
                key={slot}
                type="button"
                disabled={loading}
                onClick={() =>
                  void onWriteKeyEntry(
                    selSlot,
                    [9, macroBindMode, slot, 0],
                    isFn,
                    `M${slot + 1}`,
                  )
                }
                className="h-9 px-3.5 rounded-lg text-xs font-medium c-mono cursor-pointer transition-colors hover:border-[var(--c-hi)] disabled:opacity-45"
                style={{
                  background: 'var(--c-elev)',
                  border: '1px solid var(--c-border)',
                  color: 'var(--c-ink)',
                }}
              >
                M{String(slot + 1).padStart(2, '0')}
              </button>
            ))}
          </div>
        </div>
      ) : (
        <div className="flex flex-wrap gap-2">
          {KB_PICKER_CATEGORIES[cat].items.map((item) => (
            <button
              key={item.label}
              type="button"
              disabled={loading}
              onClick={() => void onWriteKeyEntry(selSlot, item.entry, isFn, item.label)}
              className="h-9 px-3.5 rounded-lg text-xs font-medium cursor-pointer transition-colors hover:border-[var(--c-hi)] disabled:opacity-45"
              style={{
                background: 'var(--c-elev)',
                border: '1px solid var(--c-border)',
                color: 'var(--c-ink)',
              }}
            >
              {item.label}
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
