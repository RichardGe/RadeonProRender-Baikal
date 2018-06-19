#include <Tahoe/Io/ImageIo/Cube/Lut.h>
#include <Tahoe/Io/ImageIo/Cube/LutCubeIo.h>
#include <fstream>
#include <string>

namespace Tahoe
{
void Lut::load( const float4* ptr, int res )
{
	m_res = res;
	m_data.setSize( res*res*res );

	for(int i=0; i<m_data.getSize(); i++)
	{
		const float4& a = ptr[i];
		m_data[i] = make_half4( a.x,a.y,a.z,a.w );
	}
}

float4 Lut::lookup( const float4& x ) const
{
	if( m_res == 0 )
		return x;

	float dx = 1.f/m_res;

	float i = x.x/dx;
	float di = i - floor(i);
	i = floor(i);
	float j = x.y/dx;
	float dj = j - floor(j);
	j = floor(j);
	float k = x.z/dx;
	float dk = k - floor(k);
	k = floor(k);

	half4 z0, z1;
	{
		half4 x0 = m_data[clamp((int)(i),0,m_res-1) + clamp((int)(j),0,m_res-1) * m_res + clamp((int)k,0,m_res-1) * m_res * m_res];
		half4 x1 = m_data[clamp((int)(i+1),0,m_res-1) + clamp((int)(j),0,m_res-1) * m_res + clamp((int)k,0,m_res-1) * m_res * m_res];
		half4 x2 = m_data[clamp((int)(i),0,m_res-1) + clamp((int)(j+1),0,m_res-1) * m_res + clamp((int)k,0,m_res-1) * m_res * m_res];
		half4 x3 = m_data[clamp((int)(i+1),0,m_res-1) + clamp((int)(j+1),0,m_res-1) * m_res + clamp((int)k,0,m_res-1) * m_res * m_res];
		z0 = lerp( lerp( x0, x1, di ), lerp( x2, x3, di ), dj );
	}
	{
		half4 x0 = m_data[clamp((int)(i),0,m_res-1) + clamp((int)(j),0,m_res-1) * m_res + clamp((int)(k+1),0,m_res-1) * m_res * m_res];
		half4 x1 = m_data[clamp((int)(i+1),0,m_res-1) + clamp((int)(j),0,m_res-1) * m_res + clamp((int)(k+1),0,m_res-1) * m_res * m_res];
		half4 x2 = m_data[clamp((int)(i),0,m_res-1) + clamp((int)(j+1),0,m_res-1) * m_res + clamp((int)(k+1),0,m_res-1) * m_res * m_res];
		half4 x3 = m_data[clamp((int)(i+1),0,m_res-1) + clamp((int)(j+1),0,m_res-1) * m_res + clamp((int)(k+1),0,m_res-1) * m_res * m_res];
		z1 = lerp( lerp( x0, x1, di ), lerp( x2, x3, di ), dj );
	}

	z0 = lerp( z0, z1, dk );
	return make_float4( z0.x, z0.y, z0.z, x.w );
}

CubeIoFunc::CubeIoFunc()
{

}

CubeIoFunc::~CubeIoFunc()
{

}

float4* CubeIoFunc::load( const char* path, int2& res ) const
{
	std::ifstream ifs;
	ifs.open( path );

	if (!ifs.is_open())
		return 0;

	float4* ptr = loadFromData(ifs,res);

	ifs.close();
	return ptr;
}

float4* CubeIoFunc::loadFromData( std::istream& ifs, int2& res ) const
{
	
	int& m_res = res.x;
	Array<float4> data;
	char tmp[128];
	std::string buff;

	while(ifs && std::getline(ifs, buff)) 
	{
		if( buff.empty() )
			continue;
		if( buff.at(0) == '#' )
			continue;
		if( buff.find("TITLE") != std::string::npos )
			continue;
		if( buff.find("LUT_3D_SIZE") != std::string::npos )
		{
			std::stringstream ss( buff );
			ss >> tmp;
			ss >> m_res;
			data.setSize( m_res*m_res*m_res );
			data.clear();
			continue;
		}

		{
			std::stringstream ss( buff );
			float4 x;
			ss >> x.x;
			ss >> x.y; 
			ss >> x.z;
			data.pushBack( x );
		}
	}

	float4* ptr = 0;
	if( m_res*m_res*m_res != data.getSize() )
	{
		data.clear();
		m_res = 0;
	}
	else
	{
		ptr = new float4[m_res*m_res*m_res];
		memcpy( ptr, data.begin(), sizeof(float4)*data.getSize() );
	}

	return ptr;
}

bool CubeIoFunc::write( const char* filename, const float4* data, const int2& res ) const
{
	return false;
}

void CubeIoFunc::convert(const float4* src, const int2& res, Array<u8>& dst)
{
}
};
