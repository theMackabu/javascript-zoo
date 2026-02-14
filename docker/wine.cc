// Minimal JScript host for Microsoft/Wine's jscript.dll.
//
// Usage: jscript.exe [--dll jscript.dll] [--version|script.js]
//
// Implements basic REPL and can execute a script from file.
// Provides WScript.Echo/print/console.log methods.

#define CINTERFACE
#define COBJMACROS
#include <activscp.h>
#include <ole2.h>
#include <oleauto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

static void PrintWideUtf8(FILE* out, const WCHAR* s) {
  if (!s) return;
  int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return;
  char* buf = static_cast<char*>(malloc(static_cast<size_t>(n)));
  if (!buf) return;
  if (WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, n, nullptr, nullptr) > 0) fputs(buf, out);
  free(buf);
}

static BSTR VariantToBstr(const VARIANT* v) {
  if (!v) return nullptr;
  VARIANT tmp;
  VariantInit(&tmp);
  if (FAILED(VariantCopyInd(&tmp, const_cast<VARIANT*>(v)))) return nullptr;
  if (FAILED(VariantChangeType(&tmp, &tmp, 0, VT_BSTR))) {
    VariantClear(&tmp);
    return nullptr;
  }
  BSTR out = SysAllocString(V_BSTR(&tmp));
  VariantClear(&tmp);
  return out;
}

static void PrintVariant(const VARIANT* v) {
  if (!v || V_VT(v) == VT_EMPTY || V_VT(v) == VT_NULL) return;
  BSTR text = VariantToBstr(v);
  if (!text) return;
  PrintWideUtf8(stdout, text);
  fputc('\n', stdout);
  SysFreeString(text);
}

static WCHAR* ReadUtf8File(const WCHAR* path) {
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return nullptr;

  DWORD n = GetFileSize(h, nullptr);
  if (n == INVALID_FILE_SIZE) {
    CloseHandle(h);
    return nullptr;
  }

  char* bytes = static_cast<char*>(malloc(static_cast<size_t>(n) + 1));
  if (!bytes) {
    CloseHandle(h);
    return nullptr;
  }

  DWORD got = 0;
  BOOL ok = ReadFile(h, bytes, n, &got, nullptr);
  CloseHandle(h);
  if (!ok || got != n) {
    free(bytes);
    return nullptr;
  }
  bytes[n] = 0;

  const char* data = bytes;
  if (n >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
      static_cast<unsigned char>(bytes[1]) == 0xBB &&
      static_cast<unsigned char>(bytes[2]) == 0xBF) {
    data += 3;
    n -= 3;
  }

  int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(n), nullptr, 0);
  if (wlen <= 0) {
    free(bytes);
    return nullptr;
  }
  WCHAR* out = static_cast<WCHAR*>(calloc(static_cast<size_t>(wlen) + 1, sizeof(WCHAR)));
  if (!out) {
    free(bytes);
    return nullptr;
  }
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(n), out, wlen) != wlen) {
    free(bytes);
    free(out);
    return nullptr;
  }
  free(bytes);
  return out;
}

// Host: COM object exposed to scripts as `WScript`.
// It implements IDispatch and only provides one method: Echo(...).

struct Host {
  IDispatch iface;
  LONG refs;
};

static Host* HostFromIface(IDispatch* iface) {
  return CONTAINING_RECORD(iface, Host, iface);
}

