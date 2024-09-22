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

#define GENERATOR_OPTIMIZED 0

/*
 Fast Edge-Based Pixel Contour Tracing from Seed Point
=======================================================

TODO

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

namespace FEPCT
{
	// Upper limit of contour length is used to prevent infinite loop and out-of-memory crash
	// if stop criteria is incorrect.
	// It could also be used to allocate memory to hold contour(s) without further memory
	// allocations during tracing, but note that most contours are significantly shorter.
	int upperLimitContourLength(int width, int height)
	{
		/*
		How long is the longest 8-connected countour of an 8-connected region?
		For convex 8-connected regions an upper limit can be as low as 2*(width+height).
		But doing some examples shows that in general width*height is only a lower limit.
		Realizing a pixel can be in the contour no more than twice, 2*width*height is an upper limit.
		It seems that the worst case is a single pixel wide "snake" in the image like this example:

			+-+-+-+-+-+-+-+-+-+
			|*| |*|*|*| |*|*|*|
			+-+-+-+-+-+-+-+-+-+
			|*| |*| |*| |*| |*|
			+-+-+-+-+-+-+-+-+-+
			|*| |*| |*| |*| |*|
			+-+-+-+-+-+-+-+-+-+
			|*|*|*| |*|*|*| |*|
			+-+-+-+-+-+-+-+-+-+

		Based on this scheme and assuming it is in fact close to the worst case
		we use width*heigt+width+height as an upper limit estimate good enough for practical use.
		*/
		return width * height + width + height;
	}

	namespace
	{
#ifndef FEPCT_Assert
		void defaultErrorHandler(const char* failed_expression, const char* error_message, const char* function_name, const char* file_name, int line_number)
		{
			if (!error_message || !error_message[0])
				error_message = "FEPCT_Assert failed";
			printf("%s: %s in function %s: %s(%d)\n", error_message, failed_expression, function_name, file_name, line_number);
			exit(-1);
		}
#define FEPCT_Assert(expr,msg) do { if(!!(expr)) ; else defaultErrorHandler(#expr, (msg), __func__, __FILE__, __LINE__ ); } while(0)
#endif

		constexpr int dx[] = {0, 1, 0, -1};
		constexpr int dy[] = {-1, 0, 1, 0};

		inline bool isForeground(int x, int y, const uint8_t* image_ptr, int width, int height, int stride)
		{
			return x >= 0 && y >= 0 && x < width && y < height && image_ptr[x + y * stride] != 0;
		}

		inline int left(int dir, bool clockwise)
		{
			// rules for tracing counterclockwise turn left into right and vice versa
			return (dir + (clockwise ? 4 - 1 : 1)) & 3;
		}

		inline int right(int dir, bool clockwise)
		{
			// rules for tracing counterclockwise turn left into right and vice versa
			return left(dir, !clockwise);
		}

		inline void moveLeft(int& x, int& y, int dir, bool clockwise)
		{
			dir = left(dir, clockwise);
			x += dx[dir];
			y += dy[dir];
		}

		inline void moveForward(int& x, int& y, int dir)
		{
			x += dx[dir];
			y += dy[dir];
		}

		inline bool isLeftForeground(int x, int y, int dir, bool clockwise, const uint8_t* image_ptr, int width, int height, int stride)
		{
			moveLeft(x, y, dir, clockwise);
			return isForeground(x, y, image_ptr, width, height, stride);
		}

		inline bool isLeftForwardForeground(int x, int y, int dir, bool clockwise, const uint8_t* image_ptr, int width, int height, int stride)
		{
			moveForward(x, y, dir);
			moveLeft(x, y, dir, clockwise);
			return isForeground(x, y, image_ptr, width, height, stride);
		}

		inline bool isForwardForeground(int x, int y, int dir, bool clockwise, const uint8_t* image_ptr, int width, int height, int stride)
		{
			moveForward(x, y, dir);
			return isForeground(x, y, image_ptr, width, height, stride);
		}

		inline bool isForwardBorder(int x, int y, int dir, int width, int height)
		{
			return dir < 2
				? (dir == 0 ? y == 0 : x == width - 1)
				: (dir == 2 ? y == height - 1 : x == 0);
		}

		inline bool isLeftBorder(int x, int y, int dir, bool clockwise, int width, int height)
		{
			return isForwardBorder(x, y, left(dir, clockwise), width, height);
		}

	} // namespace

	struct stop_t
	{
		int max_contour_length = -1;
		int x;
		int y;
		int dir = -1;
	};

	// TContour needs to implement a small sub-set of std::vector<cv::Point>:
	//     void TContour::emplace_back(int x, int y);
	// TImage needs to implement a small sub-set of cv::Mat:
	//     int TImage::rows; // image height
	//     int TImage::cols; // image width
	//     uint8_t* TImage::ptr(int row, int col) // get pointer into continuous row-major single channel raster image memory
	template<typename TContour, typename TImage>
	void findContour(TContour& contour, TImage const& image, int x, int y, int dir = -1, bool clockwise = false, bool do_suppress_border = false, stop_t* stop = NULL)
	{
		FEPCT_Assert(-1 <= dir && dir < 4, "seed direction is invalid");

		// image properties
		const int width = image.cols;
		const int height = image.rows;
		FEPCT_Assert(width > 0 && height > 0, "image is empty");
		const int width_m1 = width - 1;
		const int height_m1 = height - 1;

		const uint8_t* image_ptr = image.ptr(0, 0);
		const int stride = int(image.ptr(1, 0) - image_ptr);
		FEPCT_Assert(image.ptr(0, 1) - image_ptr == 1, (image.ptr(1, 0) - image_ptr == 1 ? "image is not row-major order" : "pixel is not single byte"));

		FEPCT_Assert(0 <= x && x < width && 0 <= y && y < height, "seed pixel is outside of image");
		FEPCT_Assert(isForeground(x, y, image_ptr, width, height, stride), "seed pixel is not foreground");

		if (dir == -1)
		{
			// find start edge; prefer edges of seed pixel (x,y)
			/*
			clockwise:
			             ^           |           
			           < |           |           
			           < 4           |           
			           < |           |           
			             |    ^^^    |    ^^^    
			  -----------+-----1---->+-----5---->
			             ^           |           
			           < |           | >         
			           < 0           2 >         
			           < |           | >         
			             |           v           
			  <----7-----+<----3-----+-----------
			      vvv    |    vvv    |           
			             |           | >         
			             |           6 >         
			             |           | >         
			             |           v           

			counterclockwise:
			             |           ^           
			             |           | >         
			             |           4 >         
			             |           | >         
			      ^^^    |    ^^^    |           
			  <----7-----+<----3-----+-----------
			             |           ^           
			           < |           | >         
			           < 2           0 >         
			           < |           | >         
			             v           |           
			  -----------+-----1---->+-----5---->
			             |    vvv    |    vvv    
			           < |           |           
			           < 6           |           
			           < |           |           
			             v           |           
			*/

			for (dir = 0; dir < 4; dir++)
			{
				if (!isLeftForeground(x, y, dir, clockwise, image_ptr, width, height, stride))
					break;
			}

			if (dir == 4)
			{
				for (dir = 0; dir < 4; dir++)
				{
					if (!isLeftForwardForeground(x, y, dir, clockwise, image_ptr, width, height, stride))
						break;
				}
			}
		}

		if (isLeftForeground(x, y, dir, clockwise, image_ptr, width, height, stride) &&
			isForwardForeground(x, y, dir, clockwise, image_ptr, width, height, stride))
		{
			moveForward(x, y, dir);
		}

		FEPCT_Assert(!isLeftForeground(x, y, dir, clockwise, image_ptr, width, height, stride), "seed pixel has bad direction");
		const int start_x = x;
		const int start_y = y;
		const int start_dir = dir;

		const bool is_stop_in = stop != NULL && stop->dir >= 0 && stop->dir < 4;
		if (is_stop_in)
		{
			FEPCT_Assert(isForeground(stop->x, stop->y, image_ptr, width, height, stride), "stop pixel is not foreground");
			FEPCT_Assert(!isLeftForeground(stop->x, stop->y, stop->dir, clockwise, image_ptr, width, height, stride), "stop pixel has bad direction");
		}
		const int stop_x = is_stop_in ? stop->x : start_x;
		const int stop_y = is_stop_in ? stop->y : start_y;
		const int stop_dir = is_stop_in ? stop->dir : start_dir;

		const int max_contour_length = stop != NULL && stop->max_contour_length >= 0
			? std::min(stop->max_contour_length, upperLimitContourLength(width, height))
			: upperLimitContourLength(width, height);
		int contour_length = 0;

		// does current pixel have an edge on contour which is inside of the image, i.e. not only edges at image border
		bool has_in_edge = !isLeftBorder(x, y, dir, clockwise, width, height);

		if (max_contour_length > 0)
		{

#if !GENERATOR_OPTIMIZED

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

			In case of counterclockwise tracing the rules are the same except that left and right are exchanged.
			*/

			do
			{
				// (rule 0)
				if (isForwardBorder(x, y, dir, width, height))
				{
					dir = right(dir, clockwise);
				}
				// (rule 1)
				else if (!isLeftBorder(x, y, dir, clockwise, width, height) &&
						 isLeftForwardForeground(x, y, dir, clockwise, image_ptr, width, height, stride))
				{
					contour.emplace_back(x, y);
					if (++contour_length >= max_contour_length)
						break;

					moveForward(x, y, dir);
					moveLeft(x, y, dir, clockwise);
					has_in_edge = true;

					dir = left(dir, clockwise);
				}
				// (rule 2)
				else if (isForwardForeground(x, y, dir, clockwise, image_ptr, width, height, stride))
				{
					if (!do_suppress_border || has_in_edge)
					{
						contour.emplace_back(x, y);
					}
					if (++contour_length >= max_contour_length) // contour_length is the unsuppressed length
						break;
					moveForward(x, y, dir);
					has_in_edge = isLeftBorder(x, y, dir, clockwise, width, height);
				}
				// (rule 3)
				else
				{
					dir = right(dir, clockwise);
					if (!has_in_edge)
						has_in_edge = isLeftBorder(x, y, dir, clockwise, width, height);
				}

			} while (x != stop_x || y != stop_y || dir != stop_dir);

#else

			// pointer to current pixel
			const uint8_t* pixel = &image_ptr[x + y * stride];

			// constants to address 8-connected neighbours of pixel
			constexpr int off_00 = 0;
			constexpr int off_p0 = 1;
			constexpr int off_m0 = -1;
			const int off_0p = stride;
			const int off_0m = -stride;
			const int off_pp = off_p0 + off_0p;
			const int off_pm = off_p0 + off_0m;
			const int off_mp = off_m0 + off_0p;
			const int off_mm = off_m0 + off_0m;

			if (clockwise)
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
				} while (x != stop_x || y != stop_y || dir != stop_dir);
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
				} while (x != stop_x || y != stop_y || dir != stop_dir);
			}

#endif // GENERATOR_OPTIMIZED

			if (contour_length == 0)
			{
				// single isolated pixel
				contour.emplace_back(x, y);
			}
		}

		if (stop != NULL)
		{
			stop->x = x;
			stop->y = y;
			stop->dir = dir;
		}
	}

} // namespace FEPCT
