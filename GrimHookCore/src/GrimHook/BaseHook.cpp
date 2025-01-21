// NOTE: Even if this file was empty, it's required so BaseHook vtable is attached to a translation unit (this file).

#include "GrimHook/BaseHook.h"


GrimHook::BaseHook::BaseHook(std::shared_ptr<ManagedProcess> process) : m_process(std::move(process)) {}

GrimHook::BaseHook::~BaseHook() = default;
