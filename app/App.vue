<script setup lang="ts">
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue';
import EngineTable from './EngineTable.vue';
import EngineTableControls from './EngineTableControls.vue';
import { createInitialState, initColumnOrder, initVisibleColumns } from './state';
import { ALL_COLUMNS } from './columns';
import enginesData from '../engines.json';
import MarkdownModal from './MarkdownModal.vue';

const theme = ref<'light' | 'dark'>('light');
const state = reactive(createInitialState());
const selectedEngineId = ref<string | null>(null);

initVisibleColumns(state, ALL_COLUMNS);
initColumnOrder(state, ALL_COLUMNS);

const engineMap = computed(() => {
  const map = new Map<string, Record<string, unknown>>();
  for (const row of enginesData as Record<string, unknown>[]) {
    if (typeof row.id === 'string') {
      map.set(row.id, row);
    }
  }
  return map;
});

const selectedEngine = computed(() => {
  if (!selectedEngineId.value) {
    return null;
  }
  return engineMap.value.get(selectedEngineId.value) ?? null;
});

function withBase(path: string): string {
  const base = import.meta.env.BASE_URL || '/';
  if (base === '/') {
    return path;
  }
  return base.replace(/\/$/, '') + path;
}

function syncFromLocation() {
  const rawHash = window.location.hash || '';
  const hash = rawHash.replace(/^#/, '').trim();
  if (hash && engineMap.value.has(hash)) {
    selectedEngineId.value = hash;
  } else {
    selectedEngineId.value = null;
  }
}

function applyTheme(next: 'light' | 'dark') {
  theme.value = next;
  document.documentElement.classList.toggle('dark', next === 'dark');
  localStorage.setItem('theme', next);
}

function toggleTheme() {
  applyTheme(theme.value === 'dark' ? 'light' : 'dark');
}

function openEngine(id: string) {
  if (!engineMap.value.has(id)) {
    return;
  }
  selectedEngineId.value = id;
  const search = window.location.search;
  window.history.pushState(null, '', `${withBase('/')}${search}#${id}`);
}

function closeEngine() {
  selectedEngineId.value = null;
  const search = window.location.search;
  window.history.pushState(null, '', `${withBase('/')}${search}`);
}

onMounted(() => {
  const stored = localStorage.getItem('theme');
  if (stored === 'light' || stored === 'dark') {
    applyTheme(stored);
  } else {
    const prefersDark = window.matchMedia?.('(prefers-color-scheme: dark)').matches;
    applyTheme(prefersDark ? 'dark' : 'light');
  }

  window.addEventListener('popstate', syncFromLocation);
  window.addEventListener('hashchange', syncFromLocation);
  syncFromLocation();
});

onUnmounted(() => {
  window.removeEventListener('popstate', syncFromLocation);
  window.removeEventListener('hashchange', syncFromLocation);
});
</script>

<template>
  <main class="app">
    <header class="app-header">
      <div class="title-row">
        <a
          class="app-title-link"
          href="https://github.com/ivankra/javascript-zoo"
          target="_blank"
          rel="noreferrer"
        >
          JavaScript engines zoo
        </a>
        <div class="header-actions">
          <EngineTableControls :state="state" :theme="theme" :toggle-theme="toggleTheme" :show-theme="true" />
        </div>
      </div>
    </header>
    <section class="app-body" :class="{ hidden: selectedEngine }">
      <EngineTable :state="state" :show-controls="false" @select-engine="openEngine" />
    </section>
    <MarkdownModal
      v-if="selectedEngine"
      :engine="selectedEngine"
      @close="closeEngine"
      @open-engine="openEngine"
    />
  </main>
</template>

<style scoped>
.app {
  background: var(--bg-primary);
  color: var(--text-primary);
  min-height: 100vh;
}

.app-header {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  width: 100%;
  margin: 0;
  padding: 0;
  height: var(--app-header-height);
  display: flex;
  align-items: center;
  border-bottom: 1px solid var(--border-light);
  background: var(--bg-header);
  z-index: 10;
  box-sizing: border-box;
}

.app-body {
  padding-top: var(--app-header-height);
}


.title-row {
  display: flex;
  align-items: center;
  justify-content: flex-start;
  gap: 12px;
  width: 100%;
  max-width: 100%;
  box-sizing: border-box;
  padding: 0 20px;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
  margin-left: auto;
  flex: 1 1 auto;
  justify-content: flex-end;
}

.app-title-link {
  margin: 0;
  color: var(--text-accent);
  font-size: 16px;
  line-height: 1.2;
  letter-spacing: 0.02em;
  font-weight: 600;
  font-family: Inter, ui-sans-serif, system-ui, sans-serif, 'Apple Color Emoji', 'Segoe UI Emoji',
    'Segoe UI Symbol', 'Noto Color Emoji';
  text-decoration: none;
}

.app-title-link:hover {
  text-decoration: underline;
}

.app-body.hidden {
  display: none;
}

@media (min-width: 721px) {
  .app-body.hidden {
    display: block;
  }
}

</style>
