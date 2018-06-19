#pragma once
#include <Tahoe/Math/Array.h>
#include <Tahoe/Math/Half.h>
#include <Tahoe/Io/ImageIo/ImageIo.h>
#include <Tahoe/Node/Node.h>

namespace Tahoe
{
class Lut : public Node
{
	public:
		TH_DECLARE_ALLOCATOR(Lut);

		Lut(): Node( NODE_COMPOSITE ), m_res(0){}

		void load( const float4* ptr, int res );

		float4 lookup( const float4& x ) const;

		int getRes() const { return m_res; }

		const half4* begin() const { return m_data.begin(); }

	private:
		int m_res;
		Array<half4> m_data;
};

};
