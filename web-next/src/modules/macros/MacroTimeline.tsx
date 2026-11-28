import {
  ArrowDown,
  ArrowUp,
  Circle,
  Clock,
  Keyboard,
  MousePointerClick,
  Square,
  Trash2,
} from 'lucide-react';
import type { StudioAction } from './macro-codec';

export function MacroTimeline({
  loading,
  isBusy,
  recording,
  recordDelay,
  actions,
  onToggleRecording,
  onToggleRecordDelay,
  onInsertMouseClick,
  onInsertDelay,
  onRequestClear,
  onUpdateDelay,
  onMoveAction,
  onDeleteAction,
}: {
  loading: boolean;
  isBusy: boolean;
  recording: boolean;
  recordDelay: boolean;
  actions: StudioAction[];
  onToggleRecording: () => void;
  onToggleRecordDelay: (checked: boolean) => void;
  onInsertMouseClick: (btnCode: 'LButton' | 'RButton' | 'MButton') => void;
  onInsertDelay: () => void;
  onRequestClear: () => void;
  onUpdateDelay: (actionId: number, ms: number) => void;
  onMoveAction: (index: number, dir: -1 | 1) => void;
  onDeleteAction: (id: number) => void;
}) {
  return (
    <div
      className="rounded-2xl border overflow-hidden transition-colors"
      style={{
        background: 'var(--c-elev)',
        borderColor: recording ? 'var(--c-danger)' : 'var(--c-border)',
      }}
    >
      <div
        className="px-4 py-3 border-b flex items-center justify-between gap-3 flex-wrap"
        style={{ borderColor: 'var(--c-border)' }}
      >
        <div className="flex items-center gap-3">
          <button
            type="button"
            disabled={loading || isBusy}
            onClick={onToggleRecording}
            className="h-8 px-4 rounded-lg text-xs font-semibold inline-flex items-center gap-2 cursor-pointer transition-colors disabled:opacity-45"
            style={
              recording
                ? {
                    background: 'var(--c-danger-soft)',
                    color: 'var(--c-danger)',
                    border: '1px solid var(--c-danger)',
                  }
                : {
                    background: '#e5484d',
                    color: '#ffffff',
                    boxShadow: '0 4px 14px -4px rgba(229, 72, 77, 0.55)',
                  }
            }
          >
            {recording ? (
              <>
                <Square className="w-3 h-3 fill-current" />
                停止录制
              </>
            ) : (
              <>
                <Circle className="w-2.5 h-2.5 fill-current" />
                开始录制
              </>
            )}
          </button>

          {recording ? (
            <span
              className="inline-flex items-center gap-1.5 text-xs font-medium"
              style={{ color: 'var(--c-danger)' }}
            >
              <span
                className="w-1.5 h-1.5 rounded-full motion-safe:animate-pulse"
                style={{ background: 'var(--c-danger)' }}
              />
              按下键盘按键开始录制（鼠标点击请通过右侧按钮插入）
            </span>
          ) : (
            <label
              className="inline-flex items-center gap-1.5 text-xs cursor-pointer"
              style={{ color: 'var(--c-ink-muted)' }}
            >
              <input
                type="checkbox"
                checked={recordDelay}
                onChange={(e) => onToggleRecordDelay(e.target.checked)}
                className="accent-[var(--c-hi)]"
              />
              记录真实延迟
            </label>
          )}
        </div>

        <div className="flex items-center gap-1.5 flex-wrap">
          <button
            type="button"
            onClick={() => onInsertMouseClick('LButton')}
            className="h-7 px-2 rounded-md text-xs font-medium inline-flex items-center gap-1 cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
            style={{ border: '1px solid var(--c-border)', color: 'var(--c-ink-muted)' }}
          >
            <MousePointerClick className="w-3 h-3" />
            左键点击
          </button>
          <button
            type="button"
            onClick={() => onInsertMouseClick('RButton')}
            className="h-7 px-2 rounded-md text-xs font-medium inline-flex items-center gap-1 cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
            style={{ border: '1px solid var(--c-border)', color: 'var(--c-ink-muted)' }}
          >
            右键点击
          </button>
          <button
            type="button"
            onClick={onInsertDelay}
            className="h-7 px-2.5 rounded-md text-xs font-medium inline-flex items-center gap-1 cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
            style={{ border: '1px solid var(--c-border)', color: 'var(--c-ink-muted)' }}
          >
            <Clock className="w-3 h-3" />
            插入延迟
          </button>

          {actions.length > 0 && (
            <button
              type="button"
              onClick={onRequestClear}
              className="h-7 px-2 rounded-md text-xs cursor-pointer hover:bg-[var(--c-hover)] transition-colors"
              style={{ color: 'var(--c-ink-faint)' }}
            >
              清空
            </button>
          )}
        </div>
      </div>

      {actions.length === 0 ? (
        <div
          className="py-14 text-center text-xs space-y-1.5"
          style={{ color: 'var(--c-ink-faint)' }}
        >
          <div className="font-medium" style={{ color: 'var(--c-ink-muted)' }}>
            {loading ? '正在加载槽位数据…' : '暂无动作'}
          </div>
          {!loading && <div>开始录制按键，或手动插入鼠标点击 / 延迟</div>}
        </div>
      ) : (
        <div
          className="max-h-[calc(100vh-380px)] min-h-[160px] overflow-y-auto divide-y px-3"
          style={{ borderColor: 'var(--c-border)' }}
        >
          {actions.map((act, idx) => (
            <div
              key={act.id}
              className="py-2 px-1 flex items-center justify-between gap-3 text-xs group"
              style={{ borderColor: 'var(--c-border)' }}
            >
              <div className="flex items-center gap-3 min-w-0">
                <span
                  className="c-mono text-[11px] w-6 text-right shrink-0"
                  style={{ color: 'var(--c-ink-faint)' }}
                >
                  {String(idx + 1).padStart(2, '0')}
                </span>

                {act.type === 'delay' ? (
                  <div className="inline-flex items-center gap-2">
                    <Clock className="w-3.5 h-3.5" style={{ color: 'var(--c-ink-faint)' }} />
                    <span style={{ color: 'var(--c-ink-muted)' }}>等待</span>
                    <input
                      type="number"
                      min={0}
                      max={65535}
                      value={act.ms ?? 0}
                      onChange={(e) => {
                        const ms = Number(e.target.value);
                        if (!Number.isFinite(ms)) return;
                        onUpdateDelay(act.id, Math.trunc(ms));
                      }}
                      className="w-16 h-6 px-2 rounded text-xs c-num text-right"
                      style={{
                        background: 'var(--c-panel)',
                        border: '1px solid var(--c-border)',
                        color: 'var(--c-ink)',
                      }}
                    />
                    <span className="c-num" style={{ color: 'var(--c-ink-faint)' }}>
                      ms
                    </span>
                  </div>
                ) : (
                  <div className="inline-flex items-center gap-2">
                    {act.type === 'mouse' ? (
                      <MousePointerClick
                        className="w-3.5 h-3.5"
                        style={{ color: 'var(--c-ink-muted)' }}
                      />
                    ) : (
                      <Keyboard className="w-3.5 h-3.5" style={{ color: 'var(--c-ink-muted)' }} />
                    )}
                    <span
                      className="px-2 py-0.5 rounded font-medium c-mono text-[11px]"
                      style={{
                        background: 'var(--c-panel)',
                        border: '1px solid var(--c-border)',
                        color: 'var(--c-ink)',
                      }}
                    >
                      {act.code}
                    </span>
                    <span
                      className="text-[11px]"
                      style={{
                        color: act.action === 'down' ? 'var(--c-ok)' : 'var(--c-ink-faint)',
                      }}
                    >
                      {act.action === 'down' ? '按下 ↓' : '抬起 ↑'}
                    </span>
                  </div>
                )}
              </div>

              <div className="flex items-center gap-1 opacity-0 group-hover:opacity-100 group-focus-within:opacity-100 transition-opacity">
                <button
                  type="button"
                  disabled={idx === 0}
                  onClick={() => onMoveAction(idx, -1)}
                  className="w-6 h-6 rounded flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-30"
                  style={{ color: 'var(--c-ink-muted)' }}
                  aria-label="上移"
                >
                  <ArrowUp className="w-3 h-3" />
                </button>
                <button
                  type="button"
                  disabled={idx === actions.length - 1}
                  onClick={() => onMoveAction(idx, 1)}
                  className="w-6 h-6 rounded flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)] disabled:opacity-30"
                  style={{ color: 'var(--c-ink-muted)' }}
                  aria-label="下移"
                >
                  <ArrowDown className="w-3 h-3" />
                </button>
                <button
                  type="button"
                  onClick={() => onDeleteAction(act.id)}
                  className="w-6 h-6 rounded flex items-center justify-center cursor-pointer hover:bg-[var(--c-hover)]"
                  style={{ color: 'var(--c-danger)' }}
                  aria-label="删除步骤"
                >
                  <Trash2 className="w-3 h-3" />
                </button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
