const prefix = 'mychat.web.';

export function loadJson<T>(key: string, fallback: T): T {
  try {
    const raw = window.localStorage.getItem(prefix + key);
    if (!raw) return fallback;
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function saveJson<T>(key: string, value: T): void {
  window.localStorage.setItem(prefix + key, JSON.stringify(value));
}

export function removeItem(key: string): void {
  window.localStorage.removeItem(prefix + key);
}

export function loadSessionJson<T>(key: string, fallback: T): T {
  try {
    const raw = window.sessionStorage.getItem(prefix + key)
      ?? window.localStorage.getItem(prefix + key);
    if (!raw) return fallback;
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function loadSessionOnlyJson<T>(key: string, fallback: T): T {
  try {
    const raw = window.sessionStorage.getItem(prefix + key);
    if (!raw) return fallback;
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function saveSessionJson<T>(key: string, value: T): void {
  window.sessionStorage.setItem(prefix + key, JSON.stringify(value));
  window.localStorage.removeItem(prefix + key);
}

export function removeSessionItem(key: string): void {
  window.sessionStorage.removeItem(prefix + key);
  window.localStorage.removeItem(prefix + key);
}

export function makeId(prefixValue: string): string {
  return `${prefixValue}-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}
