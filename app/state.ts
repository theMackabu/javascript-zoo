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
    visibleColumns: {},
    showEngineVersion: true,
    columnOrder: [],
  };
}

export function initVisibleColumns(state: TableState, columns: ColumnDef[]): void {
  for (const col of columns) {
    if (col.key === 'engine') {
      state.visibleColumns[col.key] = true;
      continue;
    }
    if (state.visibleColumns[col.key] === undefined) {
      state.visibleColumns[col.key] = !col.defaultHidden;
    }
  }
}

export function initColumnOrder(state: TableState, columns: ColumnDef[]): void {
  if (state.columnOrder.length === 0) {
    state.columnOrder = columns
      .filter((col) => !col.benchmark && col.key !== 'engine')
      .map((col) => col.key);
    return;
  }
  const available = new Set(columns.filter((col) => !col.benchmark && col.key !== 'engine').map((col) => col.key));
  state.columnOrder = state.columnOrder.filter((key) => available.has(key));
  for (const key of available) {
    if (!state.columnOrder.includes(key)) {
      state.columnOrder.push(key);
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
  columns: ColumnDef[],
  benchmarks: ColumnDef[],
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

  const columnsParam = params.get('columns');
  if (columnsParam) {
    const requested = columnsParam.split(/\s+/).map((key) => key.trim()).filter(Boolean);
    const valid = new Set(columns.map((col) => col.key));
    const visible = new Set(requested.filter((key) => valid.has(key)));
    visible.add('engine');
    for (const col of columns) {
      if (col.key === 'engine') {
        state.visibleColumns[col.key] = true;
        continue;
      }
      state.visibleColumns[col.key] = visible.has(col.key);
    }
    const ordered = requested.filter((key) => {
      const col = columns.find((item) => item.key === key);
      return col && !col.benchmark && col.key !== 'engine';
    });
    state.columnOrder = ordered;
  } else {
    const maskParam = params.get('mask');
    let mask = maskParam ? Number.parseInt(maskParam, 10) : DEFAULT_MASK;
    // Legacy URL param: v8=false meant "show all benchmarks".
    if (params.get('v8') === 'false') {
      mask = 0;
    }
    benchmarks.forEach((col, index) => {
      state.visibleColumns[col.key] = ((mask >> index) & 1) === 0;
    });
  }

  if (state.sort.length === 1) {
    const primary = state.sort[0];
    state.sort = [
      primary,
      ...DEFAULT_SORT.filter((item) => item.col !== primary.col),
    ];
  }
}

export function saveStateToUrl(
  state: TableState,
  columns: ColumnDef[],
  benchmarks: ColumnDef[],
): void {
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

  const orderedBase = columns.filter((col) => !col.benchmark && col.key !== 'engine');
  const baseMap = new Map(orderedBase.map((col) => [col.key, col]));
  const orderedKeys: string[] = [];
  for (const key of state.columnOrder) {
    if (baseMap.has(key)) {
      orderedKeys.push(key);
    }
  }
  for (const col of orderedBase) {
    if (!orderedKeys.includes(col.key)) {
      orderedKeys.push(col.key);
    }
  }
  for (const col of benchmarks) {
    orderedKeys.push(col.key);
  }
  const visibleKeys = orderedKeys.filter((key) => state.visibleColumns[key]);
  const defaultVisible = columns
    .filter((col) => !col.benchmark && !col.defaultHidden && col.key !== 'engine')
    .map((col) => col.key);
  benchmarks.forEach((col, index) => {
    const hidden = ((DEFAULT_MASK >> index) & 1) === 1;
    if (!hidden) {
      defaultVisible.push(col.key);
    }
  });
  if (visibleKeys.join(' ') !== defaultVisible.join(' ')) {
    params.set('columns', visibleKeys.join(' '));
  }

  const next = params.toString();
  const hash = window.location.hash;
  const url = window.location.pathname + (next ? `?${next}` : '') + hash;
  const current = window.location.search.replace(/^\?/, '');
  if (current !== next) {
    window.history.replaceState(null, '', url);
  }
}
