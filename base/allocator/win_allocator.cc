// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple allocator based on the windows heap.

extern "C" {

HANDLE win_heap;

bool win_heap_init(bool use_lfh) {
  win_heap = HeapCreate(0, 0, 0);
  if (win_heap == NULL)
    return false;

  if (use_lfh) {
    ULONG enable_lfh = 2;
    HeapSetInformation(win_heap, HeapCompatibilityInformation,
                       &enable_lfh, sizeof(enable_lfh));
    // NOTE: Setting LFH may fail.  Vista already has it enabled.
    //       And under the debugger, it won't use LFH.  So we
    //       ignore any errors.
  }

  return true;
}

void* win_heap_malloc(size_t size) {
  return HeapAlloc(win_heap, 0, size);
}

void win_heap_free(void* size) {
  HeapFree(win_heap, 0, size);
}

void* win_heap_realloc(void* ptr, size_t size) {
  if (!ptr)
    return win_heap_malloc(size);
  if (!size) {
    win_heap_free(ptr);
    return NULL;
  }
  return HeapReAlloc(win_heap, 0, ptr, size);
}

size_t win_heap_msize(void* ptr) {
  return HeapSize(win_heap, 0, ptr);
}

void* win_heap_memalign(size_t alignment, size_t size) {
  // Reserve enough space to ensure we can align and set aligned_ptr[-1] to the
  // original allocation for use with win_heap_memalign_free() later.
  size_t allocation_size = size + (alignment - 1) + sizeof(void*);

  // Check for overflow.  Alignment and size are checked in allocator_shim.
  DCHECK_LT(size, allocation_size);
  DCHECK_LT(alignment, allocation_size);

  void* ptr = win_heap_malloc(allocation_size);
  char* aligned_ptr = static_cast<char*>(ptr) + sizeof(void*);
  aligned_ptr +=
      alignment - reinterpret_cast<uintptr_t>(aligned_ptr) & (alignment - 1);

  reinterpret_cast<void**>(aligned_ptr)[-1] = ptr;
  return aligned_ptr;
}

void win_heap_memalign_free(void* ptr) {
  if (ptr)
    win_heap_free(static_cast<void**>(ptr)[-1]);
}

}  // extern "C"
