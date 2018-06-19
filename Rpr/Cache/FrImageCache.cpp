#include "FrImageCache.h"
#include "../Base/FrException.h"

#include <ctime>


#ifdef _WIN32 

#else
#include <sys/stat.h>
#endif


FrImageCache::Entry FrImageCache::LoadImage(FrNode* context, std::string const& fullPath)
{
    if (context != m_context)
    {
        throw FrException(__FILE__,__LINE__,RPR_ERROR_UNIMPLEMENTED, "Image caching for multiple contexts is not yet supported",context);
    }

    auto iter = m_cacheEntries.find(fullPath);

    if (iter != m_cacheEntries.end())
    {
		//if found, check that modification date corresponds.
		std::string modificationDateStr;
		GetModificationDateFile(fullPath,modificationDateStr);

		if ( 
			modificationDateStr != "" // if modification time found
			&& iter->second.lastModificationTime == modificationDateStr 
			)
		{
			// if same name and same modification time, we can give the cache
			return iter->second;
		}
		
		//if not the same modification time, we create a new image.

    }


	Entry entry = CreateImage(context, fullPath);
	m_cacheEntries[fullPath] = entry;
	return m_cacheEntries[fullPath];
    
}

void FrImageCache::CheckAndRemove(std::string const& path)
{
    auto iter = m_cacheEntries.find(path);

    if (iter != m_cacheEntries.end())
    {
        if (iter->second.data.use_count() == 1)
        {
            m_cacheEntries.erase(iter);
        }
    }
}

void FrImageCache::GetModificationDateFile(std::string const& fullPath,  std::string & dateOut)
{
	dateOut = "";

#ifdef _WIN32 
	struct _stat result;
	if(_stat(fullPath.c_str(), &result)==0)
#else
	struct stat result;
	if(stat(fullPath.c_str(), &result)==0)
#endif
	{
		char timebuf[512];
		time_t mod_time = result.st_mtime;
		strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&mod_time));
		dateOut = std::string(timebuf);
	}



	return;
}
