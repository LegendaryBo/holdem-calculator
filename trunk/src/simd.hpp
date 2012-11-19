/**
 * @defgroup SIMD SIMD Operations
 */

#ifndef SIMD_HPP
#define SIMD_HPP

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <emmintrin.h>

namespace simd {

/// @ingroup SIMD
/// @defgroup SIMD_Types Data Types
// @{

/// Defines the default vector type for combinations of underlying data type 
/// and vector size.
template <class T, int N> struct default_vector_type { };
template <> struct default_vector_type<int8_t,16>  { typedef __m128i type; };
template <> struct default_vector_type<uint8_t,16> { typedef __m128i type; };
template <> struct default_vector_type<int16_t,8>  { typedef __m128i type; };
template <> struct default_vector_type<uint16_t,8> { typedef __m128i type; };
template <> struct default_vector_type<int32_t,4>  { typedef __m128i type; };
template <> struct default_vector_type<uint32_t,4> { typedef __m128i type; };

/**
 * Represents a fixed-size vector of elements wrapped in an SIMD data type.
 *
 * The supported type and size of an SIMD vector depends on the hardware
 * architecture. Each supported combination of data type and size is 
 * specialized as a separate class. Currently only a subset of the features
 * are implemented for Intel SSE2 instruction set.
 */
template <class T, int N, class V = typename default_vector_type<T,N>::type>
struct simd_t 
{
	// static_assert(false, "Not implemented");
};

/**
 * Represents a fixed slice of an SIMD vector. The slice is defined by the
 * template arguments @c Begin, @c End and @c Step as <code>Begin, Begin+Step,
 * Begin+2*Step, ..., End-1</code>.
 */
template <class V, int Begin, int End, int Step = 1>
struct simd_slice_t
{
	V array;
	simd_slice_t(const V &x) : array(x) { }
};

/// Creates a slice of a given vector.
template <int Begin, int End, int Step, class V>
inline simd_slice_t<V,Begin,End,Step> make_slice(const V &x)
{
	return simd_slice_t<V,Begin,End,Step>(x);
}

/// Creates a slice of a given vector with Step set to one.
template <int Begin, int End, class V>
inline simd_slice_t<V,Begin,End,1> make_slice(const V &x)
{
	return simd_slice_t<V,Begin,End,1>(x);
}

/// Specialized SIMD vector for XMM integral vector.
template <class T, int N>
class simd_t<T, N, __m128i>
{
	/// @cond FALSE
	static_assert(sizeof(T)*N == 16, "Size of vector must be 128 bits.");
	/// @endcond

	static __m128i fill(int8_t b) { return _mm_set1_epi8(b); }
	static __m128i fill(uint8_t b) { return _mm_set1_epi8(b); }

	__m128i value;

public:

	/// Constructs a vector of zeros.
	simd_t() : value(_mm_setzero_si128()) { }

	/// Constructs a vector from the native SIMD type.
	simd_t(__m128i v) : value(v) { } 

	/// Constructs a vector with elements initialized to a given value.
	simd_t(T elem) : value(fill(elem)) { }

	/// Converts the vector to the native SIMD type.
	operator __m128i() const { return value; } 

