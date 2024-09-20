#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

/*
o__INTRODUCTION__o
*/

namespace o__NAMESPACE__o
{
#ifndef o__NAMESPACE__o_Assert
	namespace
	{
		void defaultErrorHandler(const char* failed_expression, const char* error_message, const char* function_name, const char* file_name, int line_number)
		{
			if (!error_message || !error_message[0])
				error_message = "o__NAMESPACE__o_Assert failed";
			printf("%s: %s in function %s: %s(%d)\n", error_message, failed_expression, function_name, file_name, line_number);
			exit(-1);
		}
	}
#define o__NAMESPACE__o_Assert(expr,msg) do { if(!!(expr)) ; else defaultErrorHandler(#expr, (msg), __func__, __FILE__, __LINE__ ); } while(0)
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

	template<TImage>
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

	template<TImage>
	void setSeed(SeedInfo& seed, Point foreground, Point background, TImage const& image)
	{
		TODO;
	}

	template<TImage, TContour>
	void findContour(TImage const& image, SeedInfo seed, TContour contour, bool do_suppress_border = false, int max_contour_length = 0)
	{
		// image properties
		const int width = image.cols;
		const int height = image.rows;
		o__NAMESPACE__o_Assert(width > 0 && height > 0, "image is empty");
		const int width_m1 = width - 1;
		const int height_m1 = height - 1;

		const uint8_t* data = image.ptr(0, 0);
		const int stride = image.ptr(1, 0) - data;
		o__NAMESPACE__o_Assert((image.ptr(0, 1) - data) == 1, (image.ptr(1, 0) - data) == 1 ? "image is not row-major order" : "pixel is not single byte");

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
		o__NAMESPACE__o_Assert(0 <= x && x < width && 0 <= y && y < height, "seed pixel is outside of image");
		const uint8_t* pixel = &data[x + y * stride];
		bool has_in_edge = TODO; // contour on current pixel has edge inside of the image, i.e. not only edges at image border
		bool is_clockwise = seed.clockwise;
		o__NAMESPACE__o_Assert(getClockwiseSign(seed.start, image) == (is_clockwise ? 1 : -1), "seed start has inconsistent turning direction");
		o__NAMESPACE__o_Assert(getClockwiseSign(seed.stop, image) == (is_clockwise ? 1 : -1), "seed stop has inconsistent turning direction");
		int contour_length = 0;
		if (max_contour_length <= 0)
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
					                     | (???) |       |    |       | (???) |    |       | (???) |    /: alternative
					                     +<------+-------+    +-------+-------+    +-------+------>+    
					                     |       ^       |    |       ^       |    |       ^       |    
					                     |   0   |   1   |    |   0   |   1   |    |   0   |   1   |    
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
					|       | (x,y) |    |       | (x,y) |    |       | (x,y) |    |       | (x,y) |    (???): pixel to be checked
					+-------+-------+    +-------+-------+    +-------+-------+    +-------+-------+    
					=> turn right        => turn left         => move ahead        => turn right
					                     => emit pixel (x,y)  => emit pixel (x,y)

					*/
					// if forward is border (rule 0)
					//     turn right
					// else if left is not border and forward-left pixel is foreground (rule 1)
					//     emit current pixel
					//     go to pixel
					//     has_in_edge = true
					//     turn left
					// else if forward pixel is foreground (rule 2)
					//     if not do_suppress_border or has_in_edge
					//         emit current pixel
					//     go to pixel
					//     has_in_edge = left is not border
					// else (rule 3)
					//     turn right
					//     if not has_in_edge
					//         has_in_edge = left is not border
					o__TRACE_STEP_CW_DIR_0__o;
					TODO;
				}
				else if (dir == 1)
				{
					o__TRACE_STEP_CW_DIR_1__o;
					TODO;
				}
				else if (dir == 2)
				{
					o__TRACE_STEP_CW_DIR_2__o;
					TODO;
				}
				else
				{
					assert(dir == 3);
					o__TRACE_STEP_CW_DIR_3__o;
					TODO;
				}
			}
			while ((x != seed.stop.x || y != seed.stop.y || dir != seed.stop.dir) && contour_length < max_contour_length);
		}
		else
		{
			do
			{
				if (dir == 0)
				{
					o__TRACE_STEP_CCW_DIR_0__o;
					TODO;
				}
				else if (dir == 1)
				{
					o__TRACE_STEP_CCW_DIR_1__o;
					TODO;
				}
				else if (dir == 2)
				{
					o__TRACE_STEP_CCW_DIR_2__o;
					TODO;
				}
				else
				{
					assert(dir == 3);
					o__TRACE_STEP_CCW_DIR_3__o;
					TODO;
				}
			}
			while ((x != seed.stop.x || y != seed.stop.y || dir != seed.stop.dir) && contour_length < max_contour_length);
		}
	}
}