static HRESULT WINAPI HostQueryInterface(IDispatch* iface, REFIID riid, void** out) {
  Host* self = HostFromIface(iface);
  if (!out) return E_POINTER;
  *out = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDispatch)) {
    *out = &self->iface;
    IDispatch_AddRef(&self->iface);
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI HostAddRef(IDispatch* iface) {
  return InterlockedIncrement(&HostFromIface(iface)->refs);
}

static ULONG WINAPI HostRelease(IDispatch* iface) {
  Host* self = HostFromIface(iface);
  ULONG refs = InterlockedDecrement(&self->refs);
  if (!refs) free(self);
  return refs;
}

static HRESULT WINAPI HostGetTypeInfoCount(IDispatch*, UINT* count) {
  if (!count) return E_POINTER;
  *count = 0;
  return S_OK;
}

static HRESULT WINAPI HostGetTypeInfo(IDispatch*, UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}

static HRESULT WINAPI HostGetIDsOfNames(IDispatch*, REFIID, LPOLESTR* names, UINT count, LCID, DISPID* dispids) {
  if (!names || !dispids || count == 0) return E_INVALIDARG;
  if (lstrcmpiW(names[0], L"Echo") == 0) {
    dispids[0] = 1;
    return S_OK;
  }
  return DISP_E_UNKNOWNNAME;
}

static HRESULT WINAPI HostInvoke(IDispatch*, DISPID dispid, REFIID, LCID, WORD flags, DISPPARAMS* params, VARIANT* result, EXCEPINFO*, UINT*) {
  if (result) VariantInit(result);
  if (!(flags & DISPATCH_METHOD) || dispid != 1) return DISP_E_MEMBERNOTFOUND;
  if (params) {
    for (UINT i = params->cArgs; i > 0; --i) {
      BSTR text = VariantToBstr(&params->rgvarg[i - 1]);
      if (!text) continue;
      if (i != params->cArgs) fputc(' ', stdout);
      PrintWideUtf8(stdout, text);
      SysFreeString(text);
    }
  }
  fputc('\n', stdout);
  return S_OK;
}

static IDispatchVtbl kHostVtable = {HostQueryInterface, HostAddRef, HostRelease, HostGetTypeInfoCount, HostGetTypeInfo, HostGetIDsOfNames, HostInvoke};

static Host* CreateHost() {
  Host* h = static_cast<Host*>(calloc(1, sizeof(*h)));
  if (!h) return nullptr;
  h->iface.lpVtbl = &kHostVtable;
  h->refs = 1;
  return h;
}

// Site: host callback sink used by the engine.
// Implements IActiveScriptSite, hands back our Host object from GetItemInfo.

struct Site {
  IActiveScriptSite iface;
  LONG refs;
  BOOL suppress_errors;
  Host* wscript;
};

static Site* SiteFromIface(IActiveScriptSite* iface) {
  return CONTAINING_RECORD(iface, Site, iface);
}

static HRESULT WINAPI SiteQueryInterface(IActiveScriptSite* iface, REFIID riid, void** out) {
  Site* self = SiteFromIface(iface);
  if (!out) return E_POINTER;
  *out = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IActiveScriptSite)) {
    *out = &self->iface;
    IActiveScriptSite_AddRef(&self->iface);
    return S_OK;
  }
  return E_NOINTERFACE;
}

static ULONG WINAPI SiteAddRef(IActiveScriptSite* iface) {
  return InterlockedIncrement(&SiteFromIface(iface)->refs);
}

static ULONG WINAPI SiteRelease(IActiveScriptSite* iface) {
  Site* self = SiteFromIface(iface);
  ULONG refs = InterlockedDecrement(&self->refs);
  if (!refs) {
    if (self->wscript) IDispatch_Release(&self->wscript->iface);
    free(self);
  }
  return refs;
}

static HRESULT WINAPI SiteGetLCID(IActiveScriptSite*, LCID* out_lcid) {
  if (!out_lcid) return E_POINTER;
  *out_lcid = LOCALE_SYSTEM_DEFAULT;
  return S_OK;
}

static HRESULT WINAPI SiteGetItemInfo(IActiveScriptSite* iface, LPCOLESTR name, DWORD mask, IUnknown** out_item, ITypeInfo** out_typeinfo) {
  Site* self = SiteFromIface(iface);
  if (out_item) *out_item = nullptr;
  if (out_typeinfo) *out_typeinfo = nullptr;

  if (name && lstrcmpiW(name, L"WScript") == 0) {
    if ((mask & SCRIPTINFO_IUNKNOWN) && out_item) {
      *out_item = reinterpret_cast<IUnknown*>(&self->wscript->iface);
      IUnknown_AddRef(*out_item);
    }
    if (mask & SCRIPTINFO_ITYPEINFO) return TYPE_E_ELEMENTNOTFOUND;
    return S_OK;
  }
  return TYPE_E_ELEMENTNOTFOUND;
}

