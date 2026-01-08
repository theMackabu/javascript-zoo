<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from 'vue';
import enginesData from '../engines.json';
import { ALL_COLUMNS, BENCHMARK_COLUMNS } from './columns';
import { buildRows, sortRows } from './data';
import EngineTableControls from './EngineTableControls.vue';
import {
  formatBenchmark,
  formatBinarySize,
  formatConformance,
  formatDescription,
  formatJit,
  formatLanguage,
  formatLicense,
  formatLoc,
  formatStars,
  formatStandard,
  formatYears,
} from './format';
import {
  applySort,
  createInitialState,
  initSelectedBenchmarks,
  loadStateFromUrl,
  saveStateToUrl,
} from './state';
import type { CellContent, ColumnDef, EngineEntry, TableRow, TableState } from './types';

const rawEngines = enginesData as EngineEntry[];
const props = withDefaults(defineProps<{ state?: TableState; showControls?: boolean }>(), {
  showControls: true,
});
const emit = defineEmits<{
  (event: 'select-engine', id: string): void;
}>();
const internalState = reactive(createInitialState());
const state = props.state ?? internalState;
const hydrated = ref(false);

initSelectedBenchmarks(state, BENCHMARK_COLUMNS);

function withBase(path: string): string {
  const base = import.meta.env.BASE_URL || '/';
  if (base === '/') {
    return path;
  }
  return base.replace(/\/$/, '') + path;
}

const lastVisibleBenchmark = computed(() => {
  const visible = BENCHMARK_COLUMNS.filter((col) => state.selected[col.key]);
  return visible.length ? visible[visible.length - 1].key : null;
});

const rows = computed(() => {
  const data = buildRows(rawEngines, state, BENCHMARK_COLUMNS);
  return sortRows(data, state.sort);
});

const columns = computed(() => ALL_COLUMNS);

const displayRows = computed(() => {
  return rows.value.map((row) => {
    const cells: Record<string, CellContent> = {};
    for (const col of columns.value) {
      cells[col.key] = renderCell(col, row);
    }
    return { row, cells };
  });
});

function isBenchmarkVisible(col: ColumnDef): boolean {
  if (!col.benchmark) {
    return true;
  }
  return Boolean(state.selected[col.key]);
}

function isSorted(col: ColumnDef): boolean {
  return state.sort[0]?.col === col.key;
}

function columnClasses(col: ColumnDef): string[] {
  const classes = [col.key];
  if (col.numeric) {
    classes.push('numeric');
  }
  if (col.benchmark) {
    classes.push('benchmark');
  }
  if (col.className) {
    classes.push(col.className);
  }
  if (isSorted(col)) {
    classes.push('sorted-column');
  }
  if (col.benchmark && !state.selected[col.key]) {
    classes.push('excluded-column');
  }
  if (!isBenchmarkVisible(col)) {
    classes.push('hidden');
  }
  if (lastVisibleBenchmark.value && col.key === lastVisibleBenchmark.value) {
    classes.push('last');
  }
  return classes;
}

function headerClasses(col: ColumnDef): string[] {
  const classes = columnClasses(col);
  if (isSorted(col)) {
    classes.push(state.sort[0].dir === 'asc' ? 'sort-asc' : 'sort-desc');
  }
  return classes;
}

function cellClasses(col: ColumnDef, cell: CellContent): string[] {
  const classes = columnClasses(col);
  if (cell.className) {
    classes.push(cell.className);
  }
  return classes;
}

function shortBenchmarkLabel(key: string): string {
  return key.replace('Latency', 'L');
}

function onHeaderClick(col: ColumnDef, event: MouseEvent): void {
  const target = event.target as HTMLElement;
  if (target?.tagName?.toUpperCase() === 'INPUT') {
    return;
  }
  applySort(state, col);
}

