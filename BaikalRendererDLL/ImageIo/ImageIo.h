#pragma once
#include <Tahoe/Math/Math.h>
#include <Tahoe/Math/Array.h>
#include <Tahoe/Base/ReferencedObject.h>

namespace Tahoe
{

class ImageIoFuncBase : public ReferencedObject
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIoFuncBase);
		virtual
		float4* load( const char* filename, int2& res ) const = 0;

		virtual
		float4* loadFromData( std::istream& filedata, int2& res ) const = 0;

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const = 0;

		static
		void convert(const float4* src, const int2& res, Array<u8>& dst);
};


class ImageIo : public ReferencedObject
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIo);

		static
		ImageIo& getInstance();

		static
		void init();

		static
		void quit();

		float4* load( const char* filename, int2& res ) const;

		// example of key : ".ies" , ".png" ...
		float4* loadFromData( std::istream& filedata, const std::string& key, int2& res ) const;

		void write( const char* filename, const float4* data, const int2& res ) const;

		void registerFunc( const char* key, const ImageIoFuncBase* func );

	private:
		ImageIo();
		~ImageIo();
		
	private:
		static ImageIo* s_manager;

		struct Entry
		{
			const ImageIoFuncBase* m_func;
			const char* m_key;

			Entry(){}
			Entry( const char* key, const ImageIoFuncBase* func ) : m_key(key), m_func(func) {}
		};

		Array<Entry> m_funcs;
};


};