	/// Returns a vector of zeros.
	static simd_t zero() { return _mm_setzero_si128(); } 
};

/// Shortcuts for 128-bit XMM integral vectors.
typedef simd_t<int8_t,  16, __m128i> xmm_i8;
typedef simd_t<uint8_t, 16, __m128i> xmm_u8;
typedef simd_t<int16_t,  8, __m128i> xmm_i16;
typedef simd_t<uint16_t, 8, __m128i> xmm_u16;
typedef simd_t<int32_t,  4, __m128i> xmm_i32;
typedef simd_t<uint32_t, 4, __m128i> xmm_u32;

// @}

/// SIMD boolean vectors (used as test results)...


/// @ingroup SIMD
/// @defgroup SIMD_Bitwise Bitwise Operations
// @{

/// Bitwise AND for XMM integral vectors.
template <class T, int N>
inline simd_t<T,N,__m128i>
operator & (const simd_t<T,N,__m128i> &a, const simd_t<T,N,__m128i> &b) 
{ 
	return _mm_and_si128(a, b);
}

/// Bitwise AND-assign for XMM integral vectors.
template <class T, int N>
inline simd_t<T,N,__m128i> &
operator &= (simd_t<T,N,__m128i> &a, const simd_t<T,N,__m128i> &b)
{
	return a = (a & b);
}

/// Bitwise AND-assign.
template <class T, int N>
inline simd_t<T,N,__m128i> &
operator &= (simd_t<T,N,__m128i> &a, T b)
{
	return a &= simd_t<T,N,__m128i>(b);
}

/// Bitwise-OR.
template <class T, int N>
inline simd_t<T,N,__m128i>
operator | (const simd_t<T,N,__m128i> &a, const simd_t<T,N,__m128i> &b) 
{ 
	return _mm_or_si128(a, b);
} 

/// Bitwise OR-assign.
template <class T, int N>
inline simd_t<T,N,__m128i> &
operator |= (simd_t<T,N,__m128i> &a, const simd_t<T,N,__m128i> &b)
{
	return a = (a | b);
}

/// Bitwise OR-assign.
template <class T, int N>
inline simd_t<T,N,__m128i> &
operator |= (simd_t<T,N,__m128i> &a, T b)
{
	return a |= simd_t<T,N,__m128i>(b);
}

// @}

// Bit-wise operations.
inline xmm_i8 operator & (const xmm_i8 &a, const xmm_i8 &b) { return _mm_and_si128(a, b); }

/// Element-wise arithmetics.
inline xmm_i8 operator + (const xmm_i8 &a, const xmm_i8 &b) { return _mm_add_epi8(a, b); }
inline xmm_i8 operator - (const xmm_i8 &a, const xmm_i8 &b) { return _mm_sub_epi8(a, b); }

/// Element-wise arithmetic-assign.
inline xmm_i8& operator += (xmm_i8 &a, const xmm_i8 &b) { return a = a + b; }
inline xmm_i8& operator -= (xmm_i8 &a, const xmm_i8 &b) { return a = a - b; }

/// Element-wise comparison for equality.
inline xmm_i8 operator == (const xmm_i8 &a, const xmm_i8 &b) { return _mm_cmpeq_epi8(a, b); }
inline xmm_u8 operator == (const xmm_u8 &a, const xmm_u8 &b) { return _mm_cmpeq_epi8(a, b); }

/// Element-wise comparison for greater than.
inline xmm_i8 operator > (const xmm_i8 &a, const xmm_i8 &b) { return _mm_cmpgt_epi8(a, b); }
//inline xmm_i8 operator > (const xmm_i8 &a, int8_t b) { return _mm_cmpgt_epi8(a, b); }

/// Creates a 16-bit mask from the most significant bits of the 16 bytes,
/// and zero extends the upper bits in the result.
/// @ingroup SIMD
inline int byte_mask(const xmm_i8 &a) { return _mm_movemask_epi8(a); }
inline int byte_mask(const xmm_u8 &a) { return _mm_movemask_epi8(a); }
inline int GetMask(const xmm_i8 &a) { return byte_mask(a); }

/// Shift elements (not bits) left.
template <int Count> inline 
xmm_u8 shift_elements_left(const xmm_u8 &a) { return _mm_slli_si128(a, Count); }

/// Shift elements (not bits) right.
template <int Count> inline
xmm_u8 shift_elements_right(const xmm_u8 &a) { return _mm_srli_si128(a, Count); }

/// Composite.

/// Fills the left @c Count bytes with <code>b</code>.
/// @ingroup SIMD
template <int Count> inline
xmm_u8 fill_left(uint8_t b) { return _mm_slli_si128(_mm_set1_epi8(b), 16-Count); }

/// Fills the right @c Count bytes with <code>b</code>.
/// @ingroup SIMD
template <int Count> inline
xmm_u8 fill_right(uint8_t b) { return _mm_srli_si128(_mm_set1_epi8(b), 16-Count); }

/// Keeps @c Count bytes on the right, and sets the remaining bytes on 
/// the left to zero.
/// @ingroup SIMD
template <int Count> 
inline xmm_i8 keep_right(const xmm_i8 &a) 
{
	return _mm_srli_si128(_mm_slli_si128(a,16-Count),16-Count); 
}

/// Keeps @c Count bytes on the right, and sets the remaining bytes on 
/// the left to zero.
/// @ingroup SIMD
template <int Count> 
inline xmm_u8 keep_right(const xmm_u8 &a) 
{
	return _mm_srli_si128(_mm_slli_si128(a,16-Count),16-Count); 
}

/// Keeps @c Count bytes on the left, and sets the remaining bytes on 
/// the right to zero.
template <int Count>
xmm_u8 keep_left(const xmm_u8 &a)
{
	return _mm_slli_si128(_mm_srli_si128(a,16-Count),16-Count);
}

/// Extracts the selected 16-bit word, and zero extends the result.
template <int Index> 
inline int extract(const xmm_u16 &a)
{
	static_assert(Index >= 0 && Index <= 7, "Index must be between 0 and 7");
	return _mm_extract_epi16(a, Index);
}

/// Computes the absolute difference of the upper 8 unsigned bytes
/// and stores the result in the upper quadword; computes the same
/// for the lower 8 unsigned bytes and stores the result in the 
/// lower quadword.
/// @ingroup SIMD
inline xmm_u16 sad(const xmm_u8 &a, const xmm_u8 &b) { return _mm_sad_epu8(a, b); }

/// Element-wise minimum.
/// @ingroup SIMD
inline xmm_u8 min(const xmm_u8 &a, const xmm_u8 &b) { return _mm_min_epu8(a, b); }

#if 0
template <int Begin, int End, int Step, class V>
inline int sum(const simd_slice_t<V,Begin,End,Step> &x)
{
	static_assert(false, "Not implemented");
}
#endif

inline int sum(const simd_slice_t<xmm_u8,0,16,1> &x)
{
	const xmm_u16 &t = sad(x.array, xmm_u8::zero());
	return extract<0>(t) + extract<4>(t);
}

inline int sum(const simd_slice_t<xmm_u8,0,8,1> &x)
{
	const xmm_u16 &t = sad(x.array, xmm_u8::zero());
	return extract<0>(t);
}

inline int sum(const simd_slice_t<xmm_u8,8,16,1> &x)
{
	const xmm_u16 &t = sad(x.array, xmm_u8::zero());
	return extract<4>(t);
}

#if 0
/// Returns the sum of elements.
/// @ingroup SIMD
inline unsigned int sum(const simd_t<uint8_t,16> &a)
{
	return sum<0,16>(a);
}
#endif

} // namespace simd


#if 0
#include <tmmintrin.h>

namespace util { namespace simd {
	
#define SIMD_REQUIRE(version)

SIMD_REQUIRE("SSSE3")
inline simd_t<uint8_t,16> 
shuffle(const simd_t<uint8_t,16> &a, const simd_t<int8_t,16> &perm)
{
	return _mm_shuffle_epi8(a, perm);
}

} } // namespace util::simd
#endif

#endif // SIMD_HPP
