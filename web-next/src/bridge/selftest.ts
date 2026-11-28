import { bridgeCommands } from './commands';
import { invoke } from './transport';
import type { KeysState, MacroListItem, MiscState, SensorState } from './types';

export type SelftestMode = 'none' | 'render' | 'write' | 'f26';

export function detectSelftestMode(): SelftestMode {
  if (typeof window === 'undefined') return 'none';
  const hash = window.location.hash.toLowerCase();
  const param = new URLSearchParams(window.location.search).get('selftest')?.toLowerCase();
  if (hash === '#selftest-f26' || param === 'f26') return 'f26';
  if (hash === '#selftest-write' || param === 'write') return 'write';
  if (hash === '#selftest' || param === '1') return 'render';
  return 'none';
}

async function safeInvoke<T>(
  cmd: string,
  args?: Record<string, unknown>,
  serialized = false,
): Promise<{ ok: true; data: T } | { ok: false; error: string }> {
  try {
    const data = await invoke<T>(cmd, args, { serialized });
    return { ok: true, data };
  } catch (err) {
    return { ok: false, error: err instanceof Error ? err.message : String(err) };
  }
}

async function runSelftestWrite(): Promise<void> {
  const miscGet = await safeInvoke<MiscState>('misc.get');
  const sensorGet = await safeInvoke<SensorState>('sensor.get');

  const miscArgs: Record<string, unknown> = {};
  if (miscGet.ok) {
    const d = miscGet.data;
    if (typeof d.debounce === 'number' && Number.isInteger(d.debounce) && d.debounce >= 0 && d.debounce <= 30) {
      miscArgs.debounce = d.debounce;
    } else if (typeof d.sleep === 'number' && Number.isInteger(d.sleep) && d.sleep >= 10 && d.sleep <= 900) {
      miscArgs.sleep = d.sleep;
    }
  }

  const sensorArgs: Record<string, unknown> = {};
  if (sensorGet.ok) {
    const s = sensorGet.data;
    if (typeof s.lod === 'number' && Number.isInteger(s.lod) && s.lod >= 0 && s.lod <= 2) {
      sensorArgs.lod = s.lod;
    } else if (typeof s.mode === 'number' && Number.isInteger(s.mode) && s.mode >= 0 && s.mode <= 2) {
      sensorArgs.mode = s.mode;
    }
  }

  const miscSet =
    Object.keys(miscArgs).length > 0
      ? await safeInvoke<Partial<MiscState>>('misc.set', miscArgs, true)
      : null;
  const sensorSet =
    Object.keys(sensorArgs).length > 0
      ? await safeInvoke<Partial<SensorState>>('sensor.set', sensorArgs, true)
      : null;

  const ok = Boolean(miscSet?.ok && sensorSet?.ok);
  await bridgeCommands.reportSelftest({
    mode: 'write',
    ok,
    miscGet,
    miscSet,
    sensorGet,
    sensorSet,
    shell: 'web-next',
  });
}

