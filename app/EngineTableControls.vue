<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { BENCHMARK_COLUMNS } from './columns';
import GitHubIcon from './GitHubIcon.vue';
import type { TableState } from './types';

type ControlItem =
  | { key: 'github'; label: string; type: 'link' }
  | { key: 'arch'; label: string; type: 'arch' }
  | { key: 'search'; label: string; type: 'search' }
  | { key: 'theme'; label: string; type: 'theme' };
type BenchmarkItem = { key: string; label: string };

const props = withDefaults(defineProps<{
  state: TableState;
  theme?: 'light' | 'dark';
  toggleTheme?: () => void;
  showTheme?: boolean;
}>(), {
  theme: 'light',
  toggleTheme: () => {},
  showTheme: false,
});
const state = props.state;

const controlsRootRef = ref<HTMLElement | null>(null);
const controlsMainRef = ref<HTMLElement | null>(null);
const searchRef = ref<HTMLInputElement | null>(null);
const menuSearchRef = ref<HTMLInputElement | null>(null);
const archRef = ref<HTMLElement | null>(null);
const overflowMeasureRef = ref<HTMLElement | null>(null);
const measureRefs = ref<Record<string, HTMLElement>>({});
const visibleCount = ref(3);

const items = computed<ControlItem[]>(() => {
  const base: ControlItem[] = [
    { key: 'arch', label: 'Arch', type: 'arch' },
    { key: 'search', label: 'Search', type: 'search' },
  ];
  if (props.showTheme) {
    base.push({ key: 'theme', label: 'Theme', type: 'theme' });
  }
  base.push({ key: 'github', label: 'GitHub', type: 'link' });
  return base;
});

const benchmarkItems = computed<BenchmarkItem[]>(() => {
  return BENCHMARK_COLUMNS.map((col) => ({
    key: col.key,
    label: col.label,
  }));
});

function applyBenchmarkPreset(preset: 'v8' | 'all') {
  if (preset === 'v8') {
    for (const col of BENCHMARK_COLUMNS) {
      state.selected[col.key] = Boolean(col.v8);
    }
    return;
  }
  for (const col of BENCHMARK_COLUMNS) {
    state.selected[col.key] = true;
  }
}

const visibleItems = computed(() => items.value.slice(0, visibleCount.value));
const overflowItems = computed(() => items.value.slice(visibleCount.value));
const hasOverflowItems = computed(() => overflowItems.value.length > 0);
const showOverflow = computed(() => true);
const archOpen = ref(false);
const overflowOpen = ref(false);
const overflowHover = ref(false);
const suppressOverflowHover = ref(false);
const menuOpen = computed(() => overflowOpen.value || (overflowHover.value && !suppressOverflowHover.value));

function setArch(next: 'amd64' | 'arm64') {
  state.arch = next;
  archOpen.value = false;
}

function setTheme(next: 'light' | 'dark') {
  if (next !== props.theme) {
    props.toggleTheme();
  }
}

function toggleOverflow() {
  if (overflowOpen.value) {
    overflowOpen.value = false;
    if (overflowHover.value) {
      suppressOverflowHover.value = true;
    }
    return;
  }
  overflowOpen.value = true;
  suppressOverflowHover.value = false;
}

function onOverflowEnter() {
  overflowHover.value = true;
}

function onOverflowLeave() {
  overflowHover.value = false;
  suppressOverflowHover.value = false;
}

function setSearchRef(el: HTMLInputElement | null) {
  if (el) {
    searchRef.value = el;
  }
}

function focusSearch() {
  const inline = searchRef.value;
  if (inline && typeof inline.focus === 'function') {
    inline.focus();
    inline.select();
    return;
  }
  overflowOpen.value = true;
  nextTick(() => {
    menuSearchRef.value?.focus();
    menuSearchRef.value?.select();
  });
}

function toggleFilter(filter: 'variants' | 'jitless') {
  if (filter === 'variants') {
    state.variants = !state.variants;
    if (state.variants) {
      state.jitless = false;
    }
    return;
  }
  state.jitless = !state.jitless;
  if (state.jitless) {
    state.variants = false;
  }
}

