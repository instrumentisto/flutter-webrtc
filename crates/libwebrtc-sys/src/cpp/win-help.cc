#if WEBRTC_WIN

#include "win-help.h"
#include "dwmapi.h"

// log_handler_t log_handler = def_log_handler;
// void (*crash_handler)(const char *, va_list, void *) = def_crash_handler;
const char* astrblank = "";
long num_allocs = 0;
/* not capturable or internal windows, exact executable names */
const char* internal_microsoft_exes_exact[] = {
    "startmenuexperiencehost.exe",
    "applicationframehost.exe",
    "peopleexperiencehost.exe",
    "shellexperiencehost.exe",
    "microsoft.notes.exe",
    "systemsettings.exe",
    "textinputhost.exe",
    "searchapp.exe",
    "video.ui.exe",
    "searchui.exe",
    "lockapp.exe",
    "cortana.exe",
    "gamebar.exe",
    "tabtip.exe",
    "time.exe",
    NULL,
};
/* partial matches start from the beginning of the executable name */
const char* internal_microsoft_exes_partial[] = {
    "windowsinternal",
    NULL,
};

inline long os_atomic_inc_long(volatile long* val) {
  return _InterlockedIncrement(val);
}

void* brealloc(void* ptr, size_t size) {
  if (!ptr)
    os_atomic_inc_long(&num_allocs);

  if (!size) {
    size = 1;
  }

  ptr = a_realloc(ptr, size);

  return ptr;
}

inline void dstr_free(dstr& dst) {
  bfree(dst.array);
  dst.array = NULL;
  dst.len = 0;
  dst.capacity = 0;
}

size_t wchar_to_utf8(const wchar_t* in,
                     size_t insize,
                     char* out,
                     size_t outsize,
                     int flags) {
  int i_insize = (int)insize;
  int ret;

  if (i_insize == 0)
    i_insize = (int)wcslen(in);

  ret = WideCharToMultiByte(CP_UTF8, 0, in, i_insize, out, (int)outsize, NULL,
                            NULL);

  UNUSED_PARAMETER(flags);
  return (ret > 0) ? (size_t)ret : 0;
}

inline void dstr_resize(dstr& dst, const size_t num) {
  if (!num) {
    dstr_free(dst);
    return;
  }

  dstr_ensure_capacity(dst, num + 1);
  dst.array[num] = 0;
  dst.len = num;
}

void dstr_from_wcs(dstr& dst, const wchar_t* wstr) {
  size_t len = wchar_to_utf8(wstr, 0, NULL, 0, 0);

  if (len) {
    dstr_resize(dst, len);
    wchar_to_utf8(wstr, 0, dst.array, len + 1, 0);
  } else {
    dstr_free(dst);
  }
}

inline void dstr_ensure_capacity(dstr& dst, const size_t new_size) {
  size_t new_cap;
  if (new_size <= dst.capacity)
    return;

  new_cap = (!dst.capacity) ? new_size : dst.capacity * 2;
  if (new_size > new_cap)
    new_cap = new_size;
  dst.array = (char*)brealloc(dst.array, new_cap);
  dst.capacity = new_cap;
}

inline bool dstr_is_empty(dstr& str) {
  if (!str.array || !str.len)
    return true;
  if (!str.array)
    return true;

  return false;
}

void dstr_copy(dstr& dst, const char* array) {
  size_t len;

  if (!array || !*array) {
    dstr_free(dst);
    return;
  }

  len = strlen(array);
  dstr_ensure_capacity(dst, len + 1);
  memcpy(dst.array, array, len + 1);
  dst.len = len;
}

int astrcmpi(const char* str1, const char* str2) {
  if (!str1)
    str1 = astrblank;
  if (!str2)
    str2 = astrblank;

  do {
    char ch1 = (char)toupper(*str1);
    char ch2 = (char)toupper(*str2);

    if (ch1 < ch2)
      return -1;
    else if (ch1 > ch2)
      return 1;
  } while (*str1++ && *str2++);

  return 0;
}

int astrcmpi_n(const char* str1, const char* str2, size_t n) {
  if (!n)
    return 0;
  if (!str1)
    str1 = astrblank;
  if (!str2)
    str2 = astrblank;

  do {
    char ch1 = (char)toupper(*str1);
    char ch2 = (char)toupper(*str2);

    if (ch1 < ch2)
      return -1;
    else if (ch1 > ch2)
      return 1;
  } while (*str1++ && *str2++ && --n);

  return 0;
}

inline int dstr_cmp(dstr& str1, const char* str2) {
  return strcmp(str1.array, str2);
}

