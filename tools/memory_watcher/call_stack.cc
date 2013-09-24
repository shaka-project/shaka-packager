// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory_watcher/call_stack.h"

#include <shlwapi.h>
#include <tlhelp32.h>

#include "base/strings/string_number_conversions.h"
#include "tools/memory_watcher/memory_hook.h"

// Typedefs for explicit dynamic linking with functions exported from
// dbghelp.dll.
typedef BOOL (__stdcall *t_StackWalk64)(DWORD, HANDLE, HANDLE,
                                        LPSTACKFRAME64, PVOID,
                                        PREAD_PROCESS_MEMORY_ROUTINE64,
                                        PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                        PGET_MODULE_BASE_ROUTINE64,
                                        PTRANSLATE_ADDRESS_ROUTINE64);
typedef PVOID (__stdcall *t_SymFunctionTableAccess64)(HANDLE, DWORD64);
typedef DWORD64 (__stdcall *t_SymGetModuleBase64)(HANDLE, DWORD64);
typedef BOOL (__stdcall *t_SymCleanup)(HANDLE);
typedef BOOL (__stdcall *t_SymGetSymFromAddr64)(HANDLE, DWORD64,
                                               PDWORD64, PIMAGEHLP_SYMBOL64);
typedef BOOL (__stdcall *t_SymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD,
                                                 PIMAGEHLP_LINE64);
typedef BOOL (__stdcall *t_SymInitialize)(HANDLE, PCTSTR, BOOL);
typedef DWORD (__stdcall *t_SymGetOptions)(void);
typedef DWORD (__stdcall *t_SymSetOptions)(DWORD);
typedef BOOL (__stdcall *t_SymGetSearchPath)(HANDLE, PTSTR, DWORD);
typedef DWORD64 (__stdcall *t_SymLoadModule64)(HANDLE, HANDLE, PCSTR,
                                               PCSTR, DWORD64, DWORD);
typedef BOOL (__stdcall *t_SymGetModuleInfo64)(HANDLE, DWORD64,
                                               PIMAGEHLP_MODULE64);

// static
base::Lock CallStack::dbghelp_lock_;
// static
bool CallStack::dbghelp_loaded_ = false;
// static
DWORD CallStack::active_thread_id_ = 0;


static t_StackWalk64 pStackWalk64 = NULL;
static t_SymCleanup pSymCleanup = NULL;
static t_SymGetSymFromAddr64 pSymGetSymFromAddr64 = NULL;
static t_SymFunctionTableAccess64 pSymFunctionTableAccess64 = NULL;
static t_SymGetModuleBase64 pSymGetModuleBase64 = NULL;
static t_SymGetLineFromAddr64 pSymGetLineFromAddr64 = NULL;
static t_SymInitialize pSymInitialize = NULL;
static t_SymGetOptions pSymGetOptions = NULL;
static t_SymSetOptions pSymSetOptions = NULL;
static t_SymGetModuleInfo64 pSymGetModuleInfo64 = NULL;
static t_SymGetSearchPath pSymGetSearchPath = NULL;
static t_SymLoadModule64 pSymLoadModule64 = NULL;

#define LOADPROC(module, name)  do {                                    \
  p##name = reinterpret_cast<t_##name>(GetProcAddress(module, #name));  \
  if (p##name == NULL) return false;                                    \
} while (0)

// This code has to be VERY careful to not induce any allocations, as memory
// watching code may cause recursion, which may obscure the stack for the truly
// offensive issue.  We use this function to break into a debugger, and it
// is guaranteed to not do any allocations (in fact, not do anything).
static void UltraSafeDebugBreak() {
  _asm int(3);
}

// static
bool CallStack::LoadDbgHelp() {
  if (!dbghelp_loaded_) {
    base::AutoLock Lock(dbghelp_lock_);

    // Re-check if we've loaded successfully now that we have the lock.
    if (dbghelp_loaded_)
      return true;

    // Load dbghelp.dll, and obtain pointers to the exported functions that we
    // will be using.
    HMODULE dbghelp_module = LoadLibrary(L"dbghelp.dll");
    if (dbghelp_module) {
      LOADPROC(dbghelp_module, StackWalk64);
      LOADPROC(dbghelp_module, SymFunctionTableAccess64);
      LOADPROC(dbghelp_module, SymGetModuleBase64);
      LOADPROC(dbghelp_module, SymCleanup);
      LOADPROC(dbghelp_module, SymGetSymFromAddr64);
      LOADPROC(dbghelp_module, SymGetLineFromAddr64);
      LOADPROC(dbghelp_module, SymInitialize);
      LOADPROC(dbghelp_module, SymGetOptions);
      LOADPROC(dbghelp_module, SymSetOptions);
      LOADPROC(dbghelp_module, SymGetModuleInfo64);
      LOADPROC(dbghelp_module, SymGetSearchPath);
      LOADPROC(dbghelp_module, SymLoadModule64);
      dbghelp_loaded_ = true;
    } else {
      UltraSafeDebugBreak();
      return false;
    }
  }
  return dbghelp_loaded_;
}

