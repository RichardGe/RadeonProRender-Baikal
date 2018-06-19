#pragma once

#include <string>

//	This class wraps the loading of a dynamic link library
class Library
{
public:
    Library();
    virtual ~Library();

    // Loads the specified library from path
    bool LoadFile(const char* path);

    // Gets the specified function from the library
    void* GetEntryPoint(const char* name);

    // Gets the full dll path and filename
    std::string GetPath() const;

protected:
    void* m_handle;
};