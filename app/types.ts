export type SortDirection = 'asc' | 'desc';

export interface SortSpec {
  col: string;
  dir: SortDirection;
}

export interface TableState {
  arch: string;
  variants: boolean;
  jitless: boolean;
  search: string;
  sort: SortSpec[];
  selected: Record<string, boolean>;
}

export interface BenchRow {
  arch?: string;
  binary_size?: number;
  dist_size?: number;
  revision?: string;
  revision_date?: string;
  [key: string]: unknown;
}

export interface ConformanceData {
  [key: string]: number;
}

export interface EngineEntry {
  id: string;
  title?: string;
  engine?: string;
  summary?: string;
  jsz_url?: string;
  homepage?: string;
  repository?: string;
  branch?: string;
  version?: string;
  revision?: string;
  revision_date?: string;
  variant?: string;
  language?: string;
  license?: string;
  license_abbr?: string;
  standard?: string;
  years?: string;
  type?: string;
  platform?: string;
  jit?: string;
  loc?: number;
  loc_detailed?: string;
  github?: string;
  github_stars?: number;
  github_forks?: number;
  github_contributors?: number;
  org?: string;
  tech?: string;
  note?: string;
  bench?: BenchRow[];
  conformance?: ConformanceData;
  [key: string]: unknown;
}

export interface TableRow extends EngineEntry {
  score?: number;
  binary_size?: number;
  dist_size?: number;
}

export interface ColumnDef {
  key: string;
  label: string;
  title?: string;
  numeric?: boolean;
  className?: string;
  benchmark?: boolean;
  v8?: boolean;
}

export interface CellContent {
  text?: string;
  html?: string;
  title?: string;
  className?: string;
}
