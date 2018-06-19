#include <Tahoe/Io/ImageIo/ImageIo.h>

//==
#include <Tahoe/Io/ImageIo/CompressedLoader/CompressedWriter.h>
#include <Tahoe/Io/ImageIo/OpenExr/tinyexr.h>

#include <Tahoe/Io/ImageIo/HdrLoader/stb_image.h>

#ifndef TH_USE_OIIO
#else
	#ifndef WIN32
	#include <OpenImageIO/imageio.h>
	#else
	#include <oiio/imageio.h>
	#endif
#endif

#define TJE_IMPLEMENTATION
#include <Tahoe/Io/ImageIo/Jpeg/tiny_jpeg.h>

#include <Tahoe/Light/IESLightData.h>
#include <Tahoe/Io/ImageIo/Cube/LutCubeIo.h>

namespace Tahoe
{

class ImageIoStbi : public ImageIoFuncBase
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIoStbi);

		virtual
		float4* load( const char* filename, int2& res ) const
		{
			unsigned char* pixels;
			int width;
			int height;
			int bpp;

			pixels = stbi_load( filename, &width, &height, &bpp, 0 );

			if( pixels == 0 )
			{
				TH_LOG_ERROR( "Texture Not Found (%s)\n", filename );
				return 0;
			}

			res.x = width;
			res.y = height;

			float4* ptr = new float4[res.x*res.y];

			for(int j=0; j<height; j++)
			for(int i=0; i<width; i++)
			{
				int idx = i+j*width;
				int didx = i+j*width;
				ptr[didx] = make_float4( 0.f );
				if( bpp >= 1 )
					ptr[didx].x = pixels[bpp*idx+0]/255.f;
				if( bpp >= 2 )
					ptr[didx].y = pixels[bpp*idx+1]/255.f;
				if( bpp >= 3 )
					ptr[didx].z = pixels[bpp*idx+2]/255.f;
				if( bpp == 4 )
				{
					ptr[didx].w = pixels[bpp*idx+3]/255.f;
				}
			}

			stbi_image_free (pixels);
			return ptr;
		}

		virtual
		float4* loadFromData( std::istream& filedata, int2& res ) const
		{
			//todo
			return NULL;
		}

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const
		{
			Array<u8> pixels;
			convert( data, res, pixels );

			if( strstr( filename, ".png" ) != 0 )
			{
				stbi_write_png(filename, res.x, res.y, 3, pixels.begin(), res.x*3);
				return 1;
			}
			else if( strstr( filename, ".bmp" ) != 0 )
			{
				stbi_write_bmp(filename, res.x, res.y, 3, pixels.begin());
				return 1;
			}
			else if( strstr( filename, ".tga" ) != 0 )
			{
				stbi_write_tga(filename, res.x, res.y, 3, pixels.begin());
				return 1;
			}
			else if( strstr( filename, ".jpg" ) != 0 )
			{
				tje_encode_to_file( filename, res.x, res.y, 3, pixels.begin() );
				return 1;
			}

			return false;
		}
};

class ImageIoExr : public ImageIoFuncBase
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIoExr);

		virtual
		float4* load( const char* filename, int2& res ) const
		{
			float* out; // width * height * RGBA
			const char* err;

			int ret = LoadEXR(&out, &res.x, &res.y, filename, &err);

			if( ret != TINYEXR_SUCCESS )
			{
				TH_LOG_ERROR( "Texture Not Found (%s)\n", filename );
				return 0;
			}

			return (float4*)out;
		}

		virtual
		float4* loadFromData( std::istream& filedata, int2& res ) const
		{
			//todo
			return NULL;
		}

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const
		{
			int ret = SaveEXR( (float*)data, res.x, res.y, 4, 1, filename );
			if( ret != TINYEXR_SUCCESS )
			{
				TH_LOG_ERROR( "Texture Not Found (%s)\n", filename );
				return false;
			}
			return true;
		}
};

