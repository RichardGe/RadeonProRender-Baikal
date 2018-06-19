#pragma once

#if defined(_APPLE_)
#include "stddef.h"
#else
#include "cstddef"
#endif

#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <ctype.h>

#include <math/matrix.h>
#include <math/float3.h>
#include <math/float2.h>

#include <Rpr/RadeonProRender.h>

typedef unsigned int uint;
typedef RadeonRays::float2 rpr_float2;
typedef RadeonRays::float3 rpr_float3;
typedef RadeonRays::float4 rpr_float4;
typedef RadeonRays::matrix rpr_matrix;

enum class PolygonType
{
    TRIANGLE,
    QUADS,
};

struct polygon
{
    PolygonType type;
    rpr_int p[4];
    rpr_int n[4];
    rpr_int t[4];
};

class ReferenceCountedObject
{
    volatile uint referenceCount_;

protected:
    virtual ~ReferenceCountedObject() { }

public:
    ReferenceCountedObject() : referenceCount_(1) { }

    uint referenceCount() const { return referenceCount_; }

    uint retain()  { return ++referenceCount_;}
    uint release()
    {
        uint count = --referenceCount_;
        if (0 == count)
        {
            delete this;
        }
        return count;
    }
};

struct ParameterDesc
{
	std::string name;
	std::string desc;
	rpr_parameter_type type;

	ParameterDesc(){}
	ParameterDesc(std::string const& n, std::string const& d, rpr_parameter_type t)
		: name(n)
		, desc(d)
		, type(t)
	{
	}
};

inline int GetParameterSize(rpr_parameter_type type)
{
	switch (type)
	{
	case RPR_PARAMETER_TYPE_UINT:
		return sizeof(rpr_uint);
	case RPR_PARAMETER_TYPE_FLOAT:
		return sizeof(float);
	case RPR_PARAMETER_TYPE_FLOAT2:
		return sizeof(float) * 2;
	case RPR_PARAMETER_TYPE_FLOAT3:
		return sizeof(float) * 3;
	case RPR_PARAMETER_TYPE_FLOAT4:
		return sizeof(float) * 4;
	case RPR_PARAMETER_TYPE_SHADER:
		return sizeof(rpr_material_node);
	case RPR_PARAMETER_TYPE_IMAGE:
		return sizeof(rpr_image);
	}

	return 0;
}

inline int GetFormatElementSize(rpr_component_type type, rpr_uint num_components)
{
    switch (type)
    {
    case RPR_COMPONENT_TYPE_FLOAT32:
        return 4 * num_components;
    case RPR_COMPONENT_TYPE_FLOAT16:
        return 2 * num_components;
    case RPR_COMPONENT_TYPE_UINT8:
        return num_components;
    default:
        return 0;
    }
}

std::string GetFireRenderBinaryDirectory();

#define SAFE_RETAIN(x)  if(x)(x)->retain()
#define SAFE_RELEASE(x) if(x)(x)->release()