static HRESULT WINAPI SiteGetDocVersionString(IActiveScriptSite*, BSTR* out) {
  if (!out) return E_POINTER;
  *out = SysAllocString(L"1");
  return *out ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI SiteOnScriptTerminate(IActiveScriptSite*, const VARIANT*, const EXCEPINFO*) {
  return S_OK;
}

static HRESULT WINAPI SiteOnStateChange(IActiveScriptSite*, SCRIPTSTATE) {
  return S_OK;
}

static HRESULT WINAPI SiteOnScriptError(IActiveScriptSite* iface, IActiveScriptError* err) {
  Site* self = SiteFromIface(iface);
  if (self->suppress_errors || !err) return S_OK;

  EXCEPINFO ex = {};
  DWORD ctx = 0;
  ULONG line = 0;
  LONG col = 0;
  IActiveScriptError_GetSourcePosition(err, &ctx, &line, &col);
  IActiveScriptError_GetExceptionInfo(err, &ex);
  fprintf(stderr, "error:%lu:%ld: ", static_cast<unsigned long>(line + 1), col + 1);
  if (ex.bstrDescription) PrintWideUtf8(stderr, ex.bstrDescription);
  fputc('\n', stderr);
  SysFreeString(ex.bstrSource);
  SysFreeString(ex.bstrDescription);
  SysFreeString(ex.bstrHelpFile);
  return S_OK;
}

static HRESULT WINAPI SiteOnEnterScript(IActiveScriptSite*) { return S_OK; }
static HRESULT WINAPI SiteOnLeaveScript(IActiveScriptSite*) { return S_OK; }

static IActiveScriptSiteVtbl kSiteVtable = {
    SiteQueryInterface,    SiteAddRef,        SiteRelease,
    SiteGetLCID,           SiteGetItemInfo,   SiteGetDocVersionString,
    SiteOnScriptTerminate, SiteOnStateChange, SiteOnScriptError,
    SiteOnEnterScript,     SiteOnLeaveScript
};

static Site* CreateSite() {
  Site* s = static_cast<Site*>(calloc(1, sizeof(*s)));
  if (!s) return nullptr;
  s->iface.lpVtbl = &kSiteVtable;
  s->refs = 1;
  s->wscript = CreateHost();
  if (!s->wscript) {
    free(s);
    return nullptr;
  }
  return s;
}

struct Engine {
  HMODULE module;
  IActiveScript* script;
#ifdef _WIN64
  IActiveScriptParse64* parse;
#else
  IActiveScriptParse32* parse;
#endif
  Site* site;
};

static HRESULT EngineExec(Engine* e, const WCHAR* code, DWORD flags, VARIANT* out, BOOL suppress_errors) {
  if (!e || !e->parse) return E_UNEXPECTED;
  EXCEPINFO ex = {};
  if (out) VariantInit(out);
  e->site->suppress_errors = suppress_errors;
#ifdef _WIN64
  HRESULT hr = IActiveScriptParse64_ParseScriptText(e->parse, code, nullptr, nullptr, nullptr, 0, 0, flags, out, &ex);
#else
  HRESULT hr = IActiveScriptParse32_ParseScriptText(e->parse, code, nullptr, nullptr, nullptr, 0, 0, flags, out, &ex);
#endif
  e->site->suppress_errors = FALSE;
  SysFreeString(ex.bstrSource);
  SysFreeString(ex.bstrDescription);
  SysFreeString(ex.bstrHelpFile);
  return hr;
}

static void EngineDestroy(Engine* e) {
  if (!e) return;
  if (e->script) IActiveScript_Close(e->script);
  if (e->parse) {
#ifdef _WIN64
    IActiveScriptParse64_Release(e->parse);
#else
    IActiveScriptParse32_Release(e->parse);
#endif
  }
  if (e->script) IActiveScript_Release(e->script);
  if (e->site) IActiveScriptSite_Release(&e->site->iface);
  if (e->module) FreeLibrary(e->module);
  memset(e, 0, sizeof(*e));
}

static HRESULT EngineInit(Engine* e, const WCHAR* dll_path) {
#ifdef _WIN64
  constexpr GUID kIidIActiveScriptParse = {0xc7ef7658, 0xe1ee, 0x480e, {0x97, 0xea, 0xd5, 0x2c, 0xb4, 0xd7, 0x6d, 0x17}};
#else
  constexpr GUID kIidIActiveScriptParse = {0xbb1a2ae2, 0xa4f9, 0x11cf, {0x8f, 0x20, 0x00, 0x80, 0x5f, 0x2c, 0xd0, 0x64}};
#endif
  memset(e, 0, sizeof(*e));
  e->module = LoadLibraryW((dll_path && *dll_path) ? dll_path : L"jscript.dll");
  if (!e->module) return HRESULT_FROM_WIN32(GetLastError());

  using DllGetClassObjectFn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
  auto get_class = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(e->module, "DllGetClassObject"));
  if (!get_class) return E_NOINTERFACE;

  IClassFactory* factory = nullptr;
  constexpr GUID kClsidJScript = {0xf414c260, 0x6ac0, 0x11cf, {0xb6, 0xd1, 0x00, 0xaa, 0x00, 0xbb, 0xbb, 0x58}};
  HRESULT hr = get_class(kClsidJScript, IID_IClassFactory, reinterpret_cast<void**>(&factory));
  if (FAILED(hr)) goto fail;

  hr = IClassFactory_CreateInstance(factory, nullptr, IID_IActiveScript, reinterpret_cast<void**>(&e->script));
  IClassFactory_Release(factory);
  if (FAILED(hr)) goto fail;

  hr = IActiveScript_QueryInterface(e->script, kIidIActiveScriptParse, reinterpret_cast<void**>(&e->parse));
  if (FAILED(hr)) goto fail;

  e->site = CreateSite();
  if (!e->site) {
    hr = E_OUTOFMEMORY;
    goto fail;
  }

  hr = IActiveScript_SetScriptSite(e->script, &e->site->iface);
  if (FAILED(hr)) goto fail;
  #ifdef _WIN64
  hr = IActiveScriptParse64_InitNew(e->parse);
  #else
  hr = IActiveScriptParse32_InitNew(e->parse);
  #endif
  if (FAILED(hr)) goto fail;
  // Active Scripting exposes native hooks through named COM items.
  hr = IActiveScript_AddNamedItem(e->script, L"WScript", SCRIPTITEM_ISVISIBLE);
  if (FAILED(hr)) goto fail;
  hr = IActiveScript_SetScriptState(e->script, SCRIPTSTATE_STARTED);
  if (FAILED(hr)) goto fail;

  static const WCHAR code[] = L"function print(){WScript.Echo(Array.prototype.join.call(arguments,' '));}; var console={log:print};";
  hr = EngineExec(e, code, SCRIPTTEXT_ISVISIBLE, nullptr, FALSE);
  if (FAILED(hr)) goto fail;
  return S_OK;

fail:
  EngineDestroy(e);
  return hr;
}