function setArchRef(el: HTMLElement | null) {
  if (el) {
    archRef.value = el;
  }
}

function setMeasureRef(key: string, el: HTMLElement | null) {
  if (el) {
    measureRefs.value[key] = el;
  }
}

function computeVisibleCount() {
  const main = controlsMainRef.value;
  const root = controlsRootRef.value;
  if (!main || !root) {
    return;
  }

  const mainStyle = window.getComputedStyle(main);
  const mainGap = Number.parseFloat(mainStyle.columnGap || mainStyle.gap || '0');
  const inlineGap = 20;
  const overflowWidth = overflowMeasureRef.value?.getBoundingClientRect().width ?? 0;
  const totalWidth = root.getBoundingClientRect().width;

  const available = totalWidth - (overflowWidth + mainGap);
  let used = 0;
  let count = 0;

  for (const item of items.value) {
    const el = measureRefs.value[item.key];
    if (!el) {
      continue;
    }
    const width = el.getBoundingClientRect().width;
    const nextUsed = used + width + (count > 0 ? inlineGap : 0);
    if (nextUsed > available) {
      break;
    }
    used = nextUsed;
    count += 1;
  }

  visibleCount.value = count;
}

let resizeObserver: ResizeObserver | null = null;
let handleClickOutside: ((event: MouseEvent) => void) | null = null;
let handleSearchShortcut: ((event: KeyboardEvent) => void) | null = null;

onMounted(() => {
  resizeObserver = new ResizeObserver(() => {
    computeVisibleCount();
  });
  if (controlsMainRef.value) {
    resizeObserver.observe(controlsMainRef.value);
  }
  window.addEventListener('resize', computeVisibleCount);
  nextTick(() => {
    computeVisibleCount();
  });

  handleClickOutside = (event: MouseEvent) => {
    const target = event.target as Node | null;
    if (archOpen.value) {
      if (archRef.value && target && archRef.value.contains(target)) {
        return;
      }
      archOpen.value = false;
    }
    if (overflowOpen.value) {
      const overflowEl = controlsRootRef.value?.querySelector('.controls-overflow');
      if (overflowEl && target && overflowEl.contains(target)) {
        return;
      }
      overflowOpen.value = false;
    }
  };
  document.addEventListener('mousedown', handleClickOutside);

  handleSearchShortcut = (event: KeyboardEvent) => {
    if (event.key !== '/' && event.code !== 'Slash') {
      return;
    }
    const target = event.target as HTMLElement | null;
    if (target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable)) {
      return;
    }
    event.preventDefault();
    focusSearch();
  };
  window.addEventListener('keydown', handleSearchShortcut, true);
});

watch(items, () => {
  nextTick(() => {
    computeVisibleCount();
  });
});

watch(visibleItems, () => {
  const hasArch = visibleItems.value.some((item) => item.type === 'arch');
  if (!hasArch) {
    archOpen.value = false;
  }
});

watch(overflowItems, () => {
  const hasSearch = overflowItems.value.some((item) => item.type === 'search');
  if (hasSearch) {
    nextTick(() => {
      searchRef.value?.blur();
    });
  }
});

onBeforeUnmount(() => {
  resizeObserver?.disconnect();
  window.removeEventListener('resize', computeVisibleCount);
  if (handleClickOutside) {
    document.removeEventListener('mousedown', handleClickOutside);
  }
  if (handleSearchShortcut) {
    window.removeEventListener('keydown', handleSearchShortcut, true);
  }
});
</script>

