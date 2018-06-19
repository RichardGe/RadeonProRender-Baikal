#include <Tahoe/Io/ImageIo/OpenExr/ExrIo.h>

#if defined(TH_LINK_EXR)
#define ZLIB_WINAPI
#include <OpenExr/ImfCRgbaFile.h>
#endif

namespace Tahoe
{

bool ExrIo::read(const char *fn, Array<float4>& dst, int& width, int& height)
{
#if defined(TH_LINK_EXR)
	ImfInputFile *img;
	ImfRgba *buf;
	const ImfHeader *header;
	int xmin, xmax, ymax, ymin, y;

	img = ImfOpenInputFile(fn);

	if( img == 0 )
	{
		return false;
	}

	header = ImfInputHeader(img);
	ImfHeaderDataWindow(header, &xmin, &ymin, &xmax, &ymax);
	width = xmax - xmin + 1;
	height = ymax - ymin + 1;

	buf = (ImfRgba*)malloc(width*height * sizeof(ImfRgba));
	dst.setSize( width*height );

	for (y = 0; y < height; y++)
	{
		ImfInputSetFrameBuffer(img, buf-y*width, 1, width);
		ImfInputReadPixels(img, y, y);

		for(int i=0; i<width; i++)
		{
			float4& d = dst[i+y*width];
			d.x = ImfHalfToFloat( buf[i].r );
			d.y = ImfHalfToFloat( buf[i].g );
			d.z = ImfHalfToFloat( buf[i].b );
			d.w = ImfHalfToFloat( buf[i].a );
		}
	}

	free(buf);
	ImfCloseInputFile(img);
	return true;
#endif
	return false;
}

void ExrIo::write(const char* filename, const float4* src, int width, int height)
{
#if defined(TH_LINK_EXR)
	ImfHeader *header = NULL;
	ImfOutputFile *fp = NULL;

	Array<ImfRgba> pixels( width*height );

	for(int j=0; j<height; j++)
	{
		for(int i=0; i<width; i++)
		{
			int idx = i+j*width;
			const float4& s = src[idx];
			ImfRgba& d = pixels[idx];

			ImfFloatToHalf( s.x, &d.r );
			ImfFloatToHalf( s.y, &d.g );
			ImfFloatToHalf( s.z, &d.b );
			ImfFloatToHalf( s.w, &d.a );
		}
	}

	header = ImfNewHeader();

	ImfHeaderSetDisplayWindow(header, 0, 0, width - 1, height - 1);
	ImfHeaderSetDataWindow(header, 0, 0, width - 1, height - 1);
	ImfHeaderSetScreenWindowWidth(header, width);

	fp = ImfOpenOutputFile(filename, header, IMF_WRITE_RGBA);
	if (!fp) {
		exit(-1);
	}
	ImfDeleteHeader(header);

	ImfOutputSetFrameBuffer(fp,
		pixels.begin(), 
		1,      
		width); 

	ImfOutputWritePixels(fp,
		height);

	ImfCloseOutputFile(fp);
#endif
}

};