class ImageIoHdrR : public ImageIoFuncBase
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIoHdrR);

		virtual
		float4* load( const char* filename, int2& res ) const
		{
#ifndef TH_USE_OIIO
			int comp;
			float* data = stbi_loadf( filename, &res.x, &res.y, &comp, 0 );
			if( data == 0 )
			{
				return 0;
			}
			float4* ptr = new float4[res.x*res.y];

			for(int j=0; j<res.y; j++)
				for(int i=0; i<res.x; i++)
				{
					int idx = (i+j*res.x)*3;
					ptr[i+j*res.x] = make_float4( data[idx+0], data[idx+1], data[idx+2] );
				}

			stbi_image_free( data );
			return ptr;
#else
            OIIO_NAMESPACE_USING
            ImageInput *in = ImageInput::open(filename);

            if (!in)
            {
                return NULL;
            }

            const ImageSpec &spec = in->spec();

            res.x = spec.width;
            res.y = spec.height;

            float4* ptr = new float4[res.x*res.y];

            bool success = in->read_image(TypeDesc::FLOAT, ptr, sizeof(float4));

            in->close();
            delete in;

            if (!success)
            {
                delete ptr;
                return NULL;
            }

            return ptr;
#endif
		}

		virtual
		float4* loadFromData( std::istream& filedata, int2& res ) const
		{
			//todo
			return NULL;
		}

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const
		{
			return 0;
		}
};

class ImageIoIes : public ImageIoFuncBase
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIoIes);

		virtual
		float4* load( const char* filename, int2& res ) const
		{
			try
			{
				if( res.x <= 0 || res.y <= 0 )
					return 0;

				IESLightData iesData;
				iesData.load(std::string(filename));

				float4* ptr = iesData.toImageMap(res);

				return ptr;
			}
			catch(const std::exception& ex)
			{
				return nullptr;
			}
		}

		virtual
		float4* loadFromData( std::istream &file, int2& res ) const
		{
			try
			{
				if( res.x <= 0 || res.y <= 0 )
					return 0;

				IESLightData iesData;
				iesData.loadFromData(file,"");

				float4* ptr = iesData.toImageMap(res);

				return ptr;
			}
			catch(const std::exception& ex)
			{
				return nullptr;
			}
		}

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const
		{
			return true;
		}
};

class ImageIoJpeg : public ImageIoFuncBase
{
	public:
		TH_DECLARE_ALLOCATOR(ImageIoJpeg);

		virtual
		float4* load( const char* filename, int2& res ) const
		{
			return 0;
		}

		virtual
		float4* loadFromData( std::istream& filedata, int2& res ) const
		{
			//todo
			return NULL;
		}

		virtual
		bool write( const char* filename, const float4* data, const int2& res ) const
		{
			Array<u8> pixels;
			convert( data, res, pixels );

			tje_encode_to_file( filename, res.x, res.y, 3, pixels.begin() );

			return true;
		}
};

//===

void ImageIoFuncBase::convert(const float4* src, const int2& res, Array<u8>& dst)
{
	dst.setSize( res.x*res.y );
	Array<u8>& pixels = dst;
	pixels.setSize( res.x*res.y*3 );

	for(int j=0; j<res.y; j++)
	for(int i=0; i<res.x; i++)
	{
		int idx = i+j*res.x;
		int sIdx = (i)+j*res.x;

		pixels[3*idx+0] = clamp(int(src[sIdx].x*255.f), 0, 255);
		pixels[3*idx+1] = clamp(int(src[sIdx].y*255.f), 0, 255);
		pixels[3*idx+2] = clamp(int(src[sIdx].z*255.f), 0, 255);
	}

}

ImageIo* ImageIo::s_manager = 0;


ImageIo& ImageIo::getInstance()
{
	if( s_manager == 0 )
	{
		s_manager = new ImageIo;
	}
	return *s_manager;
}

