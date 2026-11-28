import { useState } from 'react';
import { ConfirmDialog } from '../../ui/ConfirmDialog';
import {
  type MacroStudioMode,
  MAX_KBD_BYTES,
  MAX_MOUSE_ACTIONS,
  MAX_NAME_LEN,
} from './macro-codec';
import { MacroSlotSidebar } from './MacroSlotSidebar';
import { MacroTimeline } from './MacroTimeline';
import { MacroTriggerFooter } from './MacroTriggerFooter';
import { useMacroStudio } from './useMacroStudio';

export type { MacroStudioMode };

export function MacroStudio({
  deviceKey,
  mode,
}: {
  deviceKey: string;
  mode: MacroStudioMode;
}) {
  const studio = useMacroStudio(deviceKey, mode);
  const [confirmAction, setConfirmAction] = useState<'delete' | 'clear' | null>(null);

  const { current, isMouse } = studio;

  return (
    <div className="relative z-10 w-full max-w-[1220px] mx-auto flex-1 grid grid-cols-1 md:grid-cols-12 items-start py-6 gap-8 lg:gap-12">
      {/* Left Column: Slot List */}
      <MacroSlotSidebar
        isMouse={isMouse}
        isBusy={studio.isBusy}
        loading={studio.loading}
        slots={studio.slots}
        activeSlotIdx={studio.activeSlotIdx}
        current={current}
        dirtySlots={studio.dirtySlots}
        kbdSlotCount={studio.kbdSlotCount}
        onSelectSlot={(idx) => void studio.handleSelectSlot(idx)}
        onRequestDelete={() => setConfirmAction('delete')}
        onExpandKbdSlots={studio.handleExpandKbdSlots}
        onExportJson={() => void studio.handleExportJson()}
        onImportFile={(e) => void studio.handleImportFile(e)}
      />

      {/* Right Column: Clear 3-Stage Workflow */}
      <section className="md:col-span-8 lg:col-span-9 space-y-6">
        <div className="flex items-center justify-between gap-4">
          <div className="flex items-center gap-2.5 min-w-0 flex-1">
            <span
              className="c-mono text-xs px-2 py-0.5 rounded shrink-0"
              style={{ background: 'var(--c-elev)', color: 'var(--c-ink-faint)' }}
            >
              M{String(current.slot + 1).padStart(2, '0')}
            </span>
            <input
              type="text"
              value={current.name}
              maxLength={MAX_NAME_LEN}
              onChange={(e) =>
                studio.updateCurrent((s) => ({ ...s, empty: false, name: e.target.value }))
              }
              className="bg-transparent text-xl font-semibold tracking-tight border-b border-transparent focus:border-[var(--c-hi)] min-w-0 flex-1 pb-0.5"
              style={{ color: 'var(--c-ink)' }}
              aria-label="宏名称"
            />
          </div>

          <span
            className="c-num text-xs font-medium shrink-0"
            style={{ color: studio.overLimit ? 'var(--c-danger)' : 'var(--c-ink-faint)' }}
          >
            容量{' '}
            {isMouse
              ? `${studio.mouseItems.length} / ${MAX_MOUSE_ACTIONS} 步`
              : `${studio.usedBytes} / ${MAX_KBD_BYTES} B`}
          </span>
        </div>

        <MacroTimeline
          loading={studio.loading}
          isBusy={studio.isBusy}
          recording={studio.recording}
          recordDelay={studio.recordDelay}
          actions={current.actions}
          onToggleRecording={() => studio.setRecording((r) => !r)}
          onToggleRecordDelay={studio.setRecordDelay}
          onInsertMouseClick={studio.handleInsertMouseClick}
          onInsertDelay={studio.handleInsertDelay}
          onRequestClear={() => setConfirmAction('clear')}
          onUpdateDelay={(actionId, ms) =>
            studio.updateCurrent((s) => ({
              ...s,
              actions: s.actions.map((item) =>
                item.id === actionId ? { ...item, ms } : item,
              ),
            }))
          }
          onMoveAction={studio.handleMoveAction}
          onDeleteAction={studio.handleDeleteAction}
        />

        <MacroTriggerFooter
          isMouse={isMouse}
          isBusy={studio.isBusy}
          loading={studio.loading}
          overLimit={studio.overLimit}
          current={current}
          onChangeLoopType={(loopType) => studio.updateCurrent((s) => ({ ...s, loopType }))}
          onChangeCycles={(cycles) => studio.updateCurrent((s) => ({ ...s, cycles }))}
          onChangeTargetButton={(targetButton) =>
            studio.updateCurrent((s) => ({ ...s, targetButton }))
          }
          onCommit={() => void studio.handleCommitHardware()}
        />
      </section>

      <ConfirmDialog
        open={confirmAction !== null}
        title={
          confirmAction === 'clear'
            ? '清空当前宏的所有动作？'
            : `清空宏槽位 M${String(current.slot + 1).padStart(2, '0')}？`
        }
        description={
          confirmAction === 'clear'
            ? '将移除当前编辑中的全部按键、鼠标点击和延迟步骤，未写入硬件前不会影响已保存的宏。'
            : isMouse
              ? `将从本地宏库删除「${current.name}」并解除关联记录（已写入鼠标固件的缓冲需通过重新绑键覆盖）。`
              : `将清空键盘板载宏槽位 M${String(current.slot + 1).padStart(2, '0')} 中的所有步骤。`
        }
        confirmLabel={confirmAction === 'clear' ? '确认清空' : '确认删除'}
        danger
        onCancel={() => setConfirmAction(null)}
        onConfirm={() => {
          const action = confirmAction;
          setConfirmAction(null);
          if (action === 'clear') {
            studio.updateCurrent((s) => ({ ...s, actions: [] }));
          } else if (action === 'delete') {
            void studio.handleDeleteSlot();
          }
        }}
      />
    </div>
  );
}