function onTableClick(event: MouseEvent): void {
  const target = event.target as HTMLElement | null;
  if (!target) {
    return;
  }
  if (target.closest('thead')) {
    return;
  }
  if (target.closest('button, input, select, textarea, label, summary')) {
    return;
  }
  const link = target.closest('a.engine-name') as HTMLAnchorElement | null;
  const selection = window.getSelection();
  if (selection && selection.type === 'Range' && selection.toString()) {
    return;
  }
  if (link) {
    const id = link.dataset.engineId;
    if (!id) {
      return;
    }
    event.preventDefault();
    emit('select-engine', id);
    return;
  }
  const cell = target.closest('td');
  if (!cell || target !== cell) {
    return;
  }
  const row = cell.parentElement;
  const id = row?.dataset.engineId;
  if (!id) {
    return;
  }
  emit('select-engine', id);
}

function engineLink(row: TableRow): string {
  if (row.id) {
    const search = typeof window === 'undefined' ? '' : window.location.search;
    return `${withBase('/')}${search}#${row.id}`;
  }
  return row.jsz_url ?? '#';
}

function revisionLink(row: TableRow): CellContent {
  const version = (row.version ?? '').replace(/^nightly$/, '');
  let revLink = row.repository ?? '';

  if (row.revision && row.repository) {
    if (row.repository.includes('github.com')) {
      revLink = `${row.repository.replace('.git', '')}/tree/${row.revision}`;
    } else if (row.repository.includes('codeberg.org')) {
      revLink = `${row.repository.replace('.git', '')}/src/commit/${row.revision}`;
    } else if (row.repository === 'https://chromium.googlesource.com/v8/v8.git') {
      revLink = `https://github.com/v8/v8/tree/${row.revision}`;
    }
  }

  if (row.revision_date) {
    if (!version || (row.revision && version.includes(row.revision.slice(0, 7)))) {
      return {
        html: `<a class="engine-version" href="${revLink}" target="_blank" rel="noreferrer">${row.revision_date}</a>`,
        title: row.version,
      };
    }
    return {
      html: `<a class="engine-version" href="${revLink}" target="_blank" rel="noreferrer">${row.revision_date} (${version})</a>`,
      title: row.version,
    };
  }

  if (version && revLink) {
    return {
      html: `<a class="engine-version" href="${revLink}" target="_blank" rel="noreferrer">${version}</a>`,
    };
  }

  return {};
}

function renderCell(col: ColumnDef, row: TableRow): CellContent {
  if (col.key === 'engine') {
    const name = row.title ?? row.engine ?? '';
    const variant = row.variant ? `<div class="engine-variant">${row.variant}</div>` : '';
    const link = engineLink(row);
    const revision = revisionLink(row);
    const version = `<div class="engine-version">${revision.html ?? ''}</div>`;

    return {
      html: `
        <div class="engine-cell">
          <div class="engine-name-variant">
            <a href="${link}" class="engine-name" data-engine-id="${row.id ?? ''}">${name}</a>${variant}
          </div>
          ${version}
        </div>
      `,
      title: revision.title,
    };
  }

  if (col.key === 'loc') {
    return formatLoc(row.loc as number | undefined);
  }

  if (col.key === 'binary_size') {
    return formatBinarySize(row.binary_size as number | undefined, row.dist_size as number | undefined);
  }

  if (col.key === 'github_stars') {
    return formatStars(row.github_stars as number | undefined, row.repository as string | undefined, row.github as string | undefined);
  }

  if (col.key === 'github_contributors') {
    return formatStars(row.github_contributors as number | undefined, row.repository as string | undefined, row.github as string | undefined);
  }

  if (col.key === 'license') {
    return formatLicense(row.license as string | undefined, row.license_abbr as string | undefined);
  }

  if (col.key === 'standard') {
    return formatStandard(row.standard as string | undefined);
  }

  if (col.key.startsWith('es') || col.key.startsWith('kangax')) {
    return formatConformance(row[col.key]);
  }

  if (col.key === 'language') {
    return formatLanguage(row.language as string | undefined);
  }

  if (col.key === 'years') {
    return formatYears(row.years as string | undefined);
  }

  if (col.key === 'jit') {
    return formatJit(row.jit as string | undefined);
  }

  if (col.key === 'description') {
    return formatDescription(row);
  }

  if (col.benchmark) {
    return formatBenchmark(row[col.key], row[`${col.key}_detailed`], row[`${col.key}_error`]);
  }

  const value = row[col.key];
  return value ? { text: String(value) } : {};
}

