import type { CellContent, TableRow } from './types';

export function formatLoc(loc?: number): CellContent {
  if (!Number(loc)) {
    return {};
  }
  let size = Number(loc);
  let i = 0;
  const suffix = ['', 'K', 'M'];
  while (size >= 1000 && i < suffix.length - 1) {
    size /= 1000;
    i += 1;
  }
  return {
    text: size.toFixed(size < 20 ? 1 : 0) + suffix[i],
    title: String(loc),
  };
}

export function formatBinarySize(binarySize?: number, distSize?: number): CellContent {
  if (!Number(binarySize)) {
    return {};
  }
  const isDist = Number(binarySize) < 0;
  let size = Math.abs(Number(binarySize));
  let i = 0;
  const suffix = ['', 'K', 'M'];
  while (size >= 1024 && i < suffix.length - 1) {
    size /= 1024;
    i += 1;
  }
  const cell: CellContent = {
    text: size.toFixed(size < 20 ? 1 : 0) + suffix[i],
  };
  if (isDist) {
    cell.title = `Not a single native binary. Distribution size: ${distSize} bytes`;
    cell.className = 'red';
  } else {
    cell.title = `${binarySize} bytes`;
  }
  return cell;
}

export function formatStars(value?: number, repo?: string, github?: string): CellContent {
  if (!Number(value)) {
    return {};
  }
  const text = value >= 1000 ? (value / 1000).toFixed(1) + 'k' : value.toFixed(0);
  return {
    text,
    title: github ?? repo ?? undefined,
  };
}

export function formatLicense(license?: string, licenseAbbr?: string): CellContent {
  if (!license) {
    return {};
  }
  let title: string | undefined;
  if (license === 'UPL-1.0') {
    title = 'Universal Permissive License 1.0';
  } else if (license === 'BSL-1.0') {
    title = 'Boost Software License 1.0';
  } else if (licenseAbbr && licenseAbbr !== license) {
    title = license;
  }

  const display = (licenseAbbr ?? license)
    .split('/')
    .map((part) => `<nobr>${part}</nobr>`)
    .join('/');

  const cell: CellContent = {
    html: display,
    title,
  };

  if (license.match(/(proprietary|custom|missing|agpl)/i)) {
    cell.className = 'red';
  }
  return cell;
}

export function formatStandard(standard?: string): CellContent {
  if (!standard) {
    return {};
  }
  return {
    text: standard.replace(/^no$/, '❌'),
  };
}

export function formatConformance(value: unknown): CellContent {
  if (typeof value === 'number') {
    const text = (value * 100).toFixed(0) + '%';
    return {
      text,
      title: value !== 0 && value !== 1 ? (value * 100).toFixed(2) + '%' : undefined,
    };
  }
  if (value) {
    return { text: String(value) };
  }
  return {};
}

export function formatLanguage(language?: string): CellContent {
  if (!language) {
    return {};
  }
  const html = language.replace(/ \(.*\)/, '<sup>*</sup>').replace(/ +/g, '<br>');
  return {
    html,
    title: html !== language ? language : undefined,
  };
}

export function formatYears(years?: string): CellContent {
  if (!years) {
    return {};
  }
  const html = years.replace(/-/, '-<br>');
  return {
    html,
    className: years.endsWith('-') ? undefined : 'red',
  };
}

export function formatJit(jit?: string): CellContent {
  if (!jit) {
    return {};
  }
  return {
    text: 'Y',
    title: jit,
  };
}

export function formatDescription(row: TableRow): CellContent {
  const parts: string[] = [];
  if (row.summary) {
    parts.push(row.summary);
  }
  if (row.tech) {
    parts.push(`Tech: ${row.tech}`);
  }
  if (row.note) {
    parts.push(row.note);
  }
  return parts.length ? { html: parts.join('<br>') } : {};
}

export function formatBenchmark(value: unknown, detail?: unknown, error?: unknown): CellContent {
  if (typeof value !== 'number' || Number.isNaN(value)) {
    return {};
  }
  const cell: CellContent = {
    text: String(Math.round(value)),
  };

  if (detail) {
    cell.title = String(detail);
  } else if (error) {
    cell.title = String(error);
    cell.className = 'missing';
  }
  return cell;
}