<template>
  <div class="engine-controls" ref="controlsRootRef">
    <div class="controls-main" ref="controlsMainRef">
      <div class="controls-inline">
        <template v-for="item in visibleItems" :key="item.key">
          <a
            v-if="item.type === 'link'"
            class="icon-link"
            href="https://github.com/ivankra/javascript-zoo"
            target="_blank"
            rel="noreferrer"
            aria-label="Open GitHub repository"
            title="Open the GitHub repository"
          >
            <GitHubIcon :size="20" />
          </a>
          <div v-else-if="item.type === 'arch'" class="arch-select" :class="{ open: archOpen }" :ref="setArchRef">
            <button
              class="arch-trigger"
              type="button"
              aria-label="Select architecture"
              title="Select benchmark architecture"
              @click="archOpen = !archOpen"
            >
              <span class="arch-label">{{ state.arch }}</span>
              <span class="arch-caret">▾</span>
            </button>
            <div class="arch-menu" role="listbox" aria-label="Architecture">
              <button type="button" @click="setArch('amd64')">
                <span class="arch-short">amd64</span>
                <span class="arch-detail">i9-10900K 3.7-5.3GHz · Linux</span>
              </button>
              <button type="button" @click="setArch('arm64')">
                <span class="arch-short">arm64</span>
                <span class="arch-detail">Mac M4 4.5GHz · Linux VM</span>
              </button>
            </div>
          </div>
          <div v-else-if="item.type === 'search'" class="search-field">
            <input
              :ref="setSearchRef"
              v-model="state.search"
              class="search-input"
              type="search"
              placeholder="Search..."
              aria-label="Search engines"
              title="Search for a string or /regex/"
            />
          </div>
          <button
            v-else
            class="theme-toggle inline"
            type="button"
            @click="toggleTheme"
            title="Toggle theme"
          >
            {{ theme === 'dark' ? '☀️' : '🌙' }}
          </button>
        </template>
      </div>
    </div>

    <div
      v-if="showOverflow"
      class="controls-overflow"
      :class="{ open: menuOpen }"
      @mouseenter="onOverflowEnter"
      @mouseleave="onOverflowLeave"
    >
      <button
        class="more-trigger"
        type="button"
        aria-label="More filters"
        :aria-expanded="menuOpen"
        @click="toggleOverflow"
      >
        •••
      </button>
      <div class="more-menu" role="menu">
        <template v-for="item in overflowItems" :key="item.key">
          <div v-if="item.type === 'arch'" class="menu-section">
            <div class="menu-title">Architecture</div>
            <div class="preset-row">
            <button
              class="menu-button"
              :class="{ active: state.arch === 'amd64' }"
              type="button"
              title="amd64: i9-10900K 3.7-5.3GHz - Linux"
              @click="setArch('amd64')"
            >
              amd64
            </button>
            <button
              class="menu-button"
              :class="{ active: state.arch === 'arm64' }"
              type="button"
              title="arm64: Mac M4 4.5GHz - Linux VM"
              @click="setArch('arm64')"
            >
              arm64
            </button>
            </div>
          </div>
          <div v-else-if="item.type === 'link'" class="menu-section">
            <div class="menu-title">GitHub</div>
            <a
              class="menu-button icon-button"
              href="https://github.com/ivankra/javascript-zoo"
              target="_blank"
              rel="noreferrer"
            >
              <GitHubIcon :size="16" />
              Open repo
            </a>
          </div>
          <div v-else-if="item.type === 'search'" class="menu-section">
            <div class="menu-title">Search</div>
            <div class="search-field menu-search">
            <input
              v-model="state.search"
              class="search-input"
              type="search"
              placeholder="Search..."
              aria-label="Search engines"
              title="Search for a string or /regex/"
              ref="menuSearchRef"
            />
            </div>
          </div>
          <div v-else class="menu-section">
            <div class="menu-title">Theme</div>
            <div class="preset-row">
              <button
                class="menu-button"
                :class="{ active: theme === 'light' }"
                type="button"
                @click="setTheme('light')"
              >
                Light
              </button>
              <button
                class="menu-button"
                :class="{ active: theme === 'dark' }"
                type="button"
                @click="setTheme('dark')"
              >
                Dark
              </button>
            </div>
          </div>
        </template>
        <div v-if="overflowItems.length" class="menu-divider"></div>
        <div class="menu-section">
          <div class="menu-title">Variants</div>
          <div class="preset-row">
            <button
              class="menu-button"
              :class="{ active: state.variants }"
              type="button"
              @click="toggleFilter('variants')"
            >
              Show all
            </button>
            <button
              class="menu-button"
              :class="{ active: state.jitless }"
              type="button"
              @click="toggleFilter('jitless')"
            >
              JIT-less only
            </button>
          </div>
        </div>
        <div class="menu-divider"></div>
        <div class="menu-section">
          <div class="menu-title">Benchmarks</div>
          <div class="preset-row">
            <button
              class="menu-button"
              type="button"
              title="Select only v8-v7 benchmarks"
              @click="applyBenchmarkPreset('v8')"
            >
              v8-v7
            </button>
            <button
              class="menu-button"
              type="button"
              title="Select all benchmarks (v8-v9)"
              @click="applyBenchmarkPreset('all')"
            >
              v8-v9
            </button>
          </div>
          <label v-for="item in benchmarkItems" :key="item.key" class="control-toggle">
            <input
              type="checkbox"
              :checked="state.selected[item.key]"
              @change="state.selected[item.key] = ($event.target as HTMLInputElement).checked"
            />
            {{ item.label }}
          </label>
        </div>
      </div>
    </div>
  </div>
  <div class="controls-measure" aria-hidden="true">
    <template v-for="item in items" :key="item.key">
      <button
        v-if="item.type === 'theme'"
        :ref="(el) => setMeasureRef(item.key, el as HTMLElement | null)"
        class="theme-toggle inline"
        type="button"
      >
        {{ theme === 'dark' ? '☀️' : '🌙' }}
      </button>
      <div
        v-else-if="item.type === 'search'"
        :ref="(el) => setMeasureRef(item.key, el as HTMLElement | null)"
        class="search-field"
      >
        <input
          class="search-input"
          type="search"
          placeholder="Search..."
        />
      </div>
      <button
        v-else-if="item.type === 'arch'"
        :ref="(el) => setMeasureRef(item.key, el as HTMLElement | null)"
        class="arch-trigger"
        type="button"
      >
        <span class="arch-label">{{ state.arch }}</span>
        <span class="arch-caret">▾</span>
      </button>
      <a
        v-else
        :ref="(el) => setMeasureRef(item.key, el as HTMLElement | null)"
        class="icon-link"
        href="https://github.com/ivankra/javascript-zoo"
      >
        <GitHubIcon :size="20" />
      </a>
    </template>
    <button ref="overflowMeasureRef" class="more-trigger" type="button" aria-hidden="true">•••</button>
  </div>
