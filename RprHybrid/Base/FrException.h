/*****************************************************************************\
*
*  Module Name    FrException.h
*  Project        FireRender Engine
*
*  Description    FireRender exception declarations
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

#include <stdexcept>
#include <string>
//#include "Common.h"


class FrException : public std::exception
{
public:
    FrException()
        : errorCode_(0)        
    {
    }

	//frNode must be a FrNode* of any type in the failing context
    FrException(const char* fileName, int line, int errorCode, std::string const& errorDesc, void* frNode = nullptr ) throw();
	FrException(int errorCode, std::string const& errorDesc) throw();

    ~FrException() throw() {}

    virtual char const* what() const throw()
    {
        return errorDesc_.c_str();
    }

    virtual int GetErrorCode() const throw()
    {
        return errorCode_;
    }

private:
    int      errorCode_;
    std::string errorDesc_;
};
