#pragma once
#include <Tahoe/Math/Math.h>
#include <Tahoe/Io/ImageIo/ImageIo.h>


namespace Tahoe
{

class CubeIoFunc : public ImageIoFuncBase
{
	public:
		TH_DECLARE_ALLOCATOR(CubeIoFunc);

		CubeIoFunc();

		virtual
		float4* load( const char* filename, int2& res ) const;

		virtual
		float4* loadFromData( std::istream& filedata, int2& res ) const;

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const;

		static
		void convert(const float4* src, const int2& res, Array<u8>& dst);

	private:
		~CubeIoFunc();
};
};
