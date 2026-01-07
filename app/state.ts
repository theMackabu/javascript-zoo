import type { ColumnDef, SortSpec, TableState } from './types';

const DEFAULT_SORT: SortSpec[] = [
  { col: 'score', dir: 'desc' },
  { col: 'language', dir: 'asc' },
  { col: 'github_stars', dir: 'desc' },
  { col: 'engine', dir: 'asc' },
];
const VARIANTS_DEFAULT = 'base';
const DEFAULT_ARCH = 'amd64';
// Default mask that matches the v8-v7 benchmark preset.
const DEFAULT_MASK = 130688;

export function createInitialState(): TableState {
  return {
    arch: DEFAULT_ARCH,
    variants: false,
    jitless: false,
    search: '',
    sort: DEFAULT_SORT.map((item) => ({ ...item })),
    selected: {},
  };
}

export function initSelectedBenchmarks(state: TableState, columns: ColumnDef[]): void {
  for (const col of columns) {
    if (state.selected[col.key] === undefined) {
      state.selected[col.key] = true;
    }
  }
}

export function applySort(state: TableState, column: ColumnDef): void {
  if (state.sort[0]?.col === column.key) {
    state.sort[0].dir = state.sort[0].dir === 'desc' ? 'asc' : 'desc';
    return;
  }
  const dir = column.numeric ? 'desc' : 'asc';
  state.sort.unshift({ col: column.key, dir });
}

export function loadStateFromUrl(
  state: TableState,
  benchmarks: ColumnDef[],
  columns: ColumnDef[],
): void {
  if (typeof window === 'undefined') {
    return;
  }
  const params = new URLSearchParams(window.location.search);
  const variantsParam = params.get('variants') ?? VARIANTS_DEFAULT;
  state.variants = variantsParam === 'all' || variantsParam === 'true';
  state.jitless = variantsParam === 'jitless';
  state.arch = params.get('arch') ?? state.arch;

  const sortMatch = (params.get('sort') ?? '').match(/^(-?)(\w+)$/);
  const validCols = new Set(columns.map((col) => col.key));
  if (sortMatch && validCols.has(sortMatch[2])) {
    state.sort = [{
      col: sortMatch[2],
      dir: sortMatch[1] === '-' ? 'desc' : 'asc',
    }];
  }

  const maskParam = params.get('mask');
  let mask = maskParam ? Number.parseInt(maskParam, 10) : DEFAULT_MASK;
  // Legacy URL param: v8=false meant "show all benchmarks".
  if (params.get('v8') === 'false') {
    mask = 0;
  }
  benchmarks.forEach((col, index) => {
    state.selected[col.key] = ((mask >> index) & 1) === 0;
  });

  if (state.sort.length === 1) {
    const primary = state.sort[0];
    state.sort = [
      primary,
      ...DEFAULT_SORT.filter((item) => item.col !== primary.col),
    ];
  }
}

export function saveStateToUrl(state: TableState, benchmarks: ColumnDef[]): void {
  if (typeof window === 'undefined') {
    return;
  }
  const params = new URLSearchParams();
  if (state.arch !== DEFAULT_ARCH) {
    params.set('arch', state.arch);
  }
  if (state.variants) {
    params.set('variants', 'all');
  } else if (state.jitless) {
    params.set('variants', 'jitless');
  } else if (VARIANTS_DEFAULT !== 'base') {
    params.set('variants', VARIANTS_DEFAULT);
  }
  if (state.sort[0]) {
    const sortVal = (state.sort[0].dir === 'desc' ? '-' : '') + state.sort[0].col;
    if (sortVal !== '-score') {
      params.set('sort', sortVal);
    }
  }

  let mask = 0;
  benchmarks.forEach((col, index) => {
    if (!state.selected[col.key]) {
      mask |= 1 << index;
    }
  });
  if (mask !== DEFAULT_MASK) {
    params.set('mask', String(mask));
  }

  const next = params.toString();
  const hash = window.location.hash;
  const url = window.location.pathname + (next ? `?${next}` : '') + hash;
  const current = window.location.search.replace(/^\?/, '');
  if (current !== next) {
    window.history.replaceState(null, '', url);
  }
}
