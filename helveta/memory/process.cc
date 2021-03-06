/*
*project name: helveta
*purpose: multi-purpose shared library for windows, made to be submodule for projects 
*written by: Cristei Gabriel 
*file written by: T0b1-iOS, @cristeigabriel
*licensing: MIT License

*file description: external process wrapper
*/

#include "helveta/initializer.hh"
#include <tlhelp32.h>

using namespace helveta::memory;

struct handle_wrapper {

  explicit handle_wrapper(const HANDLE handle) : _handle(handle) {}

  ~handle_wrapper() {

    if (_handle) CloseHandle(_handle);
  }

  handle_wrapper(const handle_wrapper &) = delete;
  handle_wrapper &operator=(const handle_wrapper &) = delete;

  HANDLE get() const { return _handle; }

protected:
  HANDLE _handle;
};

bool process::attach_hash(const fnv::hash_type name_hash) {

  const handle_wrapper snap_handle =
      handle_wrapper{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
  if (!snap_handle.get()) return false;

  PROCESSENTRY32 proc_entry{sizeof(PROCESSENTRY32)};

  if (!Process32First(snap_handle.get(), &proc_entry)) return false;

  do {

    const auto proc_name = std::string_view{proc_entry.szExeFile};

    if (fnv::hash_view(proc_name) == name_hash)
      return attach(proc_entry.th32ProcessID);

  } while (Process32Next(snap_handle.get(), &proc_entry));

  return false;
}

std::optional<process::module_t>
process::find_module(const fnv::hash_type name_hash) {

  if (_mod_list.empty()) init_module_list();

  for (const auto &module : _mod_list) {
    if (module.name_hash != name_hash) continue;

    return module;
  }

  return {};
}

void process::update_arch() {

  BOOL wow64;
  if (!IsWow64Process(_handle, &wow64)) return;

  _x64 = !wow64;
}

void process::init_module_list() {

  const handle_wrapper snap_handle = handle_wrapper{CreateToolhelp32Snapshot(
      _x64 ? TH32CS_SNAPMODULE : TH32CS_SNAPMODULE32, _id)};
  if (!snap_handle.get()) return;

  MODULEENTRY32 mod_entry{sizeof(MODULEENTRY32)};
  if (!Module32First(snap_handle.get(), &mod_entry)) return;

  _mod_list.clear();

  do {

    auto module      = module_t{};
    module.name_hash = fnv::hash(mod_entry.szExePath);
    module.addr      = reinterpret_cast<std::uintptr_t>(mod_entry.modBaseAddr);
    module.size      = mod_entry.modBaseSize;

    // edited from push_back, @T0b1 you're pushing to a constructor, think of le speed!!!
    _mod_list.emplace_back(module);

  } while (Module32Next(snap_handle.get(), &mod_entry));
}
