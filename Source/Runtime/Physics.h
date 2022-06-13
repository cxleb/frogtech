#pragma once

#include "Runtime/VectorMath.h"

namespace Runtime
{
	namespace Physics
	{
		struct AABB
		{
			Math::Vector p;
			Math::Vector s;
		};

		inline bool IntersectAABB(const AABB& a, const AABB& b)
		{
			__m128 a1 = a.p.value;
			__m128 a2 = _mm_add_ps(a.p.value, a.s.value);
			__m128 b1 = b.p.value;
			__m128 b2 = _mm_add_ps(b.p.value, b.s.value);

			// Bullet Physics AABB intersection method
			// uses some intel intrinsic magic
			__m128 cmp = _mm_or_ps(_mm_cmplt_ps(b2, a1), _mm_cmplt_ps(a2, b1));
			// what the fuck is this
			int* pu = (int*)&cmp;
			return((pu[0] | pu[1] | pu[2]) == 0);
		}
	}
}