// Load the symbols for generating stack traces.
static bool LoadSymbols(HANDLE process_handle) {
  static bool symbols_loaded = false;
  if (symbols_loaded) return true;

  BOOL ok;

  // Initialize the symbol engine.
  ok = pSymInitialize(process_handle,  /* hProcess */
                      NULL,            /* UserSearchPath */
                      FALSE);          /* fInvadeProcess */
  if (!ok) return false;

  DWORD options = pSymGetOptions();
  options |= SYMOPT_LOAD_LINES;
  options |= SYMOPT_FAIL_CRITICAL_ERRORS;
  options |= SYMOPT_UNDNAME;
  options = pSymSetOptions(options);

  const DWORD kMaxSearchPath = 1024;
  TCHAR buf[kMaxSearchPath] = {0};
  ok = pSymGetSearchPath(process_handle, buf, kMaxSearchPath);
  if (!ok)
    return false;

  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
                                             GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE)
    return false;

  MODULEENTRY32W module;
  module.dwSize = sizeof(module);  // Set the size of the structure.
  BOOL cont = Module32FirstW(snapshot, &module);
  while (cont) {
    DWORD64 base;
    // NOTE the SymLoadModule64 function has the peculiarity of accepting a
    // both unicode and ASCII strings even though the parameter is PSTR.
    base = pSymLoadModule64(process_handle,
                            0,
                            reinterpret_cast<PSTR>(module.szExePath),
                            reinterpret_cast<PSTR>(module.szModule),
                            reinterpret_cast<DWORD64>(module.modBaseAddr),
                            module.modBaseSize);
    if (base == 0) {
      int err = GetLastError();
      if (err != ERROR_MOD_NOT_FOUND && err != ERROR_INVALID_HANDLE)
          return false;
    }
    cont = Module32NextW(snapshot, &module);
  }
  CloseHandle(snapshot);

  symbols_loaded = true;
  return true;
}


CallStack::SymbolCache* CallStack::symbol_cache_;

bool CallStack::Initialize() {
  // We need to delay load the symbol cache until after
  // the MemoryHook heap is alive.
  symbol_cache_ = new SymbolCache();
  return LoadDbgHelp();
}

CallStack::CallStack() {
  static LONG callstack_id = 0;
  frame_count_ = 0;
  hash_ = 0;
  id_ = InterlockedIncrement(&callstack_id);
  valid_ = false;

  if (!dbghelp_loaded_) {
    UltraSafeDebugBreak();  // Initialize should have been called.
    return;
  }

  GetStackTrace();
}

bool CallStack::IsEqual(const CallStack &target) {
  if (frame_count_ != target.frame_count_)
    return false;  // They can't be equal if the sizes are different.

  // Walk the frames array until we
  // either find a mismatch, or until we reach the end of the call stacks.
  for (int index = 0; index < frame_count_; index++) {
    if (frames_[index] != target.frames_[index])
      return false;  // Found a mismatch. They are not equal.
  }

  // Reached the end of the call stacks. They are equal.
  return true;
}

void CallStack::AddFrame(DWORD_PTR pc) {
  DCHECK(frame_count_ < kMaxTraceFrames);
  frames_[frame_count_++] = pc;

  // Create a unique id for this CallStack.
  pc = pc + (frame_count_ * 13);  // Alter the PC based on position in stack.
  hash_ = ~hash_ + (pc << 15);
  hash_ = hash_ ^ (pc >> 12);
  hash_ = hash_ + (pc << 2);
  hash_ = hash_ ^ (pc >> 4);
  hash_ = hash_ * 2057;
  hash_ = hash_ ^ (pc >> 16);
}

bool CallStack::LockedRecursionDetected() const {
  if (!active_thread_id_) return false;
  DWORD thread_id = GetCurrentThreadId();
  // TODO(jar): Perchance we should use atomic access to member.
  return thread_id == active_thread_id_;
}

bool CallStack::GetStackTrace() {
  if (LockedRecursionDetected())
    return false;

  // Initialize the context record.
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_FULL;
  __asm    call x
  __asm x: pop eax
  __asm    mov context.Eip, eax
  __asm    mov context.Ebp, ebp
  __asm    mov context.Esp, esp

  STACKFRAME64 frame;
  memset(&frame, 0, sizeof(frame));

#ifdef _M_IX86
  DWORD image_type = IMAGE_FILE_MACHINE_I386;
  frame.AddrPC.Offset    = context.Eip;
  frame.AddrPC.Mode      = AddrModeFlat;
  frame.AddrFrame.Offset = context.Ebp;
  frame.AddrFrame.Mode   = AddrModeFlat;
  frame.AddrStack.Offset = context.Esp;
  frame.AddrStack.Mode   = AddrModeFlat;
#elif
  NOT IMPLEMENTED!
#endif

  HANDLE current_process = GetCurrentProcess();
  HANDLE current_thread = GetCurrentThread();

  // Walk the stack.
  unsigned int count = 0;
  {
    AutoDbgHelpLock thread_monitoring_lock;

    while (count < kMaxTraceFrames) {
      count++;
      if (!pStackWalk64(image_type,
                        current_process,
                        current_thread,
                        &frame,
                        &context,
                        0,
                        pSymFunctionTableAccess64,
                        pSymGetModuleBase64,
                        NULL))
        break;  // Couldn't trace back through any more frames.

      if (frame.AddrFrame.Offset == 0)
        continue;  // End of stack.

      // Push this frame's program counter onto the provided CallStack.
      AddFrame((DWORD_PTR)frame.AddrPC.Offset);
    }
    valid_ = true;
  }
  return true;
}

