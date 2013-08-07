/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef VECTOR_HL1_H
#define VECTOR_HL1_H

//=========================================================
// 2DVector - used for many pathfinding and many other 
// operations that are treated as planar rather than 3d.
//=========================================================
class Vector2DHL1
{
public:
	inline Vector2DHL1(void)									{ }
	inline Vector2DHL1(float X, float Y)						{ x = X; y = Y; }
	inline Vector2DHL1 operator+(const Vector2DHL1& v)	const	{ return Vector2DHL1(x+v.x, y+v.y);	}
	inline Vector2DHL1 operator-(const Vector2DHL1& v)	const	{ return Vector2DHL1(x-v.x, y-v.y);	}
	inline Vector2DHL1 operator*(float fl)				const	{ return Vector2DHL1(x*fl, y*fl);	}
	inline Vector2DHL1 operator/(float fl)				const	{ return Vector2DHL1(x/fl, y/fl);	}
	
	inline float Length(void)						const	{ return sqrt(x*x + y*y );		}

	inline Vector2DHL1 Normalize ( void ) const
	{
		Vector2DHL1 vec2;

		float flLen = Length();
		if ( flLen == 0 )
		{
			return Vector2DHL1( 0, 0 );
		}
		else
		{
			flLen = 1 / flLen;
			return Vector2DHL1( x * flLen, y * flLen );
		}
	}

	vec_t_hl1	x, y;
};

inline float DotProduct(const Vector2DHL1& a, const Vector2DHL1& b) { return( a.x*b.x + a.y*b.y ); }
inline Vector2DHL1 operator*(float fl, const Vector2DHL1& v)	{ return v * fl; }

//=========================================================
// 3D Vector
//=========================================================
class VectorHL1					// same data-layout as engine's vec3_t,
{								//		which is a vec_t[3]
public:
	// Construction/destruction
	inline VectorHL1(void)								{ }
	inline VectorHL1(float X, float Y, float Z)		{ x = X; y = Y; z = Z;						}
	//inline Vector(double X, double Y, double Z)		{ x = (float)X; y = (float)Y; z = (float)Z;	}
	//inline Vector(int X, int Y, int Z)				{ x = (float)X; y = (float)Y; z = (float)Z;	}
	inline VectorHL1(const VectorHL1& v)					{ x = v.x; y = v.y; z = v.z;				} 
	inline VectorHL1(float rgfl[3])					{ x = rgfl[0]; y = rgfl[1]; z = rgfl[2];	}

	// Operators
	inline VectorHL1 operator-(void) const				{ return VectorHL1(-x,-y,-z);				}
	inline int operator==(const VectorHL1& v) const	{ return x==v.x && y==v.y && z==v.z;	}
	inline int operator!=(const VectorHL1& v) const	{ return !(*this==v);					}
	inline VectorHL1 operator+(const VectorHL1& v) const	{ return VectorHL1(x+v.x, y+v.y, z+v.z);	}
	inline VectorHL1 operator-(const VectorHL1& v) const	{ return VectorHL1(x-v.x, y-v.y, z-v.z);	}
	inline VectorHL1 operator*(float fl) const			{ return VectorHL1(x*fl, y*fl, z*fl);		}
	inline VectorHL1 operator/(float fl) const			{ return VectorHL1(x/fl, y/fl, z/fl);		}
	
	// Methods
	inline void CopyToArray(float* rgfl) const		{ rgfl[0] = x, rgfl[1] = y, rgfl[2] = z; }
	inline float Length(void) const					{ return sqrt(x*x + y*y + z*z); }
	operator float *()								{ return &x; } // Vectors will now automatically convert to float * when needed
	operator const float *() const					{ return &x; } // Vectors will now automatically convert to float * when needed
	inline VectorHL1 Normalize(void) const
	{
		float flLen = Length();
		if (flLen == 0) return VectorHL1(0,0,1); // ????
		flLen = 1 / flLen;
		return VectorHL1(x * flLen, y * flLen, z * flLen);
	}

	inline VectorHL1 Make2D ( void ) const
	{
		VectorHL1	Vec2;

		Vec2.x = x;
		Vec2.y = y;

		return Vec2;
	}
	inline float Length2D(void) const					{ return sqrt(x*x + y*y); }

	// Members
	vec_t_hl1 x, y, z;
};
inline VectorHL1 operator*(float fl, const VectorHL1& v)	{ return v * fl; }
inline float DotProduct(const VectorHL1& a, const VectorHL1& b) { return(a.x*b.x+a.y*b.y+a.z*b.z); }
inline VectorHL1 CrossProduct(const VectorHL1& a, const VectorHL1& b) { return VectorHL1( a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x ); }



#endif