#include "pch.h"

// The vendored NuGet package only ships the D3DX math headers. These are the
// few non-inline helpers the gameplay code still references, implemented
// locally so no DirectX SDK library (June 2010 or otherwise) is required by
// the renderer. Behaviour follows the D3DX row-vector / row-major convention.

#include <cmath>
#include <cstring>

namespace
{
	constexpr float kNormalizeEpsilon = 1e-6f;

	inline float Dot( const D3DXVECTOR3 & A, const D3DXVECTOR3 & B )
	{
		return A.x * B.x + A.y * B.y + A.z * B.z;
	}

	inline D3DXVECTOR3 Cross( const D3DXVECTOR3 & A, const D3DXVECTOR3 & B )
	{
		return D3DXVECTOR3(
			A.y * B.z - A.z * B.y,
			A.z * B.x - A.x * B.z,
			A.x * B.y - A.y * B.x );
	}

	inline D3DXVECTOR3 Normalized( const D3DXVECTOR3 & V )
	{
		const float LengthSq = Dot( V, V );
		if ( LengthSq <= kNormalizeEpsilon * kNormalizeEpsilon )
			return D3DXVECTOR3( 0.f, 0.f, 0.f );

		const float Inv = 1.f / std::sqrt( LengthSq );
		return D3DXVECTOR3( V.x * Inv, V.y * Inv, V.z * Inv );
	}
}