onMounted(() => {
  loadStateFromUrl(state, BENCHMARK_COLUMNS, ALL_COLUMNS);
  hydrated.value = true;
});

watch(
  state,
  () => {
    if (!hydrated.value) {
      return;
    }
    saveStateToUrl(state, BENCHMARK_COLUMNS);
  },
  { deep: true },
);
</script>

<template>
  <section class="jsz-table">
    <EngineTableControls v-if="props.showControls" :state="state" class="table-controls" />

    <div class="table-container" @click="onTableClick">
      <div class="table-scroll">
        <table>
        <thead>
          <tr>
            <th
              v-for="col in columns"
              :key="col.key"
              :class="headerClasses(col)"
              :title="col.title"
              @click="onHeaderClick(col, $event)"
            >
              <template v-if="col.benchmark">
                <div class="control">
                  {{ shortBenchmarkLabel(col.label) }}
                </div>
              </template>
              <template v-else>
                {{ col.label }}
              </template>
            </th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="item in displayRows"
            :key="`${item.row.id ?? item.row.title}-${item.row.variant ?? ''}-${item.row.arch ?? ''}`"
            :data-engine-id="item.row.id ?? ''"
          >
            <td
              v-for="col in columns"
              :key="col.key"
              :class="cellClasses(col, item.cells[col.key])"
            >
              <template v-if="item.cells[col.key].html">
                <span v-html="item.cells[col.key].html" :title="item.cells[col.key].title"></span>
              </template>
              <template v-else>
                <span :title="item.cells[col.key].title">{{ item.cells[col.key].text ?? '' }}</span>
              </template>
            </td>
          </tr>
        </tbody>
        </table>
      </div>
    </div>
  </section>
</template>

<style scoped>
.jsz-table {
  color: var(--text-primary);
  font-family: Roboto, -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif;
  font-size: 14px;
  width: 100%;
  background-color: var(--bg-primary);
}

.table-controls {
  padding: 8px 16px;
  border-bottom: 1px solid var(--border-light);
  position: sticky;
  top: var(--app-header-height);
  background: color-mix(in srgb, var(--bg-primary) 92%, transparent);
  backdrop-filter: blur(6px);
  z-index: 7;
}

.jsz-table input,
.jsz-table select,
.jsz-table button {
  accent-color: var(--text-accent);
  border: 1px solid var(--border-medium);
  border-radius: 4px;
  background-color: var(--bg-control);
  color: var(--text-primary);
  padding: 4px 6px;
  cursor: pointer;
}

.jsz-table label {
  cursor: pointer;
}

.table-container {
  width: 100%;
  overflow: visible;
  border: 0;
  border-radius: 0;
  box-shadow: none;
  position: relative;
  z-index: 1;
}

.table-scroll {
  overflow: visible;
}

.table-container table {
  border-collapse: collapse;
  border: 0px;
  margin: 0;
  width: 100%;
  min-width: max-content;
}

.table-container th,
.table-container td {
  padding: 10px 12px;
  text-align: left;
  border-bottom: 1px solid var(--border-muted);
  vertical-align: top;
  font-family: Roboto, -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif;
  font-size: 14px;
}

.table-container thead th {
  border-bottom: 0;
}

.table-container th {
  position: sticky;
  top: var(--app-header-height);
  background: color-mix(in srgb, var(--bg-thead) 88%, transparent);
  backdrop-filter: blur(8px);
  font-weight: 500;
  cursor: pointer;
  z-index: 5;
  pointer-events: auto;
  text-align: left;
  vertical-align: center;
  color: var(--text-primary);
  white-space: nowrap;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  font-size: 11px;
  padding-top: 14px;
  padding-bottom: 14px;
}

