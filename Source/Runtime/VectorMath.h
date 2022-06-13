#pragma once

#include <xmmintrin.h>
#include <emmintrin.h>
#include <cmath>

// hehehe
#define M_PI 3.14159265358979323846

namespace Runtime
{
	namespace Math
	{
		struct Vector
		{
			Vector() = default;
			Vector(const Vector& other) = default;
			Vector(Vector&& other) = default;
			Vector& operator=(const Vector& b) = default;
			Vector& operator=(Vector&& b) = default;

			Vector(float x, float y, float z, float w = 0.0f)
			{
				LoadValues(x, y, z, w);
			}

			void LoadValues(float x, float y, float z, float w)
			{
				value = _mm_setr_ps(x, y, z, w);
			}

			Vector& operator+=(const Vector& b)
			{
				*this = Add(*this, b);
				return *this;
			}

			Vector operator+(const Vector& b) const
			{
				Vector r = Add(*this, b);
				return r;
			}

			Vector& operator-=(const Vector& b)
			{
				*this = Sub(*this, b);
				return *this;
			}

			Vector operator-(const Vector& b) const
			{
				Vector r = Sub(*this, b);
				return r;
			}
			
			float Length()
			{
				__m128 squared = _mm_mul_ps(value, value);
				float values[4];
				_mm_store_ps(values, squared);
				float sum = values[0] + values[1] + values[2] + values[3];
				return sqrt(sum);
			}

			static Vector Add(Vector a, Vector b)
			{
				Vector r;
				r.value = _mm_add_ps(a.value, b.value);
				return r;
			}

			static Vector Sub(Vector a, Vector b)
			{
				Vector r;
				r.value = _mm_sub_ps(a.value, b.value);
				return r;
			}
			
			__m128 value;
		};

		struct Mat4
		{
			Mat4() = default;
			Mat4(const Mat4& other) = default;
			Mat4(Mat4&& other) = default;

			Mat4& operator=(const Mat4& b) = default;
			Mat4& operator=(Mat4&& b) = default;

			Mat4& operator*=(const Mat4& b)
			{
				*this = Multiply(*this, b);
				return *this;
			}

			Mat4 operator*(const Mat4& b)
			{
				*this = Multiply(*this, b);
				return *this;
			}

			static Mat4 Identity()
			{
				Mat4 mat;
				mat.c1 = _mm_setr_ps(1.0f, 0.0f, 0.0f, 0.0f);
				mat.c2 = _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f);
				mat.c3 = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
				mat.c4 = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
				return mat;
			}

			static Mat4 Multiply(Mat4 a, Mat4 b)
			{
				Mat4 r;
				{
					__m128 m1 = _mm_shuffle_ps(a.c1, a.c1, _MM_SHUFFLE(0, 0, 0, 0));
					__m128 m2 = _mm_shuffle_ps(a.c1, a.c1, _MM_SHUFFLE(1, 1, 1, 1));
					__m128 m3 = _mm_shuffle_ps(a.c1, a.c1, _MM_SHUFFLE(2, 2, 2, 2));
					__m128 m4 = _mm_shuffle_ps(a.c1, a.c1, _MM_SHUFFLE(3, 3, 3, 3));

					m1 = _mm_mul_ps(m1, b.c1);
					m2 = _mm_mul_ps(m2, b.c2);
					m3 = _mm_mul_ps(m3, b.c3);
					m4 = _mm_mul_ps(m4, b.c4);

					__m128 a1 = _mm_add_ps(m1, m3);
					__m128 a2 = _mm_add_ps(m2, m4);

					r.c1 = _mm_add_ps(a1, a2);
				}

				{
					__m128 m1 = _mm_shuffle_ps(a.c2, a.c2, _MM_SHUFFLE(0, 0, 0, 0));
					__m128 m2 = _mm_shuffle_ps(a.c2, a.c2, _MM_SHUFFLE(1, 1, 1, 1));
					__m128 m3 = _mm_shuffle_ps(a.c2, a.c2, _MM_SHUFFLE(2, 2, 2, 2));
					__m128 m4 = _mm_shuffle_ps(a.c2, a.c2, _MM_SHUFFLE(3, 3, 3, 3));

					m1 = _mm_mul_ps(m1, b.c1);
					m2 = _mm_mul_ps(m2, b.c2);
					m3 = _mm_mul_ps(m3, b.c3);
					m4 = _mm_mul_ps(m4, b.c4);

					__m128 a1 = _mm_add_ps(m1, m3);
					__m128 a2 = _mm_add_ps(m2, m4);

					r.c2 = _mm_add_ps(a1, a2);
				}

				{
					__m128 m1 = _mm_shuffle_ps(a.c3, a.c3, _MM_SHUFFLE(0, 0, 0, 0));
					__m128 m2 = _mm_shuffle_ps(a.c3, a.c3, _MM_SHUFFLE(1, 1, 1, 1));
					__m128 m3 = _mm_shuffle_ps(a.c3, a.c3, _MM_SHUFFLE(2, 2, 2, 2));
					__m128 m4 = _mm_shuffle_ps(a.c3, a.c3, _MM_SHUFFLE(3, 3, 3, 3));

					m1 = _mm_mul_ps(m1, b.c1);
					m2 = _mm_mul_ps(m2, b.c2);
					m3 = _mm_mul_ps(m3, b.c3);
					m4 = _mm_mul_ps(m4, b.c4);

					__m128 a1 = _mm_add_ps(m1, m3);
					__m128 a2 = _mm_add_ps(m2, m4);

					r.c3 = _mm_add_ps(a1, a2);
				}

				{
					__m128 m1 = _mm_shuffle_ps(a.c4, a.c4, _MM_SHUFFLE(0, 0, 0, 0));
					__m128 m2 = _mm_shuffle_ps(a.c4, a.c4, _MM_SHUFFLE(1, 1, 1, 1));
					__m128 m3 = _mm_shuffle_ps(a.c4, a.c4, _MM_SHUFFLE(2, 2, 2, 2));
					__m128 m4 = _mm_shuffle_ps(a.c4, a.c4, _MM_SHUFFLE(3, 3, 3, 3));

					m1 = _mm_mul_ps(m1, b.c1);
					m2 = _mm_mul_ps(m2, b.c2);
					m3 = _mm_mul_ps(m3, b.c3);
					m4 = _mm_mul_ps(m4, b.c4);

					__m128 a1 = _mm_add_ps(m1, m3);
					__m128 a2 = _mm_add_ps(m2, m4);

					r.c4 = _mm_add_ps(a1, a2);
				}
				return r;
			}