std::vector<AudioSourceInfo> ms_fill_window_list(enum window_search_mode mode) {
  std::vector<AudioSourceInfo> result;
  HWND parent;
  bool use_findwindowex = false;

  HWND window = first_window(mode, &parent, &use_findwindowex);

  while (window) {
    struct dstr title = {0};
    struct dstr exe = {0};
    if (!ms_get_window_exe(exe, window)) {
      window = next_window(window, mode, &parent, use_findwindowex);
      continue;
    }

    if (is_microsoft_internal_window_exe(exe.array)) {
      dstr_free(exe);
      window = next_window(window, mode, &parent, use_findwindowex);
      continue;
    }

    ms_get_window_title(title, window);
    if (dstr_cmp(exe, "explorer.exe") == 0 && dstr_is_empty(title)) {
      dstr_free(exe);
      dstr_free(title);
      window = next_window(window, mode, &parent, use_findwindowex);
      continue;
    }

    DWORD dwProcessId = 0;
    if (GetWindowThreadProcessId(window, &dwProcessId)) {
      ms_get_window_title(title, window);
      AudioSourceInfo info(dwProcessId, std::string(title.array), dwProcessId);
      result.push_back(info);
    }

    window = next_window(window, mode, &parent, use_findwindowex);
    dstr_free(exe);
    dstr_free(title);
  }

  return result;
}

bool ms_get_window_exe(dstr& name, HWND window) {
  wchar_t wname[MAX_PATH];
  struct dstr temp = {0};
  bool success = false;
  HANDLE process = NULL;
  char* slash;
  DWORD id;

  GetWindowThreadProcessId(window, &id);
  if (id == GetCurrentProcessId())
    return false;

  process = open_process(PROCESS_QUERY_LIMITED_INFORMATION, false, id);
  if (!process)
    goto fail;

  if (!GetProcessImageFileNameW(process, wname, MAX_PATH))
    goto fail;

  dstr_from_wcs(temp, wname);
  slash = strrchr(temp.array, '\\');
  if (!slash)
    goto fail;

  dstr_copy(name, slash + 1);
  success = true;

fail:
  if (!success)
    dstr_copy(name, "unknown");

  dstr_free(temp);
  CloseHandle(process);
  return true;
}

HWND ms_get_uwp_actual_window(HWND parent) {
  DWORD parent_id = 0;
  HWND child;

  GetWindowThreadProcessId(parent, &parent_id);
  child = FindWindowEx(parent, NULL, NULL, NULL);

  while (child) {
    DWORD child_id = 0;
    GetWindowThreadProcessId(child, &child_id);

    if (child_id != parent_id)
      return child;

    child = FindWindowEx(parent, child, NULL, NULL);
  }

  return NULL;
}

HWND first_window(enum window_search_mode mode,
                  HWND* parent,
                  bool* use_findwindowex) {
  HWND window = FindWindowEx(GetDesktopWindow(), NULL, NULL, NULL);

  if (!window) {
    *use_findwindowex = false;
    window = GetWindow(GetDesktopWindow(), GW_CHILD);
  } else {
    *use_findwindowex = true;
  }

  *parent = NULL;

  if (!check_window_valid(window, mode)) {
    window = next_window(window, mode, parent, *use_findwindowex);

    if (!window && *use_findwindowex) {
      *use_findwindowex = false;

      window = GetWindow(GetDesktopWindow(), GW_CHILD);
      if (!check_window_valid(window, mode))
        window = next_window(window, mode, parent, *use_findwindowex);
    }
  }

  if (ms_is_uwp_window(window)) {
    HWND child = ms_get_uwp_actual_window(window);
    if (child) {
      *parent = window;
      return child;
    }
  }

  return window;
}

bool check_window_valid(HWND window, enum window_search_mode mode) {
  DWORD styles, ex_styles;
  RECT rect;

  if (!IsWindowVisible(window) ||
      (mode == EXCLUDE_MINIMIZED &&
       (IsIconic(window) || IsWindowCloaked(window))))
    return false;

  GetClientRect(window, &rect);
  styles = (DWORD)GetWindowLongPtr(window, GWL_STYLE);
  ex_styles = (DWORD)GetWindowLongPtr(window, GWL_EXSTYLE);

  if (ex_styles & WS_EX_TOOLWINDOW)
    return false;
  if (styles & WS_CHILD)
    return false;
  if (mode == EXCLUDE_MINIMIZED && (rect.bottom == 0 || rect.right == 0))
    return false;

  return true;
}

inline bool IsWindowCloaked(HWND window) {
  DWORD cloaked;
  HRESULT hr =
      DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  return SUCCEEDED(hr) && cloaked;
}