ImageIo::ImageIo()
{
	{
		ImageIoStbi* f = new ImageIoStbi();
		registerFunc( ".png", f );
		registerFunc( ".jpg", f );
		registerFunc( ".gif", f );
		registerFunc( ".bmp", f );
		registerFunc( ".tga", f );
		f->removeReference();
	}
	{
		ImageIoExr* f = new ImageIoExr();
		registerFunc( ".exr", f );
		f->removeReference();
	}
	{
		ImageIoHdrR* f = new ImageIoHdrR();
		registerFunc( ".hdr", f );
		f->removeReference();
	}
	{
		ImageIoIes* f = new ImageIoIes();
		registerFunc( ".ies", f );
		f->removeReference();
	}
	if(0)
	{
		ImageIoJpeg* f = new ImageIoJpeg();
		registerFunc( ".jpg", f );
		f->removeReference();
	}
	{
		CubeIoFunc* f = new CubeIoFunc();
		registerFunc( ".cube", f );
		f->removeReference();
	}
}

ImageIo::~ImageIo()
{
	for(int i=0; i<m_funcs.getSize(); i++)
	{
		ImageIoFuncBase* nonconst = (ImageIoFuncBase*)m_funcs[i].m_func;
		nonconst->removeReference();
	}
}

void ImageIo::init()
{
	if( s_manager == 0 )
	{
		s_manager = new ImageIo;
	}
	else
	{
		s_manager->addReference();
	}
}

void ImageIo::quit()
{
	if( s_manager )
	{
		int c = s_manager->getReferenceCount();
		s_manager->removeReference();
		if( c == 0 )
			s_manager = 0;
	}
}

float4* ImageIo::load( const char* filename, int2& res ) const
{
	for(int i=0; i<m_funcs.getSize(); i++)
	{
		const Entry& e = m_funcs[i];

		//m_funcs m_key are in lower case so filename must be in lower case too
		std::string filenameLowercase = std::string(filename);
		std::transform(filenameLowercase.begin(), filenameLowercase.end(), filenameLowercase.begin(), ::tolower);

		if( strstr( filenameLowercase.c_str(), e.m_key ) != 0 )
		{
			float4* ptr = e.m_func->load( filename, res );
			if( ptr )
				TH_LOG_IO( "Texture Loaded (%s)\n", filename );
			return ptr;
		}
	}
	return 0;
}

float4* ImageIo::loadFromData( std::istream& filedata, const std::string& key, int2& res ) const
{
	for(int i=0; i<m_funcs.getSize(); i++)
	{
		const Entry& e = m_funcs[i];
		if ( key == std::string(e.m_key) )
		{
			float4* ptr = e.m_func->loadFromData( filedata, res );
			if( ptr )
				TH_LOG_IO( "Texture Loaded from data.\n" );
			return ptr;
		}
	}
	return 0;
}

void ImageIo::write( const char* filename, const float4* data, const int2& res ) const
{
	for(int i=0; i<m_funcs.getSize(); i++)
	{
		const Entry& e = m_funcs[i];
		if( strstr( filename, e.m_key ) != 0 )
		{
			bool done = e.m_func->write( filename, data, res );
			if( done )
				TH_LOG_IO( "Texture Written (%s)\n", filename );
			return;
		}
	}
	TH_LOG_ERROR("Unsupported file format (%s)\n", filename);
}

void ImageIo::registerFunc( const char* key, const ImageIoFuncBase* func )
{
	ImageIoFuncBase* nonconst = (ImageIoFuncBase*)func;
	nonconst->addReference();
	bool found = 0;
	for(int i=0; i<m_funcs.getSize(); i++)
	{
		if( strcmp( m_funcs[i].m_key, key ) == 0 )
		{
			found = 1;
			ImageIoFuncBase* nonconst = (ImageIoFuncBase*)m_funcs[i].m_func;
			nonconst->removeReference();
			m_funcs[i].m_func = func;
			return;
		}
	}
	if( !found )
		m_funcs.pushBack( Entry( key, func ) );
}

};
