#pragma once
/*
#############################################################################
# WARNING: this code was generated - do not edit, your changes may get lost #
#############################################################################
Consider to edit ContourTracingGenerator.py and ContourTracingGeneratorTemplate.hpp instead.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

/*
 Fast Edge-Based Pixel Contour Tracing from Seed Point
=======================================================
*/

namespace FEPCT
{
#ifndef FEPCT_Assert
	namespace
	{
		void defaultErrorHandler(const char* failed_expression, const char* error_message, const char* function_name, const char* file_name, int line_number)
		{
			if (!error_message || !error_message[0])
				error_message = "FEPCT_Assert failed";
			printf("%s: %s in function %s: %s(%d)\n", error_message, failed_expression, function_name, file_name, line_number);
			exit(-1);
		}
	}
#define FEPCT_Assert(expr,msg) do { if(!!(expr)) ; else defaultErrorHandler(#expr, (msg), __func__, __FILE__, __LINE__ ); } while(0)
#endif

	struct Point
	{
		int x;
		int y;
	};

	struct Edge : Point
	{
		/*
		   Definition of direction
		                                   x    
		  +---------------------------------->  
		  |               (0, -1)               
		  |                  0                  
		  |                  ^                  
		  |                  |                  
		  |                  |                  
		  | (-1, 0) 3 <------+------> 1 (1, 0)  
		  |                  |                  
		  |                  |                  
		  |                  v                  
		  |                  2                  
		y |               (0, 1)                
		  v                                     
		*/
		int dir;
	};

	struct SeedInfo
	{
		Edge start;
		Edge stop;
		bool clockwise;  // can be derived from start and stop by checking image
	};

	template<typename TImage>
	int getClockwiseSign(Edge const& edge, TImage const& image)
	{
		/*
		+--------+--------+
		|        |        |
		|   P1   |   P2   |
		|        |        |
		+--------+--------+
		|        ^        |
		|   P3   | (x, y) |
		|        |        |
		+--------+--------+
		*/

		if (edge.dir == 0)
		{
			if (isSetP3 == isSetXY)
				return 0; // error
			return isSetXY
				? 1 /* clockwise */
				: -1 /* counterclockwise */;
		}
		else if (edge.dir == 1)
		{
			TODO;
		}
		else if (edge.dir == 2)
		{
			TODO;
		}
		else
		{
			assert(edge.dir == 3);
			TODO;
		}
	}

	template<typename TImage>
	void setSeed(SeedInfo& seed, Point foreground, Point background, TImage const& image)
	{
		TODO;
	}