.table-container th::before {
  content: '';
  position: absolute;
  left: 0;
  right: 0;
  bottom: 0;
  height: 1px;
  background: var(--border-muted);
  pointer-events: none;
}

.table-container th.sort-asc:not(.benchmark)::after,
.table-container th.sort-desc:not(.benchmark)::after {
  color: var(--text-accent);
}

.table-container th.benchmark.sort-asc .control::after,
.table-container th.benchmark.sort-desc .control::after {
  color: var(--text-accent);
}

.benchmark {
  padding: 8px 4px;
  text-align: right;
  font-family: 'Roboto Condensed', Roboto, -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif;
  color: var(--text-numeric);
}

.table-container th.benchmark {
  padding: 8px 4px;
  text-align: right;
}

.benchmark .control {
  font-size: 9px;
  font-weight: 500;
  display: flex;
  margin: 0;
  padding: 0;
  align-items: center;
  white-space: nowrap;
}


.numeric {
  text-align: right;
  color: var(--text-numeric);
}


.numeric.red {
  color: var(--text-red);
}

.hidden {
  display: none;
}

.sorted-column {
  background-color: var(--bg-sorted);
}

.table-container th.sorted-column {
  color: var(--text-primary);
}

.table-container th.sorted-column::before {
  background: var(--text-accent);
  height: 2px;
}

.sort-asc::after,
.sort-desc::after {
  display: inline-block;
  margin-left: 4px;
  font-size: 10px;
  color: var(--text-accent);
  white-space: nowrap;
}

.sort-asc::after {
  content: '▲';
}

.sort-desc::after {
  content: '▼';
}

.benchmark.sort-asc .control::after,
.benchmark.sort-desc .control::after {
  display: inline-block;
  margin-left: 4px;
  font-size: 10px;
  color: var(--text-accent);
}

.benchmark.sort-asc .control::after {
  content: '▲';
}

.benchmark.sort-desc .control::after {
  content: '▼';
}

.excluded-column {
  opacity: 0.55;
}

.engine-cell {
  min-width: 120px;
}

.engine-name-variant {
  white-space: nowrap;
}

:deep(.engine-name) {
  font-weight: 600;
  color: var(--text-accent);
  text-decoration: none;
}

:deep(.engine-name:hover) {
  text-decoration: underline;
  text-decoration-skip-ink: none;
}

:deep(.engine-variant) {
  font-size: 12px;
  padding: 2px 4px;
  border-radius: 6px;
  display: inline-block;
  margin-left: 5px;
  color: var(--bg-primary);
  background-color: var(--text-accent);
}

:deep(.engine-version) {
  color: var(--text-muted);
  font-size: 12px;
  display: block;
}

:deep(.engine-version a) {
  color: var(--text-muted);
  text-decoration: none;
}

:global(.dark) :deep(.engine-version a) {
  color: #99b3ff !important;
}

:deep(.engine-version a:hover) {
  text-decoration: underline;
  text-decoration-skip-ink: none;
}

.description-cell {
  min-width: 300px;
  max-width: 360px;
  white-space: normal;
  word-wrap: break-word;
  overflow-wrap: anywhere;
}

.license-cell {
  max-width: 100px;
  white-space: nowrap;
}

.missing {
  background-color: var(--bg-missing);
}

.table-container tbody tr:nth-child(odd) td {
  background-color: var(--bg-row-odd);
}

.table-container tbody tr:nth-child(even) td {
  background-color: var(--bg-row-even);
}

.table-container tbody td.sorted-column {
  background-color: var(--bg-sorted) !important;
}

.table-container tbody tr:hover td {
  background-color: var(--bg-hover) !important;
}

@media (max-width: 720px) {
  .table-scroll {
    overflow-x: auto;
    overflow-y: visible;
    max-width: 100%;
    -webkit-overflow-scrolling: touch;
  }

  .table-container th {
    position: static;
  }
}
</style>