void CallStack::ToString(PrivateAllocatorString* output) {
  static const int kStackWalkMaxNameLen = MAX_SYM_NAME;
  HANDLE current_process = GetCurrentProcess();

  if (!LoadSymbols(current_process)) {
    *output = "Error";
    return;
  }

  base::AutoLock lock(dbghelp_lock_);

  // Iterate through each frame in the call stack.
  for (int32 index = 0; index < frame_count_; index++) {
    PrivateAllocatorString line;

    DWORD_PTR intruction_pointer = frame(index);

    SymbolCache::iterator it;
    it = symbol_cache_->find(intruction_pointer);
    if (it != symbol_cache_->end()) {
      line = it->second;
    } else {
      // Try to locate a symbol for this frame.
      DWORD64 symbol_displacement = 0;
      ULONG64 buffer[(sizeof(IMAGEHLP_SYMBOL64) +
                      sizeof(TCHAR)*kStackWalkMaxNameLen +
                      sizeof(ULONG64) - 1) / sizeof(ULONG64)];
      IMAGEHLP_SYMBOL64* symbol = reinterpret_cast<IMAGEHLP_SYMBOL64*>(buffer);
      memset(buffer, 0, sizeof(buffer));
      symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
      symbol->MaxNameLength = kStackWalkMaxNameLen;
      BOOL ok = pSymGetSymFromAddr64(current_process,            // hProcess
                                     intruction_pointer,         // Address
                                     &symbol_displacement,       // Displacement
                                     symbol);                    // Symbol
      if (ok) {
        // Try to locate more source information for the symbol.
        IMAGEHLP_LINE64 Line;
        memset(&Line, 0, sizeof(Line));
        Line.SizeOfStruct = sizeof(Line);
        DWORD line_displacement;
        ok = pSymGetLineFromAddr64(current_process,
                                   intruction_pointer,
                                   &line_displacement,
                                   &Line);
        if (ok) {
          // Skip junk symbols from our internal stuff.
          if (strstr(symbol->Name, "CallStack::") ||
              strstr(symbol->Name, "MemoryWatcher::") ||
              strstr(symbol->Name, "Perftools_") ||
              strstr(symbol->Name, "MemoryHook::") ) {
            // Just record a blank string.
            (*symbol_cache_)[intruction_pointer] = "";
            continue;
          }

          line += "    ";
          line += static_cast<char*>(Line.FileName);
          line += " (";
          // TODO(jar): get something like this template to work :-/
          // line += IntToCustomString<PrivateAllocatorString>(Line.LineNumber);
          // ...and then delete this line, which uses std::string.
          line += base::IntToString(Line.LineNumber).c_str();
          line += "): ";
          line += symbol->Name;
          line += "\n";
        } else {
          line += "    unknown (0):";
          line += symbol->Name;
          line += "\n";
        }
      } else {
        // OK - couldn't get any info.  Try for the module.
        IMAGEHLP_MODULE64 module_info;
        module_info.SizeOfStruct = sizeof(module_info);
        if (pSymGetModuleInfo64(current_process, intruction_pointer,
                                &module_info)) {
          line += "    (";
          line += static_cast<char*>(module_info.ModuleName);
          line += ")\n";
        } else {
          line += "    ???\n";
        }
      }
    }

    (*symbol_cache_)[intruction_pointer] = line;
    *output += line;
  }
  *output += "==================\n";
}


base::Lock AllocationStack::freelist_lock_;
AllocationStack* AllocationStack::freelist_ = NULL;

void* AllocationStack::operator new(size_t size) {
  DCHECK(size == sizeof(AllocationStack));
  {
    base::AutoLock lock(freelist_lock_);
    if (freelist_ != NULL) {
      AllocationStack* stack = freelist_;
      freelist_ = freelist_->next_;
      stack->next_ = NULL;
      return stack;
    }
  }
  return MemoryHook::Alloc(size);
}

void AllocationStack::operator delete(void* ptr) {
  AllocationStack *stack = reinterpret_cast<AllocationStack*>(ptr);
  base::AutoLock lock(freelist_lock_);
  DCHECK(stack->next_ == NULL);
  stack->next_ = freelist_;
  freelist_ = stack;
}
