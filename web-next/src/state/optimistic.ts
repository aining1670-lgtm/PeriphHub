/**
 * Tracks monotonically increasing write sequence numbers per logical field.
 * Ensures out-of-order/older responses never overwrite newer optimistic edits
 * and rollbacks only revert when no newer write has superseded the failed one.
 */
export class FieldVersionTracker {
  private seqs = new Map<string, number>();

  next(field: string): number {
    const v = (this.seqs.get(field) ?? 0) + 1;
    this.seqs.set(field, v);
    return v;
  }

  isLatest(field: string, seq: number): boolean {
    return this.seqs.get(field) === seq;
  }

  reset(): void {
    this.seqs.clear();
  }
}
