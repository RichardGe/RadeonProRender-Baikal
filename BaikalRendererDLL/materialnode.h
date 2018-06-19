#pragma once


#include <RprHybrid/Node/FrNode.h>
#include <RprHybrid/Base/Common.h>


class MaterialNode
{
public:

	MaterialNode()
	{
		m_type = UNDEF;
	}

	MaterialNode(rpr_uint ui)
	{
		SetUint1(ui);
	}

	~MaterialNode()
	{
		
	}

	enum MATNODETYPE 
	{ 
		UNDEF = 0,
		FRNODE, 
		FLOAT4, 
		UINT1,
	};

	void SetFloat4(const rpr_float4& n)
	{
		m_type = MATNODETYPE::FLOAT4;
		m_float4 = n;
	}

	void SetFloat4(rpr_float x, rpr_float y, rpr_float z, rpr_float w  )
	{
		m_type = MATNODETYPE::FLOAT4;
		m_float4.x = x;
		m_float4.y = y;
		m_float4.z = z;
		m_float4.w = w;
	}

	void SetUint1(const rpr_uint& n)
	{
		m_type = MATNODETYPE::UINT1;
		m_uint = n;
	}

	void SetFrNode(FrNode* node)
	{
		m_type = MATNODETYPE::FRNODE;
		m_node = node;
	}


//private:

	MATNODETYPE m_type;

	union
	{
		FrNode* m_node;
		rpr_float4 m_float4;
		rpr_uint m_uint;
	};
};