bool ms_is_uwp_window(HWND hwnd) {
  wchar_t name[256];

  name[0] = 0;
  if (!GetClassNameW(hwnd, name, sizeof(name) / sizeof(wchar_t)))
    return false;

  return wcscmp(name, L"ApplicationFrameWindow") == 0;
}

HWND next_window(HWND window,
                 enum window_search_mode mode,
                 HWND* parent,
                 bool use_findwindowex) {
  if (*parent) {
    window = *parent;
    *parent = NULL;
  }

  while (true) {
    if (use_findwindowex)
      window = FindWindowEx(GetDesktopWindow(), window, NULL, NULL);
    else
      window = GetNextWindow(window, GW_HWNDNEXT);

    if (!window || check_window_valid(window, mode))
      break;
  }

  if (ms_is_uwp_window(window)) {
    HWND child = ms_get_uwp_actual_window(window);
    if (child) {
      *parent = window;
      return child;
    }
  }

  return window;
}

HMODULE kernel32(void) {
  HMODULE kernel32_handle = NULL;
  if (!kernel32_handle)
    kernel32_handle = GetModuleHandleA("kernel32");
  return kernel32_handle;
}

inline HANDLE open_process(DWORD desired_access,
                           bool inherit_handle,
                           DWORD process_id) {
  typedef HANDLE(WINAPI * PFN_OpenProcess)(DWORD, BOOL, DWORD);
  PFN_OpenProcess open_process_proc = NULL;
  if (!open_process_proc)
    open_process_proc = (PFN_OpenProcess)ms_get_obfuscated_func(
        kernel32(), "B}caZyah`~q", 0x2D5BEBAF6DDULL);

  return open_process_proc(desired_access, inherit_handle, process_id);
}

void* ms_get_obfuscated_func(HMODULE module, const char* str, uint64_t val) {
  char new_name[128];
  strcpy(new_name, str);
  deobfuscate_str(new_name, val);
  return GetProcAddress(module, new_name);
}

void deobfuscate_str(char* str, uint64_t val) {
  uint8_t* dec_val = (uint8_t*)&val;
  int i = 0;

  while (*str != 0) {
    int pos = i / 2;
    bool bottom = (i % 2) == 0;
    uint8_t* ch = (uint8_t*)str;
    uint8_t xor = bottom ? LOWER_HALFBYTE(dec_val[pos])
                         : UPPER_HALFBYTE(dec_val[pos]);

    *ch ^= xor;

    if (++i == sizeof(uint64_t) * 2)
      i = 0;

    str++;
  }
}

bool is_microsoft_internal_window_exe(const char* exe) {
  if (!exe)
    return false;

  for (const char** vals = internal_microsoft_exes_exact; *vals; vals++) {
    if (astrcmpi(exe, *vals) == 0)
      return true;
  }

  for (const char** vals = internal_microsoft_exes_partial; *vals; vals++) {
    if (astrcmpi_n(exe, *vals, strlen(*vals)) == 0)
      return true;
  }

  return false;
}

void ms_get_window_title(dstr& name, HWND hwnd) {
  int len;

  len = GetWindowTextLengthW(hwnd);
  if (!len)
    return;

  if (len > 1024) {
    wchar_t* temp;

    temp = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    if (!temp)
      return;

    if (GetWindowTextW(hwnd, temp, len + 1))
      dstr_from_wcs(name, temp);

    free(temp);
  } else {
    wchar_t temp[1024 + 1];

    if (GetWindowTextW(hwnd, temp, len + 1))
      dstr_from_wcs(name, temp);
  }
}

void* a_realloc(void* ptr, size_t size) {
#ifdef ALIGNED_MALLOC
  return _aligned_realloc(ptr, size, ALIGNMENT);
#elif ALIGNMENT_HACK
  long diff;

  if (!ptr)
    return a_malloc(size);
  diff = ((char*)ptr)[-1];
  ptr = realloc((char*)ptr - diff, size + diff);
  if (ptr)
    ptr = (char*)ptr + diff;
  return ptr;
#else
  return realloc(ptr, size);
#endif
}

inline long os_atomic_dec_long(volatile long* val) {
  return _InterlockedDecrement(val);
}

void a_free(void* ptr) {
#ifdef ALIGNED_MALLOC
  _aligned_free(ptr);
#elif ALIGNMENT_HACK
  if (ptr)
    free((char*)ptr - ((char*)ptr)[-1]);
#else
  free(ptr);
#endif
}

void bfree(void* ptr) {
  if (ptr) {
    os_atomic_dec_long(&num_allocs);
    a_free(ptr);
  }
}
#endif