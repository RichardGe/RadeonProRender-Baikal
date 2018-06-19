#include <Tahoe/Math/Math.h>
#include <Tahoe/Math/Array.h>

namespace Tahoe
{
class ExrIo
{
public:
	static
	bool read(const char *fn, Array<float4>& dst, int& width, int& height);

	static
	void write(const char* filename, const float4* src, int width, int height);
};

};