			static Mat4 Scale(float x, float y, float z)
			{
				Mat4 mat;
				mat.c1 = _mm_setr_ps(   x, 0.0f, 0.0f, 0.0f);
				mat.c2 = _mm_setr_ps(0.0f,    y, 0.0f, 0.0f);
				mat.c3 = _mm_setr_ps(0.0f, 0.0f,    z, 0.0f);
				mat.c4 = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
				return mat;
			}

			static Mat4 Scale(float s)
			{
				return Scale(s, s, s);
			}

			static Mat4 Scale(Vector t)
			{ 
				// note(caleb): we have to include the emmintrin header just for this, 
				// might look into replacing this
				__m128 xmask = _mm_castsi128_ps(_mm_setr_epi32(-1, 0, 0, 0));
				__m128 ymask = _mm_castsi128_ps(_mm_setr_epi32(0, -1, 0, 0));
				__m128 zmask = _mm_castsi128_ps(_mm_setr_epi32(0, 0, -1, 0));

				Mat4 mat;
				mat.c1 = _mm_and_ps(t.value, xmask);
				mat.c2 = _mm_and_ps(t.value, ymask);
				mat.c3 = _mm_and_ps(t.value, zmask);
				mat.c4 = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);
				return mat;
			}

			static Mat4 Translate(float x, float y, float z)
			{
				Mat4 mat;
				mat.c1 = _mm_setr_ps(1.0f, 0.0f, 0.0f, 0.0f);
				mat.c2 = _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f);
				mat.c3 = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
				mat.c4 = _mm_setr_ps(   x,    y,    z, 1.0f);
				return mat;
			}

			static Mat4 Translate(Vector v)
			{
				Mat4 mat = Identity();
				__m128 xyzmask = _mm_castsi128_ps(_mm_setr_epi32(-1, -1, -1, 0));
				__m128 value = _mm_and_ps(xyzmask, v.value);
				mat.c4 = _mm_or_ps(value, mat.c4);
				return mat;
			}

			static Mat4 Perspective(float width, float height, float fov, float near_plane, float far_plane)
			{
				float aspect_ratio = width / height;
				float y_scale = 1.0f / (float)tan((fov / 2.0f) * (M_PI / 180.0f)) * aspect_ratio;
				float x_scale = y_scale / aspect_ratio;
				float frustum_length = far_plane - near_plane;

				float z_scale = -((far_plane + near_plane) / frustum_length);
				float z_trans = -((2 * near_plane * far_plane) / frustum_length);

				Mat4 mat;
				mat.c1 = _mm_setr_ps(x_scale,    0.0f,    0.0f,  0.0f);
				mat.c2 = _mm_setr_ps(   0.0f, y_scale,    0.0f,  0.0f);
				mat.c3 = _mm_setr_ps(   0.0f,    0.0f, z_scale, -1.0f); // note(caleb): what does this -1 do?
				mat.c4 = _mm_setr_ps(   0.0f,    0.0f, z_trans,  0.0f);
				return mat;
			}

			static Mat4 RotateY(float d)
			{
				Mat4 mat = Identity();
				mat.c1 = _mm_setr_ps(cos(d), 0.0f, -sin(d), 0.0f);
				mat.c3 = _mm_setr_ps(sin(d), 0.0f,  cos(d), 0.0f);
				return mat;
			}

			static Mat4 RotateX(float d)
			{
				Mat4 mat = Identity();
				mat.c2 = _mm_setr_ps(0.0f, cos(d), sin(d), 0.0f);
				mat.c3 = _mm_setr_ps(0.0f, -sin(d), cos(d), 0.0f);
				return mat;
			}

			static Mat4 RotateZ(float d)
			{
				Mat4 mat = Identity();
				mat.c1 = _mm_setr_ps( cos(d), sin(d), 0.0f, 0.0f);
				mat.c2 = _mm_setr_ps(-sin(d), cos(d), 0.0f, 0.0f);
				return mat;
			}

			__m128 c1;
			__m128 c2;
			__m128 c3;
			__m128 c4;
		};
	}
}