</template>

<style scoped>
.engine-controls {
  display: flex;
  align-items: center;
  gap: 20px;
  font-size: 14px;
  color: var(--text-primary);
  min-width: 0;
  font-weight: 500;
  font-family: Inter, ui-sans-serif, system-ui, sans-serif, 'Apple Color Emoji', 'Segoe UI Emoji',
    'Segoe UI Symbol', 'Noto Color Emoji';
  flex: 1 1 auto;
  justify-content: flex-end;
}

.engine-controls input {
  accent-color: var(--text-accent);
}

.controls-main {
  display: flex;
  align-items: center;
  gap: 20px;
  min-width: 0;
}

.controls-inline {
  display: flex;
  align-items: center;
  gap: 20px;
  white-space: nowrap;
}

.search-field {
  position: relative;
  display: inline-flex;
  align-items: center;
}

.icon-link {
  display: inline-flex;
  align-items: center;
  color: var(--text-primary);
  text-decoration: none;
  height: 32px;
  width: 32px;
  justify-content: center;
}

.search-input {
  border: 1px solid var(--border-light);
  background: var(--bg-control);
  color: var(--text-primary);
  border-radius: 8px;
  height: 32px;
  padding: 0 10px;
  font-size: 14px;
  width: clamp(160px, 16vw, 240px);
}

.search-input:focus {
  outline: none;
  border-color: var(--text-accent);
}

.menu-search {
  width: 100%;
}

.control-toggle {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  cursor: pointer;
  font-weight: 500;
  letter-spacing: 0;
  text-transform: none;
}

.control-toggle input {
  margin: 0;
  transform: scale(0.85);
}

.arch-select {
  position: relative;
}

