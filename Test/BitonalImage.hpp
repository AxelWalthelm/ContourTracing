#pragma once
//
// Copyright 2024 Axel Walthelm
//
#include <algorithm>
#include <type_traits>
#include <cstdint>
#include <limits>
#ifndef IMAGE1BPP_USE_INTRINSICS
#define IMAGE1BPP_USE_INTRINSICS 0
#endif
#if defined(_WIN32) && IMAGE1BPP_USE_INTRINSICS
#include <intrin.h>
#endif


// A simple bitonal image format with 1 bit per pixel and optional padding at the end of lines
template<typename TAddr = unsigned int> // type for address calculations - unsigned may be faster than int? or size_t?
class BitonalImage
{
	static inline
	bool bittest(uint8_t const *bits, int32_t bit_index)
	{
		return (bits[bit_index >> 3] & (uint8_t)(1 << (bit_index & 7))) != 0;
	}

	static inline
	bool bittest(uint8_t const *bits, uint32_t bit_index)
	{
		return (bits[bit_index >> 3] & (uint8_t)(1 << (bit_index & 7))) != 0;
	}

	static inline
	bool bittest(uint8_t const *bits, int64_t bit_index)
	{
		return (bits[bit_index >> 3] & (uint8_t)(1 << (bit_index & 7))) != 0;
	}

	static inline
	bool bittest(uint8_t const *bits, uint64_t bit_index)
	{
		return (bits[bit_index >> 3] & (uint8_t)(1 << (bit_index & 7))) != 0;
	}

	static inline
	void bitset(uint8_t *bits, int32_t bit_index, bool value)
	{
		if (value)
			bits[bit_index >> 3] |= (uint8_t)(1 << (bit_index & 7));
		else
			bits[bit_index >> 3] &= (uint8_t)~(1 << (bit_index & 7));
	}

	static inline
	void bitset(uint8_t *bits, uint32_t bit_index, bool value)
	{
		if (value)
			bits[bit_index >> 3] |= (uint8_t)(1 << (bit_index & 7));
		else
			bits[bit_index >> 3] &= (uint8_t)~(1 << (bit_index & 7));
	}

	static inline
	void bitset(uint8_t *bits, int64_t bit_index, bool value)
	{
		if (value)
			bits[bit_index >> 3] |= (uint8_t)(1 << (bit_index & 7));
		else
			bits[bit_index >> 3] &= (uint8_t)~(1 << (bit_index & 7));
	}

	static inline
	void bitset(uint8_t *bits, uint64_t bit_index, bool value)
	{
		if (value)
			bits[bit_index >> 3] |= (uint8_t)(1 << (bit_index & 7));
		else
			bits[bit_index >> 3] &= (uint8_t)~(1 << (bit_index & 7));
	}

public:
	const int width = 0;
	const int height = 0;
	const int stride = 0;
	uint8_t* const data;

	BitonalImage(int width, int height, int stride = 0) :
		width(width),
		height(height),
		stride(std::max(width, stride)),
		data(reinterpret_cast<uint8_t*>(new uint64_t[(std::max(width, stride) * height + 7) / 8]))
	{}

	~BitonalImage()
	{
		delete[] reinterpret_cast<uint64_t*>(data);
	}


	bool IsInside(int x, int y) const { return 0 <= x && x < width && 0 <= y && y < height; }

	bool GetPixelUnchecked(int x, int y) const { return bittest(data, (TAddr)y * (TAddr)stride + (TAddr)x); }

	bool GetPixel(int x, int y, bool border_value = false) const { return IsInside(x, y) ? GetPixelUnchecked(x, y) : border_value; }

	void SetPixelUnchecked(int x, int y, bool value) { bitset(data, (TAddr)y * (TAddr)stride + (TAddr)x, value); }

	void SetPixel(int x, int y, bool value)
	{
		if (IsInside(x, y))
			SetPixelUnchecked(x, y, value);
	}

	void CopyFrom(uint8_t* image, int width, int height, int stride = 0)
	{
		if (stride < width)
			stride = width;

		width = std::min(width, this->width);
		height = std::min(height, this->height);

		// TBD: should this double-loop be optimized manually?
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				SetPixelUnchecked(x, y, image[y * stride + x] != 0);
	}

	bool IsEqual(uint8_t* image, int width, int height, int stride = 0)
	{
		if (stride < width)
			stride = width;

		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
				if ((GetPixelUnchecked(x, y) != 0) != (image[y * stride + x] != 0))
					return false;

		return true;
	}


	void PrintDimensions() const
	{
		printf("%dx%d bitonal image stride=%d\n", width, height, stride);
	}

	void PrintContent() const
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				bool value = GetPixelUnchecked(x, y);
				printf("%c", value ? '*' : '.');
			}
			printf("\n");
		}
	}

	void Print() const
	{
		PrintDimensions();
		PrintContent();
	}
};
