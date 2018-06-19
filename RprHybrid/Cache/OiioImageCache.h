#/*****************************************************************************\
*
*  Module Name    SimpleImageCache.h
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

#include "FrImageCache.h"

#ifdef RPR_USE_OIIO

class OiioImageCache : public FrImageCache
{
public:
    OiioImageCache(FrNode* context);

protected:
    Entry CreateImage(FrNode* context, std::string const& fullPath);
};

#endif