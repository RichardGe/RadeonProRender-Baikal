#/*****************************************************************************\
*
*  Module Name    FrImageCache.h
*  Project        FireRender Engine
*
*  Description    FireRender image cache header file
*
*  Copyright 2011 - 2013 Advanced Micro Devices, Inc. (unpublished)
*
*  All rights reserved.  This notice is intended as a precaution against
*  inadvertent publication and does not imply publication or any waiver
*  of confidentiality.  The year included in the foregoing notice is the
*  year of creation of the work.
*
\*****************************************************************************/

#pragma once

#include <memory>
#include <map>
#include <vector>
#include "../RadeonProRender.h"
#include "../Node/FrNode.h"

class FrImageCache
{
public:
    struct Entry
    {
        Entry() 
		{ 
			
		}

        ~Entry()
        {
            data.reset();
        }

        std::string path;
		std::string lastModificationTime;
        rpr_image_format format;
        rpr_image_desc desc;
        std::shared_ptr<unsigned char> data;
    };

    FrImageCache(FrNode* context)
        : m_context(context) { }

    virtual ~FrImageCache() { }

    Entry LoadImage(FrNode* context, std::string const& fullPath);

    void CheckAndRemove(std::string const& path);

	//get modified date
	// return  dateOut = ""  if not found
	static void GetModificationDateFile(std::string const& fullPath,  std::string & dateOut);

protected:
    FrImageCache(FrImageCache const&) = delete;
    FrImageCache& operator = (FrImageCache const&) = delete;

    virtual Entry CreateImage(FrNode* context, std::string const& fullPath) = 0;

private:
    FrNode* m_context;
    std::map<std::string, Entry> m_cacheEntries;
};