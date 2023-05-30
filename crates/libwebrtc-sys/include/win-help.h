#pragma once

#include <Windows.h>
#include <Psapi.h>
#include <stdio.h>
#include <iostream>
#include "system_audio_module.h"
#include <vector>

#define UNUSED_PARAMETER(param) (void)param

enum window_search_mode {
	INCLUDE_MINIMIZED,
	EXCLUDE_MINIMIZED,
};

inline long os_atomic_inc_long(volatile long *val);
size_t wchar_to_utf8(const wchar_t *in, size_t insize, char *out,
		     size_t outsize, int flags);

struct dstr {
	char *array;
	size_t len; /* number of characters, excluding null terminator */
	size_t capacity;
};

inline void dstr_resize(dstr& dst, const size_t num);
inline void dstr_free(dstr& dst);
void dstr_copy(dstr& dst, const char *array);
inline void dstr_ensure_capacity(dstr& dst, const size_t new_size);
inline int dstr_cmp(const dstr& str1, const char *str2);
inline bool dstr_is_empty(const dstr& str);

void bfree(void *ptr);
void *brealloc(void *ptr, size_t size);

#define LOWER_HALFBYTE(x) ((x)&0xF)
#define UPPER_HALFBYTE(x) (((x) >> 4) & 0xF)

std::vector<AudioSourceInfo> ms_fill_window_list(enum window_search_mode mode);
HWND first_window(enum window_search_mode mode, HWND *parent,
			 bool *use_findwindowex);
bool check_window_valid(HWND window, enum window_search_mode mode);
inline bool IsWindowCloaked(HWND window);
bool ms_is_uwp_window(HWND hwnd);
HWND next_window(HWND window, enum window_search_mode mode, HWND *parent,
			bool use_findwindowex);

bool ms_get_window_exe(dstr& exe, HWND window);
inline HANDLE open_process(DWORD desired_access, bool inherit_handle,
				  DWORD process_id);
void *ms_get_obfuscated_func(HMODULE module, const char *str, uint64_t val);
void deobfuscate_str(char *str, uint64_t val);
bool is_microsoft_internal_window_exe(const char *exe);
void ms_get_window_title(dstr& name, HWND hwnd);

int astrcmpi(const char *str1, const char *str2);
int astrcmpi_n(const char *str1, const char *str2, size_t n);
void *a_realloc(void *ptr, size_t size);