async function runSelftestF26(): Promise<void> {
  const keysBefore = await safeInvoke<KeysState>('keys.get');
  const origRow = keysBefore.ok ? keysBefore.data.bindings.find((b) => b.index === 4) : null;
  const origBinding = origRow?.binding ?? null;

  if (!origBinding || origBinding.startsWith('Macro:')) {
    await bridgeCommands.reportSelftest({
      mode: 'f26',
      ok: false,
      error: 'button 4 baseline unreadable or already Macro',
      keysBefore,
      shell: 'web-next',
    });
    return;
  }

  const miscBefore = await safeInvoke<MiscState>('misc.get');
  const origLoops =
    miscBefore.ok && typeof miscBefore.data.fireLoops === 'number' && Number.isInteger(miscBefore.data.fireLoops)
      ? miscBefore.data.fireLoops
      : null;
  const origInterval =
    miscBefore.ok && typeof miscBefore.data.fireInterval === 'number' && Number.isInteger(miscBefore.data.fireInterval)
      ? miscBefore.data.fireInterval
      : null;

  const listBefore = await safeInvoke<MacroListItem[]>('macros.list');
  const usedSlots = new Set(listBefore.ok ? listBefore.data.map((m) => m.slot) : []);
  let testSlot = -1;
  for (let s = 9; s >= 0; s--) {
    if (!usedSlots.has(s)) {
      testSlot = s;
      break;
    }
  }
  if (testSlot < 0) {
    await bridgeCommands.reportSelftest({
      mode: 'f26',
      ok: false,
      error: 'no free macro slot',
      listBefore,
      shell: 'web-next',
    });
    return;
  }

  let saveRes: { ok: boolean } = { ok: false };
  let exportRes: { ok: boolean } = { ok: false };
  let uploadRes: { ok: boolean } = { ok: false };
  let boundAfterUpload = false;
  let fireMiscRes: { ok: boolean; data?: Partial<MiscState> } = { ok: false };
  let fireParamsOk = false;
  let fireKeyRes: { ok: boolean } = { ok: false };
  let unboundAfterFire = false;
  let restoreKeyRes: { ok: boolean } = { ok: false };
  let restoreMiscRes: { ok: boolean } = { ok: false };
  let deleteRes: { ok: boolean } = { ok: false };

  try {
    saveRes = await safeInvoke(
      'macros.save',
      {
        slot: testSlot,
        macro: {
          name: 'f26_selftest',
          looptype: 3,
          cycle: 1,
          uid: 20261123,
          item: [
            { key: 'KeyA', press: true, delay: 20 },
            { key: 'KeyA', press: false, delay: 0 },
          ],
        },
      },
      true,
    );
    exportRes = saveRes.ok ? await safeInvoke('macro.export', { slot: testSlot }) : { ok: false };
    uploadRes = exportRes.ok
      ? await safeInvoke('macros.upload', { slot: testSlot, buttonIndex: 4 }, true)
      : { ok: false };
    if (uploadRes.ok) {
      const listAfterUpload = await safeInvoke<MacroListItem[]>('macros.list');
      const slotAfterUpload = listAfterUpload.ok
        ? listAfterUpload.data.find((m) => m.slot === testSlot)
        : null;
      boundAfterUpload = Boolean(slotAfterUpload?.bind?.includes(4));
    }

    fireMiscRes = uploadRes.ok
      ? await safeInvoke<Partial<MiscState>>('misc.set', { fireLoops: 2, fireInterval: 50 }, true)
      : { ok: false };
    fireParamsOk = Boolean(
      fireMiscRes.ok &&
        fireMiscRes.data?.fireLoops === 2 &&
        fireMiscRes.data?.fireInterval === 50,
    );
    fireKeyRes = fireParamsOk
      ? await safeInvoke('keys.set', { index: 4, binding: 'Fire' }, true)
      : { ok: false };
    if (fireKeyRes.ok) {
      const listAfterFire = await safeInvoke<MacroListItem[]>('macros.list');
      const slotAfterFire = listAfterFire.ok
        ? listAfterFire.data.find((m) => m.slot === testSlot)
        : null;
      unboundAfterFire = Boolean(slotAfterFire && !slotAfterFire.bind.includes(4));
    }
  } finally {
    restoreKeyRes = await safeInvoke('keys.set', { index: 4, binding: origBinding }, true);
    restoreMiscRes =
      origLoops !== null && origInterval !== null
        ? await safeInvoke('misc.set', { fireLoops: origLoops, fireInterval: origInterval }, true)
        : { ok: true };
    deleteRes = saveRes.ok
      ? await safeInvoke('macros.delete', { slot: testSlot }, true)
      : { ok: true };
  }

  const restoredOk = Boolean(restoreKeyRes.ok && restoreMiscRes.ok && deleteRes.ok);
  const ok = Boolean(
    saveRes.ok &&
      exportRes.ok &&
      uploadRes.ok &&
      boundAfterUpload &&
      fireParamsOk &&
      fireKeyRes.ok &&
      unboundAfterFire &&
      restoredOk,
  );

  await bridgeCommands.reportSelftest({
    mode: 'f26',
    ok,
    testSlot,
    origBinding,
    wp1: {
      save: saveRes.ok,
      export: exportRes.ok,
      upload: uploadRes.ok,
      boundAfterUpload,
    },
    wp2: {
      fireMisc: fireMiscRes.ok,
      fireParamsOk,
      fireKey: fireKeyRes.ok,
      unboundAfterFire,
    },
    restore: {
      key: restoreKeyRes.ok,
      misc: restoreMiscRes.ok,
      del: deleteRes.ok,
    },
    shell: 'web-next',
  });
}

export async function runSelftestIfRequested(getSnapshot: () => {
  deviceCount: number;
  activeKey: string | null;
  rendered: boolean;
}): Promise<void> {
  const mode = detectSelftestMode();
  if (mode === 'none') return;

  // Allow React commit + microtasks to flush; avoid requestAnimationFrame because
  // hidden WebView2 selftest windows throttle rAF indefinitely.
  await new Promise<void>((resolve) => setTimeout(resolve, 50));

  if (mode === 'write') {
    await runSelftestWrite();
    return;
  }
  if (mode === 'f26') {
    await runSelftestF26();
    return;
  }

  const snap = getSnapshot();
  await bridgeCommands.reportSelftest({
    mode: 'render',
    ok: snap.rendered,
    deviceCount: snap.deviceCount,
    activeKey: snap.activeKey,
    shell: 'web-next',
  });
}
