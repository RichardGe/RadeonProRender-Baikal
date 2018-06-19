#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include <string>

std::string GetFireRenderBinaryDirectory()
{
#ifdef WIN32
    char path[MAX_PATH] = { '\0' };
    GetModuleFileName(NULL, path, MAX_PATH);
    std::string result = path;
    result = result.substr(0, result.find_last_of('\\') + 1);
    return result;
#else
    // Linux implementation
#endif
}