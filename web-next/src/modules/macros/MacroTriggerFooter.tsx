import { SegmentedTrack } from '../../ui/PrecisionControls';
import {
  MOUSE_BIND_OPTIONS,
  MOUSE_BUTTON_NAMES,
  type StudioMacroSlot,
} from './macro-codec';

export function MacroTriggerFooter({
  isMouse,
  isBusy,
  loading,
  overLimit,
  current,
  onChangeLoopType,
  onChangeCycles,
  onChangeTargetButton,
  onCommit,
}: {
  isMouse: boolean;
  isBusy: boolean;
  loading: boolean;
  overLimit: boolean;
  current: StudioMacroSlot;
  onChangeLoopType: (loopType: 0 | 1 | 2) => void;
  onChangeCycles: (cycles: number) => void;
  onChangeTargetButton: (targetButton: number | null) => void;
  onCommit: () => void;
}) {
  return (
    <>
      {/* STAGE 2: Trigger & Binding */}
      <div className="space-y-4">
        <div className="text-xs font-semibold" style={{ color: 'var(--c-ink-muted)' }}>
          触发方式与按键绑定
        </div>

        {isMouse ? (
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-6 items-start">
            <div className="space-y-2">
              <div className="flex items-center justify-between text-xs">
                <span style={{ color: 'var(--c-ink-faint)' }}>循环模式</span>
                {current.loopType === 0 && (
                  <label className="inline-flex items-center gap-1.5">
                    <span style={{ color: 'var(--c-ink-faint)' }}>循环次数</span>
                    <input
                      type="number"
                      min={1}
                      max={65535}
                      value={current.cycles}
                      onChange={(e) => {
                        const v = Number(e.target.value);
                        if (Number.isFinite(v)) {
                          onChangeCycles(Math.trunc(v));
                        }
                      }}
                      className="w-16 h-6 px-2 rounded text-xs c-num text-right"
                      style={{
                        background: 'var(--c-elev)',
                        border: '1px solid var(--c-border-strong)',
                        color: 'var(--c-ink)',
                      }}
                    />
                  </label>
                )}
              </div>
              <SegmentedTrack
                labels={['按次数', '按住循环', '再次按停止']}
                activeIndex={current.loopType}
                itemPaddingClass="py-1"
                onCommit={(idx) => onChangeLoopType(idx as 0 | 1 | 2)}
              />
            </div>

            <div className="space-y-2">
              <div className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
                保存后绑定到鼠标按键
              </div>
              <div className="flex flex-wrap gap-1.5">
                {MOUSE_BIND_OPTIONS.map((opt) => {
                  const act = current.targetButton === opt.index;
                  return (
                    <button
                      key={String(opt.index)}
                      type="button"
                      onClick={() => onChangeTargetButton(opt.index)}
                      className="px-2.5 py-1.5 rounded-lg text-xs font-medium cursor-pointer transition-colors"
                      style={{
                        background: act ? 'var(--c-hi)' : 'var(--c-elev)',
                        color: act ? 'var(--c-hi-inv)' : 'var(--c-ink-muted)',
                        border: `1px solid ${act ? 'var(--c-hi)' : 'var(--c-border)'}`,
                      }}
                    >
                      {opt.label}
                    </button>
                  );
                })}
              </div>
            </div>
          </div>
        ) : (
          <div className="flex items-center justify-between gap-4 flex-wrap">
            <div className="flex items-center gap-3">
              <span className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
                触发重复次数 (1–65535)
              </span>
              <input
                type="number"
                min={1}
                max={65535}
                value={current.cycles}
                onChange={(e) => {
                  const v = Number(e.target.value);
                  if (Number.isFinite(v)) {
                    onChangeCycles(Math.trunc(v));
                  }
                }}
                className="w-20 h-7 px-2.5 rounded-lg text-xs c-num text-right"
                style={{
                  background: 'var(--c-elev)',
                  border: '1px solid var(--c-border-strong)',
                  color: 'var(--c-ink)',
                }}
              />
            </div>
            <span className="text-xs" style={{ color: 'var(--c-ink-faint)' }}>
              写入槽位后，可在「按键映射」中绑定至任意按键
            </span>
          </div>
        )}
      </div>

      {/* STAGE 3: Commit */}
      <div
        className="pt-5 border-t flex items-center justify-between gap-4 flex-wrap"
        style={{ borderColor: 'var(--c-border)' }}
      >
        <span className="text-[11px]" style={{ color: 'var(--c-ink-faint)' }}>
          {isMouse
            ? '末项若为「按下」，本地保存时会自动补全松开动作'
            : '单条板载宏上限 250 字节'}
        </span>

        <button
          type="button"
          onClick={onCommit}
          disabled={overLimit || isBusy || loading}
          className="h-9 px-5 rounded-lg text-xs font-semibold cursor-pointer disabled:opacity-40 transition-colors"
          style={{ background: 'var(--c-hi)', color: 'var(--c-hi-inv)' }}
        >
          {isMouse && current.targetButton !== null
            ? `保存并上传绑定至${MOUSE_BUTTON_NAMES[current.targetButton] ?? '按键'}`
            : '保存并写入硬件'}
        </button>
      </div>
    </>
  );
}
