/**
 * @defgroup CompilerIntrinsic Compiler Intrinsics
 * Portable implementation of compiler intrinsics with a uniform signature.
 */

#ifndef INTRINSIC_HPP
#define INTRINSIC_HPP

#ifdef _WIN32
#include <intrin.h>
#else
//#include <x86intrin.h>
#endif

namespace intrinsic {

// @cond DETAILS

/// Implements an intrinsic function by a call to another function with the
/// same argument type.
#define DELEGATE_INTRINSIC(return_type, name, type, delegate_name) \
	inline return_type name(type x) { return delegate_name(x); }

/// Implements an intrinsic function by a call to the intrinsic function of
/// the same name but with a cast argument type.
#define DELEGATE_INTRINSIC_CAST(return_type, name, type, delegate_type) \
	inline return_type name(type x) { return name((delegate_type)x); }

// @endcond

/// Returns the number of bits set in an integer.
template <class T>
inline int pop_count(T x)
{
	int n = 0;
	for (; x; x &= (x - T(1)))
		++n;
	return n;
}

// @cond DETAILS
#if defined(_WIN32)
#if HAVE_POPCOUNT
DELEGATE_INTRINSIC(int, pop_count, unsigned short, __popcnt16)
DELEGATE_INTRINSIC(int, pop_count, unsigned int,   __popcnt)
DELEGATE_INTRINSIC_CAST(int, pop_count, unsigned char, unsigned int)
DELEGATE_INTRINSIC_CAST(int, pop_count, unsigned long, unsigned int)
#if defined(_WIN64)
DELEGATE_INTRINSIC(int, pop_count, unsigned long long, __popcnt64)
#else // _WIN64
inline int pop_count(unsigned long long x) 
{
	return __popcnt((unsigned long)(x >> 32)) + _popcnt((unsigned long)x);
}
#endif // defined(_WIN64)
#endif // HAVE_POPCOUNT
#else  // defined(_WIN32)
DELEGATE_INTRINSIC(int, pop_count, unsigned int, __builtin_popcount)
DELEGATE_INTRINSIC(int, pop_count, unsigned long, __builtin_popcountl)
DELEGATE_INTRINSIC(int, pop_count, unsigned long long, __builtin_popcountll)
DELEGATE_INTRINSIC_CAST(int, pop_count, unsigned char,  unsigned int)
DELEGATE_INTRINSIC_CAST(int, pop_count, unsigned short, unsigned int)
#endif // defined(_WIN32)

DELEGATE_INTRINSIC_CAST(int, pop_count, char,        unsigned char)
DELEGATE_INTRINSIC_CAST(int, pop_count, signed char, unsigned char)
DELEGATE_INTRINSIC_CAST(int, pop_count, short,       unsigned short)
DELEGATE_INTRINSIC_CAST(int, pop_count, int,         unsigned int)
DELEGATE_INTRINSIC_CAST(int, pop_count, long,        unsigned long)
DELEGATE_INTRINSIC_CAST(int, pop_count, long long,   unsigned long long)
// @endcond

/// Returns the (zero-based) position of the least significant bit set
/// in an integer. If the integer is zero, the return value is undefined.
template <class T>
inline int bit_scan_forward(T x)
{
	if (x == T(0))
		return -1;

	int i = 0;
	while (!(x & T(1)))
	{
		x >>= 1;
		++i;
	}
	return i;
}

// @cond DETAILS
#if defined(_WIN32)
inline int bit_scan_forward(unsigned long x)
{
	unsigned long pos = 0;
	_BitScanForward(&pos, x);
	return pos;
}
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, unsigned int,   unsigned long)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, unsigned short, unsigned long)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, unsigned char,  unsigned long)
#if defined(_WIN64)
inline int bit_scan_forward(unsigned long long x)
{
	unsigned long pos = 0;
	_BitScanForward64(&pos, x);
	return pos;
}
#else  // defined(_WIN64)
inline int bit_scan_forward(unsigned long long x)
{
	if ((unsigned long)x == 0)
		return bit_scan_forward((unsigned long)(x >> 32)) + 32;
	else
		return bit_scan_forward((unsigned long)x);
}
#endif // defined(_WIN64)
#else  // defined(_WIN32)
DELEGATE_INTRINSIC(int, bit_scan_forward, unsigned int, __builtin_ctz)
DELEGATE_INTRINSIC(int, bit_scan_forward, unsigned long, __builtin_ctzl)
DELEGATE_INTRINSIC(int, bit_scan_forward, unsigned long long, __builtin_ctzll)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, unsigned char, unsigned int)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, unsigned short, unsigned int)
#endif // defined(_WIN32)

DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, char,        unsigned char)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, signed char, unsigned char)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, short,       unsigned short)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, int,         unsigned int)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, long,        unsigned long)
DELEGATE_INTRINSIC_CAST(int, bit_scan_forward, long long,   unsigned long long)
// @endcond

/// Returns the (zero-based) position of the most significant bit set
/// in an integer. If the integer is zero, the return value is undefined.
template <class T>
inline int bit_scan_reverse(T x)
{
	if (x == T(0))
		return -1;

	const T bit = T(1) << (sizeof(T)*8-1);

	int i = sizeof(T)*8-1;
	while (!(x & bit))
	{
		x <<= 1;
		--i;
	}
	return i;
}

// @cond DETAILS
#if defined(_WIN32)
inline int bit_scan_reverse(unsigned long x)
{
	unsigned long pos = 0;
	_BitScanReverse(&pos, x);
	return pos;
}
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, unsigned int,   unsigned long)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, unsigned short, unsigned long)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, unsigned char,  unsigned long)
#if defined(_WIN64)
inline int bit_scan_reverse(unsigned long long x)
{
	unsigned long pos = 0;
	_BitScanReverse64(&pos, x);
	return pos;
}
#else  // defined(_WIN64)
inline int bit_scan_reverse(unsigned long long x)
{
	if ((unsigned long)(x >> 32) == 0)
		return bit_scan_reverse((unsigned long)x);
	else
		return bit_scan_reverse((unsigned long)(x >> 32)) + 32;
}
#endif // defined(_WIN64)
#else  // defined(_WIN32)
DELEGATE_INTRINSIC(int, bit_scan_reverse, unsigned int, __builtin_clz)
DELEGATE_INTRINSIC(int, bit_scan_reverse, unsigned long, __builtin_clzl)
DELEGATE_INTRINSIC(int, bit_scan_reverse, unsigned long long, __builtin_clzll)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, unsigned char, unsigned int)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, unsigned short, unsigned int)
#endif // defined(_WIN32)

DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, char,        unsigned char)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, signed char, unsigned char)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, short,       unsigned short)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, int,         unsigned int)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, long,        unsigned long)
DELEGATE_INTRINSIC_CAST(int, bit_scan_reverse, long long,   unsigned long long)
// @endcond

} // namespace intrinsic

#endif // INTRINSIC_HPP