.arch-trigger {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  border: 1px solid var(--border-medium);
  background: var(--bg-control);
  color: var(--text-primary);
  border-radius: 6px;
  height: 32px;
  padding: 0 8px;
  cursor: pointer;
  font-size: 12px;
  font-weight: 600;
  text-transform: none;
  letter-spacing: 0;
  min-width: 76px;
  justify-content: center;
}

.arch-caret {
  font-size: 10px;
}

.arch-menu {
  position: absolute;
  right: 0;
  top: calc(100% + 6px);
  min-width: 240px;
  background: var(--bg-primary);
  border: 1px solid var(--border-light);
  border-radius: 8px;
  padding: 6px;
  display: grid;
  gap: 4px;
  opacity: 0;
  visibility: hidden;
  transform: translateY(-4px);
  transition: opacity 150ms ease, transform 150ms ease, visibility 150ms ease;
  box-shadow:
    0 8px 18px color-mix(in srgb, var(--border-medium) 45%, transparent),
    0 2px 6px color-mix(in srgb, var(--border-medium) 35%, transparent);
  z-index: 20;
}

.arch-select.open .arch-menu {
  opacity: 1;
  visibility: visible;
  transform: translateY(0);
}

.arch-menu button {
  border: 0;
  background: transparent;
  text-align: left;
  padding: 8px 10px;
  border-radius: 6px;
  cursor: pointer;
  color: var(--text-primary);
}

.arch-menu button:hover {
  background: var(--bg-hover);
}


.arch-short {
  font-weight: 600;
  display: block;
  margin-bottom: 2px;
}

.arch-detail {
  font-size: 11px;
  color: var(--text-muted);
}

.controls-overflow {
  position: relative;
}

.more-trigger {
  border: 0;
  background: transparent;
  color: var(--text-primary);
  border-radius: 0;
  padding: 0 6px;
  cursor: pointer;
  font-size: 12px;
  font-weight: 500;
  letter-spacing: 0.1em;
}

.more-menu {
  position: absolute;
  right: 0;
  top: calc(100% + 6px);
  min-width: 180px;
  background: var(--bg-primary);
  border: 1px solid var(--border-light);
  border-radius: 8px;
  padding: 8px;
  display: grid;
  gap: 8px;
  opacity: 0;
  visibility: hidden;
  transform: translateY(-4px);
  transition: opacity 150ms ease, transform 150ms ease, visibility 150ms ease;
  box-shadow:
    0 8px 18px color-mix(in srgb, var(--border-medium) 45%, transparent),
    0 2px 6px color-mix(in srgb, var(--border-medium) 35%, transparent);
  z-index: 20;
}

.menu-section {
  display: grid;
  gap: 8px;
}

.preset-row {
  display: flex;
  gap: 8px;
}

.preset-row .menu-button {
  padding: 4px 8px;
}

.menu-title {
  font-size: 11px;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.08em;
}

.menu-divider {
  height: 1px;
  background: var(--border-light);
}

.controls-overflow.open .more-menu {
  opacity: 1;
  visibility: visible;
  transform: translateY(0);
}

.theme-toggle {
  border: 1px solid var(--border-medium);
  background: var(--bg-control);
  color: var(--text-primary);
  border-radius: 6px;
  width: 32px;
  height: 32px;
  cursor: pointer;
  font-size: 13px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
}

.theme-toggle:hover {
  filter: brightness(1.05);
}

.menu-button {
  border: 1px solid var(--border-light);
  background: var(--bg-control);
  color: var(--text-primary);
  border-radius: 6px;
  padding: 6px 10px;
  cursor: pointer;
  font-size: 12px;
  text-transform: none;
  letter-spacing: 0;
}

.icon-button {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.menu-button:hover {
  background: var(--bg-hover);
}

.menu-button.active {
  border-color: var(--text-accent);
  color: var(--text-primary);
}

:global(.dark) .menu-button.active {
  background: var(--bg-control);
}

.controls-measure {
  position: absolute;
  visibility: hidden;
  pointer-events: none;
  height: 0;
  overflow: hidden;
  display: flex;
  gap: 20px;
}
</style>