extern "C" {

	D3DXVECTOR3 * WINAPI D3DXVec3Normalize( D3DXVECTOR3 * pOut, const D3DXVECTOR3 * pV )
	{
		if ( !pOut || !pV )
			return pOut;

		*pOut = Normalized( *pV );
		return pOut;
	}

	D3DXVECTOR4 * WINAPI D3DXVec3Transform( D3DXVECTOR4 * pOut, const D3DXVECTOR3 * pV, const D3DXMATRIX * pM )
	{
		if ( !pOut || !pV || !pM )
			return pOut;

		pOut->x = pV->x * pM->m[ 0 ][ 0 ] + pV->y * pM->m[ 1 ][ 0 ] + pV->z * pM->m[ 2 ][ 0 ] + pM->m[ 3 ][ 0 ];
		pOut->y = pV->x * pM->m[ 0 ][ 1 ] + pV->y * pM->m[ 1 ][ 1 ] + pV->z * pM->m[ 2 ][ 1 ] + pM->m[ 3 ][ 1 ];
		pOut->z = pV->x * pM->m[ 0 ][ 2 ] + pV->y * pM->m[ 1 ][ 2 ] + pV->z * pM->m[ 2 ][ 2 ] + pM->m[ 3 ][ 2 ];
		pOut->w = pV->x * pM->m[ 0 ][ 3 ] + pV->y * pM->m[ 1 ][ 3 ] + pV->z * pM->m[ 2 ][ 3 ] + pM->m[ 3 ][ 3 ];
		return pOut;
	}

	D3DXVECTOR3 * WINAPI D3DXVec3TransformNormal( D3DXVECTOR3 * pOut, const D3DXVECTOR3 * pV, const D3DXMATRIX * pM )
	{
		if ( !pOut || !pV || !pM )
			return pOut;

		pOut->x = pV->x * pM->m[ 0 ][ 0 ] + pV->y * pM->m[ 1 ][ 0 ] + pV->z * pM->m[ 2 ][ 0 ];
		pOut->y = pV->x * pM->m[ 0 ][ 1 ] + pV->y * pM->m[ 1 ][ 1 ] + pV->z * pM->m[ 2 ][ 1 ];
		pOut->z = pV->x * pM->m[ 0 ][ 2 ] + pV->y * pM->m[ 1 ][ 2 ] + pV->z * pM->m[ 2 ][ 2 ];
		return pOut;
	}

	D3DXMATRIX * WINAPI D3DXMatrixTranspose( D3DXMATRIX * pOut, const D3DXMATRIX * pM )
	{
		if ( !pOut || !pM )
			return pOut;

		D3DXMATRIX Result;
		for ( UINT Row = 0; Row < 4; ++Row )
			for ( UINT Col = 0; Col < 4; ++Col )
				Result.m[ Row ][ Col ] = pM->m[ Col ][ Row ];

		*pOut = Result;
		return pOut;
	}

	D3DXMATRIX * WINAPI D3DXMatrixMultiply( D3DXMATRIX * pOut, const D3DXMATRIX * pM1, const D3DXMATRIX * pM2 )
	{
		if ( !pOut || !pM1 || !pM2 )
			return pOut;

		D3DXMATRIX Result;
		for ( UINT Row = 0; Row < 4; ++Row )
		{
			for ( UINT Col = 0; Col < 4; ++Col )
			{
				Result.m[ Row ][ Col ] =
					pM1->m[ Row ][ 0 ] * pM2->m[ 0 ][ Col ] +
					pM1->m[ Row ][ 1 ] * pM2->m[ 1 ][ Col ] +
					pM1->m[ Row ][ 2 ] * pM2->m[ 2 ][ Col ] +
					pM1->m[ Row ][ 3 ] * pM2->m[ 3 ][ Col ];
			}
		}

		*pOut = Result;
		return pOut;
	}

	D3DXMATRIX * WINAPI D3DXMatrixLookAtLH( D3DXMATRIX * pOut, const D3DXVECTOR3 * pEye, const D3DXVECTOR3 * pAt, const D3DXVECTOR3 * pUp )
	{
		if ( !pOut || !pEye || !pAt || !pUp )
			return pOut;

		const D3DXVECTOR3 ZAxis = Normalized( D3DXVECTOR3( pAt->x - pEye->x, pAt->y - pEye->y, pAt->z - pEye->z ) );
		const D3DXVECTOR3 XAxis = Normalized( Cross( *pUp, ZAxis ) );
		const D3DXVECTOR3 YAxis = Cross( ZAxis, XAxis );

		std::memset( pOut, 0, sizeof( D3DXMATRIX ) );
		pOut->m[ 0 ][ 0 ] = XAxis.x;
		pOut->m[ 1 ][ 0 ] = XAxis.y;
		pOut->m[ 2 ][ 0 ] = XAxis.z;
		pOut->m[ 3 ][ 0 ] = -Dot( XAxis, *pEye );

		pOut->m[ 0 ][ 1 ] = YAxis.x;
		pOut->m[ 1 ][ 1 ] = YAxis.y;
		pOut->m[ 2 ][ 1 ] = YAxis.z;
		pOut->m[ 3 ][ 1 ] = -Dot( YAxis, *pEye );

		pOut->m[ 0 ][ 2 ] = ZAxis.x;
		pOut->m[ 1 ][ 2 ] = ZAxis.y;
		pOut->m[ 2 ][ 2 ] = ZAxis.z;
		pOut->m[ 3 ][ 2 ] = -Dot( ZAxis, *pEye );

		pOut->m[ 3 ][ 3 ] = 1.f;
		return pOut;
	}

	D3DXMATRIX * WINAPI D3DXMatrixPerspectiveFovLH( D3DXMATRIX * pOut, float FovY, float Aspect, float Zn, float Zf )
	{
		if ( !pOut )
			return pOut;

		const float YScale = 1.f / std::tan( FovY * 0.5f );
		const float XScale = YScale / Aspect;
		const float DepthScale = Zf / ( Zf - Zn );

		std::memset( pOut, 0, sizeof( D3DXMATRIX ) );
		pOut->m[ 0 ][ 0 ] = XScale;
		pOut->m[ 1 ][ 1 ] = YScale;
		pOut->m[ 2 ][ 2 ] = DepthScale;
		pOut->m[ 2 ][ 3 ] = 1.f;
		pOut->m[ 3 ][ 2 ] = -Zn * DepthScale;
		return pOut;
	}

}
