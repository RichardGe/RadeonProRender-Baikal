#include "../Base/FrException.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../BaikalRendererDLL/ImageIo/HdrLoader/stb_image.h"

#define TINYEXR_IMPLEMENTATION
#include "../BaikalRendererDLL/ImageIo/OpenExr/tinyexr.h"

#include "../BaikalRendererDLL/ImageIo/libTiff/tiffio.h"

#include "SimpleImageCache.h"

SimpleImageCache::SimpleImageCache(FrNode* context)
    : FrImageCache(context)
{
}

FrImageCache::Entry SimpleImageCache::CreateImage(FrNode* context, std::string const& fullPath)
{
    char const* supportedFormats_STBI_char[] =
    {
        ".png",
        ".bmp",
        ".tga",
        ".jpg",
        ".gif",
    };

	char const* supportedFormats_STBI_float[] =
    {
		".hdr",
    };

	char const* supportedFormats_TinyEXR[] =
    {
		".exr",
    };

	char const* supportedFormats_LibTiff[] =
    {
		".tif",
    };


    int numSupportedFormats_STBI_char = sizeof(supportedFormats_STBI_char) / sizeof(char const*);
	int numSupportedFormats_STBI_float = sizeof(supportedFormats_STBI_float) / sizeof(char const*);
	int numSupportedFormats_TinyEXR = sizeof(supportedFormats_TinyEXR) / sizeof(char const*);
	int numSupportedFormats_LibTiff = sizeof(supportedFormats_LibTiff) / sizeof(char const*);
    bool isSupported_STBI_char = false;
	bool isSupported_STBI_float = false;
	bool isSupported_TinyEXR = false;
	bool isSupported_LibTiff = false;

    std::string fullName = fullPath;

	
	// get the extension from file name, in lower case,  with the dot
	std::string fileExtension = "";
	{
		std::string fullName_lower = fullName;
		std::transform(fullName_lower.begin(), fullName_lower.end(), fullName_lower.begin(), ::tolower);
		for(int i=0; ;i++)
		{
			if ( i >= fullName_lower.size() )
			{
				fileExtension = "";
				break;
			}

			fileExtension += fullName_lower[fullName_lower.size()-i-1];

			if ( fullName_lower[fullName_lower.size()-i-1] == '.' )
				break;
		}
		std::reverse(fileExtension.begin(),fileExtension.end());
	}


    for (int i = 0; i < numSupportedFormats_STBI_char; ++i)
    {
        if ( fileExtension == supportedFormats_STBI_char[i] )
        {
            isSupported_STBI_char = true;
            break;
        }
    }

	for (int i = 0; i < numSupportedFormats_STBI_float; ++i)
    {
        if ( fileExtension == supportedFormats_STBI_float[i] )
        {
            isSupported_STBI_float = true;
            break;
        }
    }

	for (int i = 0; i < numSupportedFormats_TinyEXR; ++i)
    {
        if ( fileExtension == supportedFormats_TinyEXR[i] )
        {
            isSupported_TinyEXR = true;
            break;
        }
    }

	for (int i = 0; i < numSupportedFormats_LibTiff; ++i)
    {
        if ( fileExtension == supportedFormats_LibTiff[i] )
        {
            isSupported_LibTiff = true;
            break;
        }
    }

    if (isSupported_STBI_char)
    {
        unsigned char* pixelBuffer;
        int width, height, numChannels_original;

        pixelBuffer = stbi_load(fullName.c_str(), &width, &height, &numChannels_original, 0);

        if (pixelBuffer == NULL)
        {
            throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load image", context);
        }

        int numChannels_exposed = 0;
		unsigned char* data = nullptr;

		// special case : for PNG, if there are 2 channels, then it seems to be necessay  Grey + Alpha.
		//                this is the spec of the png_get_channels from official lib libpng.
		if ( fileExtension == ".png"  &&  numChannels_original == 2 )
		{
			numChannels_exposed = 4;
			data = new unsigned char[width * height * numChannels_exposed];


			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; i++)
				{
					int idx = i + j*width;
					int didx = i + (height - 1 - j)*width;
					data[(j * width + i) * numChannels_exposed + 0] = pixelBuffer[numChannels_original*idx];
					data[(j * width + i) * numChannels_exposed + 1] = pixelBuffer[numChannels_original*idx];
					data[(j * width + i) * numChannels_exposed + 2] = pixelBuffer[numChannels_original*idx];
					data[(j * width + i) * numChannels_exposed + 3] = pixelBuffer[numChannels_original*idx + 1];
				}
			}

		}

		//all the other cases
		else
		{
			numChannels_exposed = numChannels_original;
			data = new unsigned char[width * height * numChannels_exposed];

			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; i++)
				{
					int idx = i + j*width;
					int didx = i + (height - 1 - j)*width;

					data[(j * width + i) * numChannels_original] = pixelBuffer[numChannels_original*idx];

					if (numChannels_original > 1)
					{
						data[(j * width + i) * numChannels_original + 1] = pixelBuffer[numChannels_original*idx + 1];

						if (numChannels_original > 2)
						{
							data[(j * width + i) * numChannels_original + 2] = pixelBuffer[numChannels_original*idx + 2];

							if (numChannels_original > 3)
							{
								data[(j * width + i) * numChannels_original + 3] = pixelBuffer[numChannels_original*idx + 3];
							}
						}
					}
				}
			}
		}



        stbi_image_free(pixelBuffer);

        Entry entry;
        entry.path = fullPath;
        entry.format = { static_cast<rpr_uint>(numChannels_exposed), RPR_COMPONENT_TYPE_UINT8 };
        entry.desc = 
		{ 
			static_cast<rpr_uint>(width), 
			static_cast<rpr_uint>(height), 
			1,


			0,0
			
			//static_cast<rpr_uint>(width * numChannels_exposed), 
			//1 
		};

        entry.data.reset(data);

        return entry;
    }

	else if (isSupported_STBI_float)
    {
        float* pixelBuffer;
        int width, height, numChannels;

        pixelBuffer = stbi_loadf(fullName.c_str(), &width, &height, &numChannels, 0);

        if (pixelBuffer == NULL)
        {
            throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load image", context);
        }

        float* data = new float[width * height * numChannels];


		/*
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                int idx = i + j*width;
    

                data[(j * width + i) * numChannels] = pixelBuffer[numChannels*idx];

                if (numChannels > 1)
                {
                    data[(j * width + i) * numChannels + 1] = pixelBuffer[numChannels*idx + 1];

                    if (numChannels > 2)
                    {
                        data[(j * width + i) * numChannels + 2] = pixelBuffer[numChannels*idx + 2];

                        if (numChannels > 3)
                        {
                            data[(j * width + i) * numChannels + 3] = pixelBuffer[numChannels*idx + 3];
                        }
                    }
                }
            }
        }
		*/



		for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
				int idx = i + j*width;
                //int idx = i +  (  height - j - 1  ) *width;

                data[(j * width + i) * numChannels] = pixelBuffer[numChannels*idx];

                if (numChannels > 1)
                {
                    data[(j * width + i) * numChannels + 1] = pixelBuffer[numChannels*idx + 1];

                    if (numChannels > 2)
                    {
                        data[(j * width + i) * numChannels + 2] = pixelBuffer[numChannels*idx + 2];

                        if (numChannels > 3)
                        {
                            data[(j * width + i) * numChannels + 3] = pixelBuffer[numChannels*idx + 3];
                        }
                    }
                }
            }
        }


		data[0];
		data[1];
		data[2];

		data[3];
		data[4];
		data[5];

		data[6];


        stbi_image_free(pixelBuffer);

        Entry entry;
        entry.path = fullPath;
        entry.format = { static_cast<rpr_uint>(numChannels), RPR_COMPONENT_TYPE_FLOAT32 };
        entry.desc = 
		{ 
			static_cast<rpr_uint>(width), 
			static_cast<rpr_uint>(height), 
			1, 

			0,0

			//static_cast<rpr_uint>(width * numChannels), 
			//1 
		};

        entry.data.reset((unsigned char*)data);

        return entry;
    }


	else if ( isSupported_TinyEXR )
	{
		float* pixelBuffer = nullptr; // width * height * RGBA
		const char* err = nullptr;
		int width = 0;
		int height = 0;

		int ret = LoadEXR(&pixelBuffer, &width, &height, fullName.c_str(), &err);

		if( ret != TINYEXR_SUCCESS || pixelBuffer == nullptr )
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load image", context);
		}

		const int numChannels = 4;

		float* data = new float[width * height * numChannels];

        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                int idx = i + j*width;
                int didx = i + (height - 1 - j)*width;
                data[(j * width + i) * numChannels] = pixelBuffer[numChannels*idx];
				data[(j * width + i) * numChannels + 1] = pixelBuffer[numChannels*idx + 1];
				data[(j * width + i) * numChannels + 2] = pixelBuffer[numChannels*idx + 2];
				data[(j * width + i) * numChannels + 3] = pixelBuffer[numChannels*idx + 3];
            }
        }

        Entry entry;
        entry.path = fullPath;
        entry.format = { static_cast<rpr_uint>(numChannels), RPR_COMPONENT_TYPE_FLOAT32 };
        entry.desc = 
		{ 
			static_cast<rpr_uint>(width), 
			static_cast<rpr_uint>(height), 
			1, 

			0,0


			//static_cast<rpr_uint>(width * numChannels), 
			//1 
		};

        entry.data.reset((unsigned char*)data);

		free(pixelBuffer); pixelBuffer = nullptr;

        return entry;

	}

	else if ( isSupported_LibTiff )
	{
		TIFF* tif = TIFFOpen(fullPath.c_str(), "r");
		
		if (!tif) 
		{
			throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load TIFF image - TIFFOpen", context);
		}

		uint32 width = 0;
		uint32 height = 0;
		size_t npixels = 0;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

		if ( width == 0 || height == 0 )
		{
			TIFFClose(tif); tif = nullptr;
			throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load TIFF image - TIFFGetField", context);
		}

		npixels = width * height;
		uint32* raster = (uint32*) _TIFFmalloc(npixels * sizeof (uint32));
		if (raster == nullptr) 
		{
			TIFFClose(tif); tif = nullptr;
			throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load TIFF image - _TIFFmalloc", context);
		}


		if (TIFFReadRGBAImage(tif, width, height, raster, 0)) 
		{
			const int numChannels = 4;
			unsigned char* data = new unsigned char[width * height * numChannels];
			for (uint32 j = 0; j < height; j++)
			{
				for (uint32 i = 0; i < width; i++)
				{
					int idx = i + j*width;
					int didx = i + (height - 1 - j)*width;

					uint32 pxlColor = raster[idx];
					const unsigned char* pxlColor8 = (const unsigned char*)(&pxlColor);
					data[(j * width + i) * numChannels] =      pxlColor8[0];
					data[(j * width + i) * numChannels + 1] =  pxlColor8[1];
					data[(j * width + i) * numChannels + 2] =  pxlColor8[2];
					data[(j * width + i) * numChannels + 3] =  pxlColor8[3];
				}
			}

			Entry entry;
			entry.path = fullPath;
			entry.format = { static_cast<rpr_uint>(numChannels), RPR_COMPONENT_TYPE_UINT8 };
			entry.desc = 
			{ 
				static_cast<rpr_uint>(width), 
				static_cast<rpr_uint>(height), 
				1, 

				0,0


				//static_cast<rpr_uint>(width * numChannels), 
				//1 
			};

			entry.data.reset((unsigned char*)data);

			if ( raster ) {_TIFFfree(raster); raster = nullptr; }
			if ( tif ) {TIFFClose(tif); tif = nullptr; }
			return entry;

		}

		if ( raster ) { _TIFFfree(raster); raster = nullptr; }
		if ( tif ) { TIFFClose(tif); tif = nullptr; }
		throw FrException(__FILE__,__LINE__,RPR_ERROR_IO_ERROR, "Can't load TIFF image - TIFFReadRGBAImage", context);
		

		
	}
	else
	{
		throw FrException(__FILE__,__LINE__,RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT, "Unsupported image format", context);
	}

	Entry entryFail;
	return entryFail;
}