static int RunScript(Engine* e, const WCHAR* path) {
  WCHAR* code = ReadUtf8File(path);
  if (!code) {
    fputs("Failed to read file: ", stderr);
    PrintWideUtf8(stderr, path);
    fputc('\n', stderr);
    return 1;
  }
  HRESULT hr = EngineExec(e, code, SCRIPTTEXT_ISVISIBLE, nullptr, FALSE);
  free(code);
  return FAILED(hr) ? 1 : 0;
}

static int RunRepl(Engine* e) {
  WCHAR line[8192];
  while (1) {
    fputs("> ", stdout);
    fflush(stdout);
    if (!fgetws(line, static_cast<int>(sizeof(line) / sizeof(line[0])), stdin)) break;
    size_t len = wcscspn(line, L"\r\n");
    line[len] = 0;
    if (line[0] == 0x04 /*Unix Ctrl-D*/ && line[1] == 0) break;
    if (!line[0]) continue;
    if (lstrcmpiW(line, L"exit") == 0 || lstrcmpiW(line, L"quit") == 0) break;

    VARIANT v;
    HRESULT hr = EngineExec(e, line, SCRIPTTEXT_ISVISIBLE | SCRIPTTEXT_ISEXPRESSION, &v, TRUE);
    if (SUCCEEDED(hr)) {
      PrintVariant(&v);
      VariantClear(&v);
      continue;
    }
    EngineExec(e, line, SCRIPTTEXT_ISVISIBLE, nullptr, FALSE);
  }
  return 0;
}

int wmain(int argc, WCHAR** argv) {
  const WCHAR* dll_path = L"jscript.dll";
  const WCHAR* script = nullptr;
  BOOL show_version = FALSE;

  for (int i = 1; i < argc; ++i) {
    if (lstrcmpiW(argv[i], L"--help") == 0 || lstrcmpiW(argv[i], L"-h") == 0) {
      puts("Usage: jscript.exe [--dll jscript.dll] [--version|script.js]\n");
      return 0;
    } else if (lstrcmpiW(argv[i], L"--dll") == 0 && i + 1 < argc) {
      dll_path = argv[++i];
    } else if (lstrcmpiW(argv[i], L"--version") == 0) {
      show_version = TRUE;
    } else if (!script) {
      script = argv[i];
    } else {
      fputs("Error: only one script file is supported\n", stderr);
      return 1;
    }
  }

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    fprintf(stderr, "CoInitializeEx failed: 0x%08lx\n", static_cast<unsigned long>(hr));
    return 1;
  }

  Engine e;
  hr = EngineInit(&e, dll_path);
  if (FAILED(hr)) {
    fprintf(stderr, "EngineInit failed: 0x%08lx\n", static_cast<unsigned long>(hr));
    CoUninitialize();
    return 1;
  }

  int rc = 0;
  if (show_version) {
    static const WCHAR code[] = L"print(ScriptEngineMajorVersion()+'.'+ScriptEngineMinorVersion()+'.'+ScriptEngineBuildVersion())";
    EngineExec(&e, code, SCRIPTTEXT_ISVISIBLE, nullptr, FALSE);
  } else if (script) {
    rc = RunScript(&e, script);
  } else {
    rc = RunRepl(&e);
  }

  EngineDestroy(&e);
  CoUninitialize();
  return rc;
}
