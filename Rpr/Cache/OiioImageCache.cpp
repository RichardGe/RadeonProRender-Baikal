#ifdef RPR_USE_OIIO

#ifndef WIN32
#include <OpenImageIO/imageio.h>
#else
#include <oiio/imageio.h>
#endif

OIIO_NAMESPACE_USING

#include "../Base/FrException.h"
#include "SimpleImageCache.h"
#include "OiioImageCache.h"

#include "Tahoe/Math/Half.h"
#include "Tahoe/Math/Half.cpp"

OiioImageCache::OiioImageCache(FrNode* context)
: FrImageCache(context)
{
}

FrImageCache::Entry OiioImageCache::CreateImage(FrNode* context, std::string const& fullPath)
{

	ImageSpec config = ImageSpec();
	config.attribute("oiio:UnassociatedAlpha", 1); // FA-156 - FIR-853   - make sure that IOOI doesn't multiply RGB by Alpha : unit test : test_feature_ContextImageFromFile_alpha
	ImageInput *in = ImageInput::open(fullPath,&config);


    if (!in)
    {
        throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Cannot load image file //// OIIO message = " + OpenImageIO::geterror(),context);
    }

    const ImageSpec &spec = in->spec();

    int xres = spec.width;
    int yres = spec.height;
    int channels = spec.nchannels;

    Entry entry;
    entry.path = fullPath;

    if (spec.format == TypeDesc::UINT8 || spec.format == TypeDesc::FLOAT)
    {
        size_t typeSize = (spec.format == TypeDesc::FLOAT) ? sizeof(float) : sizeof(char);
        unsigned char* pixels = new unsigned char[xres * yres * channels * typeSize];
        bool success = in->read_image(spec.format, &pixels[0]);
        

		// special case : 2 channels, with names : 
		// I (greyscale)
		// A (alpha)
		//
		// The raw format R8A8 and R32A32 is not supported by Radeon ProRender and Tahoe.
		// so we convert it to RGBA with R=G=B
		//
		if (   spec.nchannels == 2 
			&& spec.channelnames.size() == 2
			&& spec.channelnames[0] == "I"
			&& spec.channelnames[1] == "A"
			)
		{
			channels = 4;
			unsigned char* dst = new unsigned char[xres * yres * channels * typeSize];
			
			if (spec.format == TypeDesc::FLOAT)
			{
				float* dst_ = (float*)dst;
				float* src_ = (float*)pixels;

				for(int i=0; i<xres * yres; i++)
				{
					dst_[i*4+0] = src_[i*2+0]; // red
					dst_[i*4+1] = src_[i*2+0]; // green
					dst_[i*4+2] = src_[i*2+0]; // blue
					dst_[i*4+3] = src_[i*2+1]; // alpha
				}
			}
			else
			{
				unsigned char* dst_ = (unsigned char*)dst;
				unsigned char* src_ = (unsigned char*)pixels;

				for(int i=0; i<xres * yres; i++)
				{
					dst_[i*4+0] = src_[i*2+0]; // red
					dst_[i*4+1] = src_[i*2+0]; // green
					dst_[i*4+2] = src_[i*2+0]; // blue
					dst_[i*4+3] = src_[i*2+1]; // alpha
				}

			}

			delete[] pixels; //delete old buffer...
			pixels = dst; // ... and replace it by the new one.
		}


		in->close();

        entry.format.num_components = channels;
        entry.format.type = (spec.format == TypeDesc::FLOAT) ? RPR_COMPONENT_TYPE_FLOAT32 : RPR_COMPONENT_TYPE_UINT8;
        entry.desc = { (rpr_uint)xres, (rpr_uint)yres, 1U, (rpr_uint)(xres * typeSize * channels), 0U };
        entry.data.reset(pixels);
    }

	// Radeon ProRender and Tahoe don't support RGBA16.
	// we decided that it's better to convert the image from RGBA16 -> RGBA8 rather than support RGBA8
	// ( FIR-636 )
	else if ( spec.format == TypeDesc::UINT16 )
	{
        unsigned short* pixels_rgba16 = new unsigned short[xres * yres * channels * sizeof(short)];
        bool success = in->read_image(spec.format, &pixels_rgba16[0]);
        in->close();

		unsigned char* pixels_rgba8 = new unsigned char[xres * yres * channels * sizeof(char)];

		for(int i=0; i<xres * yres * channels; i++)
		{
			pixels_rgba8[i] = pixels_rgba16[i] / 257;
		}

		delete[] pixels_rgba16; pixels_rgba16=nullptr;

        entry.format.num_components = channels;
        entry.format.type = RPR_COMPONENT_TYPE_UINT8;
        entry.desc = { (rpr_uint)xres, (rpr_uint)yres, 1U, (rpr_uint)(xres * sizeof(char) * channels), 0U };
        entry.data.reset(pixels_rgba8);
	}

	else if ( spec.format == TypeDesc::HALF)
	{

		int sizeof_half = sizeof(Tahoe::half);

		//just a check in case someone decide to change it
		if ( sizeof_half != 2 )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT, "bad half size",context);
		}
		
		Tahoe::half* pixels_rgba16 = new Tahoe::half[xres * yres * channels * sizeof(Tahoe::half)];
        bool success = in->read_image(spec.format, &pixels_rgba16[0]);
        in->close();

		float* pixels_rgba_float = new float[xres * yres * channels * sizeof(float)];

		for(int i=0; i<xres * yres * channels; i++)
		{
			pixels_rgba_float[i]  = pixels_rgba16[i];
			int aa=0;
		}

		delete[] pixels_rgba16; pixels_rgba16=nullptr;

        entry.format.num_components = channels;
        entry.format.type = RPR_COMPONENT_TYPE_FLOAT32;
        entry.desc = { (rpr_uint)xres, (rpr_uint)yres, 1U, (rpr_uint)(xres * sizeof(float) * channels), 0U };
        entry.data.reset((unsigned char*)pixels_rgba_float);

	}

    else
    {
        in->close();
        throw FrException(__FILE__,__LINE__,RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT, "Unsupported pixel data format",context);
    }


	GetModificationDateFile(fullPath, entry.lastModificationTime);


    return entry;
}


#endif
