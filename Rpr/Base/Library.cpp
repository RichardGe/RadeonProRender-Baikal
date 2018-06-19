#ifdef WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "Library.h"

Library::Library()
    : m_handle(nullptr)
{
}

Library::~Library()
{
#ifdef WIN32
    if (m_handle != nullptr)
        ::FreeLibrary((HMODULE)m_handle);
#else
    // Linux implementation
    if (m_handle != nullptr)
        dlclose(m_handle);
#endif
}

bool Library::LoadFile(const char* path)
{
#ifdef WIN32
    m_handle = (void*)::LoadLibrary(path);
#else
    // Linux implementation
    m_handle = dlopen(path, RTLD_LAZY);
#endif
    return (m_handle != nullptr);
}

void* Library::GetEntryPoint(const char* name)
{
#ifdef WIN32
    return ::GetProcAddress((HMODULE)m_handle, name);
#else
    // Linux implementation
    return dlsym(m_handle, name);
#endif
}

std::string Library::GetPath() const
{
    char buff[1024] = { '\0' };

#ifdef WIN32
    if (::GetModuleFileName((HMODULE)m_handle, buff, 1024) == 0)
        buff[0] = '\0';
#else
    // Linux implementation
#endif

    return std::string(buff);
}