	template<typename TImage, typename TContour>
	void findContour(TImage const& image, SeedInfo seed, TContour contour, bool do_suppress_border = false, int max_contour_length = 0)
	{
		// image properties
		const int width = image.cols;
		const int height = image.rows;
		FEPCT_Assert(width > 0 && height > 0, "image is empty");
		const int width_m1 = width - 1;
		const int height_m1 = height - 1;

		const uint8_t* data = image.ptr(0, 0);
		const int stride = image.ptr(1, 0) - data;
		FEPCT_Assert(image.ptr(0, 1) - data == 1, (image.ptr(1, 0) - data == 1 ? "image is not row-major order" : "pixel is not single byte"));

		// constants to address 8-connected neighbour pixels
		constexpr int off_00 = 0;
		constexpr int off_p0 = 1;
		constexpr int off_m0 = -1;
		const int off_0p = stride;
		const int off_0m = -stride;
		const int off_pp = off_p0 + off_0p;
		const int off_pm = off_p0 + off_0m;
		const int off_mp = off_m0 + off_0p;
		const int off_mm = off_m0 + off_0m;

		// tracing state
		int x = seed.start.x;
		int y = seed.start.y;
		int dir = seed.start.dir;
		FEPCT_Assert(0 <= x && x < width && 0 <= y && y < height, "seed pixel is outside of image");
		const uint8_t* pixel = &data[x + y * stride];
		bool has_in_edge = TODO; // contour on current pixel has edge inside of the image, i.e. not only edges at image border
		bool is_clockwise = seed.clockwise;
		FEPCT_Assert(getClockwiseSign(seed.start, image) == (is_clockwise ? 1 : -1), "seed start has inconsistent turning direction");
		FEPCT_Assert(getClockwiseSign(seed.stop, image) == (is_clockwise ? 1 : -1), "seed stop has inconsistent turning direction");
		int contour_length = 0;
		if (max_contour_length <= 0) // TODO
			max_contour_length = INT_MAX;

		if (is_clockwise)
		{
			do
			{
				if (dir == 0)
				{
					/*
					direction 0 basic clockwise rules:
					==================================
					
					                     rule 1:              rule 2:              rule 3:              
					                     +-------+-------+    +-------+-------+    +-------+-------+    
					                     |       |       |    |       ^       |    |       |       |    1: foreground
					                     |   1   |  0/1  |    |   0   |   1   |    |   0   |   0   |    0: background or border
					                     |  ???  |       |    |       |  ???  |    |       |  ???  |    /: alternative
					                     +<------+-------+    +-------+-------+    +-------+------>+    
					                     |       ^       |    |       ^       |    |       ^       |    (x,y): current pixel
					                     |   0   |   1   |    |   0   |   1   |    |   0   |   1   |    ???: pixel to be checked
					                     |       | (x,y) |    |       | (x,y) |    |       | (x,y) |    
					                     +-------+-------+    +-------+-------+    +-------+-------+    
					                     - turn left          - move ahead         - turn right
					                     - emit pixel (x,y)   - emit pixel (x,y)


					direction 0 clockwise rules with border checks:
					===============================================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       |       |    |       |       |    |       ^       |    |       |       |    1: foreground
					|   b   |   b   |    |   1   |  0/1  |    |  0/b  |   1   |    |  0/b  |   0   |    0: background
					|  ???  |  ???  |    |  ???  |       |    |       |  ???  |    |       |  ???  |    b: border outside of image
					+-------+------>+    +<------+-------+    +-------+-------+    +-------+------>+    /: alternative
					|       ^       |    |       ^       |    |       ^       |    |       ^       |
					|  0/b  |   1   |    |   0   |   1   |    |  0/b  |   1   |    |  0/b  |   1   |    (x,y): current pixel
					|       | (x,y) |    |       | (x,y) |    |       | (x,y) |    |       | (x,y) |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn right        => turn left         => move ahead        => turn right
					                     => emit pixel (x,y)  => emit pixel (x,y)

					if forward is border (rule 0)
					    turn right
					else if left is not border and forward-left pixel is foreground (rule 1)
					    emit current pixel
					    go to checked pixel
					    has_in_edge = true
					    turn left
					else if forward pixel is foreground (rule 2)
					    if not do_suppress_border or has_in_edge
					        emit current pixel
					    go to checked pixel
					    has_in_edge = left is not border
					else (rule 3)
					    turn right
					    has_in_edge = has_in_edge or left is not border
					*/

					// if forward is border (rule 0)
					if (y == 0)
					{
					    // turn right
					    dir = 1;
					}
					// else if left is not border and forward-left pixel is foreground (rule 1)
					else if (x != 0 && pixel[off_mm] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_mm;
					    --x;
					    --y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn left
					    dir = 3;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_0m] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length) // contour_length is the unsuppressed length
					        break;
					    // go to checked pixel
					    pixel += off_0m;
					    --y;
					    // has_in_edge = left is not border
					    has_in_edge = x != 0;
					}
					// else (rule 3)
					else
					{
					    // turn right
					    dir = 1;
					    // has_in_edge = has_in_edge or left is not border
					    if (!has_in_edge)
					        has_in_edge = x != 0;
					}

				}
				else if (dir == 1)
				{
					/*
					direction 1 clockwise rules:
					============================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       |       |    |       ^       |    |       |       |    |       |       |    1: foreground
					|  0/b  |   b   |    |   0   |   1   |    |  0/b  |  0/b  |    |  0/b  |  0/b  |    0: background
					|       |  ???  |    |       |  ???  |    |       |       |    |       |       |    b: border outside of image
					+------>+-------+    +------>+-------+    +------>+------>+    +------>+-------+    /: alternative
					|       |       |    |       |       |    |       |       |    |       |       |
					|   1   |   b   |    |   1   |  0/1  |    |   1   |   1   |    |   1   |   0   |    (x,y): current pixel
					| (x,y) v  ???  |    | (x,y) |       |    | (x,y) |  ???  |    | (x,y) v  ???  |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn right        => turn left         => move ahead        => turn right
					                     => emit pixel (x,y)  => emit pixel (x,y)
					*/

					// if forward is border (rule 0)
					if (x == width_m1)
					{
					    // turn right
					    dir = 2;
					}
					// else if left is not border and forward-left pixel is foreground (rule 1)
					else if (y != 0 && pixel[off_pm] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_pm;
					    ++x;
					    --y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn left
					    dir = 0;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_p0] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_p0;
					    ++x;
					    // has_in_edge = left is not border
					    has_in_edge = y != 0;
					}
					// else (rule 3)
					else
					{
					    // turn right
					    dir = 2;
					    // has_in_edge = has_in_edge or left is not border
					    if (!has_in_edge)
					        has_in_edge = y != 0;
					}

				}
				else if (dir == 2)
				{
					/*
					direction 2 clockwise rules:
					============================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       |       |    |       |       |    |       |       |    |       |       |    1: foreground
					|   1   |  0/b  |    |   1   |   0   |    |   1   |  0/b  |    |   1   |  0/b  |    0: background
					| (x,y) v       |    | (x,y) v       |    | (x,y) v       |    | (x,y) v       |    b: border outside of image
					+<------+-------+    +-------+------>+    +-------+-------+    +<------+-------+    /: alternative
					|       |       |    |       |       |    |       |       |    |       |       |
					|   b   |   b   |    |  0/1  |   1   |    |   1   |  0/b  |    |   0   |  0/b  |    (x,y): current pixel
					|  ???  |  ???  |    |       |  ???  |    |  ???  v       |    |  ???  |       |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn right        => turn left         => move ahead        => turn right
					                     => emit pixel (x,y)  => emit pixel (x,y)
					*/

					// if forward is border (rule 0)
					if (y == height_m1)
					{
					    // turn right
					    dir = 3;
					}
					// else if left is not border and forward-left pixel is foreground (rule 1)
					else if (x != width_m1 && pixel[off_pp] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_pp;
					    ++x;
					    ++y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn left
					    dir = 1;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_0p] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_0p;
					    ++y;
					    // has_in_edge = left is not border
					    has_in_edge = x != width_m1;
					}
					// else (rule 3)
					else
					{
					    // turn right
					    dir = 3;
					    // has_in_edge = has_in_edge or left is not border
					    if (!has_in_edge)
					        has_in_edge = x != width_m1;
					}

				}
				else
				{
					assert(dir == 3);
					/*
					direction 3 clockwise rules:
					============================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       ^       |    |       |       |    |       |       |    |       ^       |    1: foreground
					|   b   |   1   |    |  0/1  |   1   |    |   1   |   1   |    |   0   |   1   |    0: background
					|  ???  | (x,y) |    |       | (x,y) |    |  ???  | (x,y) |    |  ???  | (x,y) |    b: border outside of image
					+-------+<------+    +-------+<------+    +<------+<------+    +-------+<------+    /: alternative
					|       |       |    |       |       |    |       |       |    |       |       |
					|   b   |  0/b  |    |   1   |   0   |    |  0/b  |  0/b  |    |  0/b  |  0/b  |    (x,y): current pixel
					|  ???  |       |    |  ???  v       |    |       |       |    |       |       |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn right        => turn left         => move ahead        => turn right
					                     => emit pixel (x,y)  => emit pixel (x,y)
					*/

					// if forward is border (rule 0)
					if (x == 0)
					{
					    // turn right
					    dir = 0;
					}
					// else if left is not border and forward-left pixel is foreground (rule 1)
					else if (y != height_m1 && pixel[off_mp] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_mp;
					    --x;
					    ++y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn left
					    dir = 2;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_m0] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_m0;
					    --x;
					    // has_in_edge = left is not border
					    has_in_edge = y != height_m1;
					}
					// else (rule 3)
					else
					{
					    // turn right
					    dir = 0;
					    // has_in_edge = has_in_edge or left is not border
					    if (!has_in_edge)
					        has_in_edge = y != height_m1;
					}

				}
			}
			while ((x != seed.stop.x || y != seed.stop.y || dir != seed.stop.dir));
		}
		else
		{
			do
			{
				if (dir == 0)
				{
					/*
					direction 0 counterclockwise rules:
					===================================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       |       |    |       |       |    |       ^       |    |       |       |    1: foreground
					|   b   |   b   |    |  0/1  |   1   |    |   1   |  0/b  |    |   0   |  0/b  |    0: background
					|  ???  |  ???  |    |       |  ???  |    |  ???  |       |    |  ???  |       |    b: border outside of image
					+<------+-------+    +-------+------>+    +-------+-------+    +<------+-------+    /: alternative
					|       ^       |    |       ^       |    |       ^       |    |       ^       |
					|   1   |  0/b  |    |   1   |   0   |    |   1   |  0/b  |    |   1   |  0/b  |    (x,y): current pixel
					| (x,y) |       |    | (x,y) |       |    | (x,y) |       |    | (x,y) |       |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn left         => turn right        => move ahead        => turn left
					                     => emit pixel (x,y)  => emit pixel (x,y)

					if forward is border (rule 0)
					    turn left
					else if right is not border and forward-right pixel is foreground (rule 1)
					    emit current pixel
					    go to checked pixel
					    has_in_edge = true
					    turn right
					else if forward pixel is foreground (rule 2)
					    if not do_suppress_border or has_in_edge
					        emit current pixel
					    go to checked pixel
					    has_in_edge = left is not border
					else (rule 3)
					    turn left
					    has_in_edge = has_in_edge or right is not border
					*/

					// if forward is border (rule 0)
					if (y == 0)
					{
					    // turn left
					    dir = 3;
					}
					// else if right is not border and forward-right pixel is foreground (rule 1)
					else if (x != width_m1 && pixel[off_pm] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_pm;
					    ++x;
					    --y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn right
					    dir = 1;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_0m] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_0m;
					    --y;
					    // has_in_edge = left is not border
					    has_in_edge = x != width_m1;
					}
					// else (rule 3)
					else
					{
					    // turn left
					    dir = 3;
					    // has_in_edge = has_in_edge or right is not border
					    if (!has_in_edge)
					        has_in_edge = x != width_m1;
					}

				}
				else if (dir == 1)
				{
					/*
					direction 1 counterclockwise rules:
					===================================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       ^       |    |       |       |    |       |       |    |       ^       |    1: foreground
					|   1   |   b   |    |   1   |  0/1  |    |   1   |   1   |    |   1   |   0   |    0: background
					| (x,y) |  ???  |    | (x,y) |       |    | (x,y) |  ???  |    | (x,y) |  ???  |    b: border outside of image
					+------>+-------+    +------>+-------+    +------>+------>+    +------>+-------+    /: alternative
					|       |       |    |       |       |    |       |       |    |       |       |
					|  0/b  |   b   |    |   0   |   1   |    |  0/b  |  0/b  |    |  0/b  |  0/b  |    (x,y): current pixel
					|       |  ???  |    |       v  ???  |    |       |       |    |       |       |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn left         => turn right        => move ahead        => turn left
					                     => emit pixel (x,y)  => emit pixel (x,y)
					*/

					// if forward is border (rule 0)
					if (x == width_m1)
					{
					    // turn left
					    dir = 0;
					}
					// else if right is not border and forward-right pixel is foreground (rule 1)
					else if (y != height_m1 && pixel[off_pp] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_pp;
					    ++x;
					    ++y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn right
					    dir = 2;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_p0] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_p0;
					    ++x;
					    // has_in_edge = left is not border
					    has_in_edge = y != height_m1;
					}
					// else (rule 3)
					else
					{
					    // turn left
					    dir = 0;
					    // has_in_edge = has_in_edge or right is not border
					    if (!has_in_edge)
					        has_in_edge = y != height_m1;
					}

				}
				else if (dir == 2)
				{
					/*
					direction 2 counterclockwise rules:
					===================================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       |       |    |       |       |    |       |       |    |       |       |    1: foreground
					|  0/b  |   1   |    |   0   |   1   |    |  0/b  |   1   |    |  0/b  |   1   |    0: background
					|       v (x,y) |    |       v (x,y) |    |       v (x,y) |    |       v (x,y) |    b: border outside of image
					+-------+------>+    +<------+-------+    +-------+-------+    +-------+------>+    /: alternative
					|       |       |    |       |       |    |       |       |    |       |       |
					|   b   |   b   |    |   1   |  0/1  |    |  0/b  |   1   |    |  0/b  |   0   |    (x,y): current pixel
					|  ???  |  ???  |    |  ???  |       |    |       v  ???  |    |       |  ???  |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn left         => turn right        => move ahead        => turn left
					                     => emit pixel (x,y)  => emit pixel (x,y)
					*/

					// if forward is border (rule 0)
					if (y == height_m1)
					{
					    // turn left
					    dir = 1;
					}
					// else if right is not border and forward-right pixel is foreground (rule 1)
					else if (x != 0 && pixel[off_mp] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_mp;
					    --x;
					    ++y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn right
					    dir = 3;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_0p] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_0p;
					    ++y;
					    // has_in_edge = left is not border
					    has_in_edge = x != 0;
					}
					// else (rule 3)
					else
					{
					    // turn left
					    dir = 1;
					    // has_in_edge = has_in_edge or right is not border
					    if (!has_in_edge)
					        has_in_edge = x != 0;
					}

				}
				else
				{
					assert(dir == 3);
					/*
					direction 3 counterclockwise rules:
					===================================

					rule 0:              rule 1:              rule 2:              rule 3:
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					|       |       |    |       ^       |    |       |       |    |       |       |    1: foreground
					|   b   |  0/b  |    |   1   |   0   |    |  0/b  |  0/b  |    |  0/b  |  0/b  |    0: background
					|  ???  |       |    |  ???  |       |    |       |       |    |       |       |    b: border outside of image
					+-------+<------+    +-------+<------+    +<------+<------+    +-------+<------+    /: alternative
					|       |       |    |       |       |    |       |       |    |       |       |
					|   b   |   1   |    |  0/1  |   1   |    |   1   |   1   |    |   0   |   1   |    (x,y): current pixel
					|  ???  v (x,y) |    |       | (x,y) |    |  ???  | (x,y) |    |  ???  v (x,y) |    ???: pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+
					=> turn left         => turn right        => move ahead        => turn left
					                     => emit pixel (x,y)  => emit pixel (x,y)
					*/

					// if forward is border (rule 0)
					if (x == 0)
					{
					    // turn left
					    dir = 2;
					}
					// else if right is not border and forward-right pixel is foreground (rule 1)
					else if (y != 0 && pixel[off_mm] != 0)
					{
					    // emit current pixel
					    contour.emplace_back(x, y);
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_mm;
					    --x;
					    --y;
					    // has_in_edge = true
					    has_in_edge = true;
					    // turn right
					    dir = 0;
					}
					// else if forward pixel is foreground (rule 2)
					else if (pixel[off_m0] != 0)
					{
					    // if not do_suppress_border or has_in_edge
					    if (!do_suppress_border || has_in_edge)
					    {
					        // emit current pixel
					        contour.emplace_back(x, y);
					    }
					    if (++contour_length >= max_contour_length)
					        break;
					    // go to checked pixel
					    pixel += off_m0;
					    --x;
					    // has_in_edge = left is not border
					    has_in_edge = y != 0;
					}
					// else (rule 3)
					else
					{
					    // turn left
					    dir = 2;
					    // has_in_edge = has_in_edge or right is not border
					    if (!has_in_edge)
					        has_in_edge = y != 0;
					}

				}
			}
			while ((x != seed.stop.x || y != seed.stop.y || dir != seed.stop.dir));
		}

		if (contour_length == 0 && contour_length < max_contour_length)
		{
			// single isolated pixel
			contour.emplace_back(x, y);
		}
	}
}
