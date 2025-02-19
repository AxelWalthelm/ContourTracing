#pragma once
//
// Copyright 2024 Axel Walthelm
//

/*
#############################################################################
# WARNING: this code was generated - do not edit, your changes may get lost #
#############################################################################
Consider to edit Generator\Generator.py and Generator\Template.hpp instead.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

#ifndef FECTS_GENERATOR_OPTIMIZED
#define FECTS_GENERATOR_OPTIMIZED 1
#endif

/*
 Fast Edge-Based Contour Tracing from Seed-Point (FECTS)
============================================================

See README.md at https://github.com/AxelWalthelm/ContourTracing/ for more information.

    Definition of direction
                                    x    
    +---------------------------------->  
    |               (0, -1)               
    |                  0  up              
    |                  ^                  
    |                  |                  
    |                  |                  
    | (-1, 0) 3 <------+------> 1 (1, 0)  
    |       left       |      right       
    |                  |                  
    |                  v                  
    |                  2  down            
  y |               (0, 1)                
    v                                     

Tracing contour of 4-connected objects
----------------------------------------

The current implementation does not support it.
To trace contour pixel of a 4-connected foreground area, the rules need to be changed.
For clockwise tracing they would be basically something like:
	if forward pixel is not foreground
		turn right
	else if forward-left pixel is foreground
		turn left
	else
		move ahead 

The pixel emission would also change a little, giving rules like:
	if forward pixel is not foreground
		turn right
	else if forward-left pixel is foreground
		emit current pixel, emit foward pixel (if you want a 4-connected contour), turn left, move to forward-left pixel
	else
		emit current pixel, move ahead

For counterclockwise tracing the rules change in that left is swapped with right.
The rules for border suppression and optimized border checking should be similar too.
Since OpenCV did not see any need to support 4-connected object contour tracing, a different way of testing
the result needs to be found.

As a workaround you might consider to invert the image and trace the background contour.
The resulting contour line is still 8-connected, but the contour line goes around the 4-connected object,
but all contour pixel are background, i.e. it will be "grown" outwards.
Maybe your application would work better with eroding the inverted mask a little,
but it still wouldn't be exactly the same result in the end.
*/

namespace FECTS_B
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
#ifndef FECTS_Assert
		void defaultErrorHandler(const char* failed_expression, const char* error_message, const char* function_name, const char* file_name, int line_number)
		{
			if (!error_message || !error_message[0])
				error_message = "FECTS_Assert failed";
			printf("%s: %s in function %s: %s(%d)\n", error_message, failed_expression, function_name, file_name, line_number);
			exit(-1);
		}
#define FECTS_Assert(expr,msg) do { if(!!(expr)) ; else defaultErrorHandler(#expr, (msg), __func__, __FILE__, __LINE__ ); } while(0)
#endif

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

		constexpr int dx[] = {0, 1, 0, -1};
		constexpr int dy[] = {-1, 0, 1, 0};

		inline bool isForeground(int x, int y, const uint8_t* const image, const int width, const int height, const int stride)
		{
			return x >= 0 && y >= 0 && x < width && y < height && bittest(image, x + y * stride);
		}

		inline int turnLeft(int dir, bool clockwise)
		{
			// rules for tracing counterclockwise turn left into right and vice versa
			return (dir + (clockwise ? 4 - 1 : 1)) & 3;
		}

		inline int turnRight(int dir, bool clockwise)
		{
			// rules for tracing counterclockwise turn left into right and vice versa
			return turnLeft(dir, !clockwise);
		}

		inline void moveLeft(int& x, int& y, int dir, bool clockwise)
		{
			dir = turnLeft(dir, clockwise);
			x += dx[dir];
			y += dy[dir];
		}

		inline void moveForward(int& x, int& y, int dir)
		{
			x += dx[dir];
			y += dy[dir];
		}

		inline bool isLeftForeground(int x, int y, int dir, bool clockwise, const uint8_t* const image, const int width, const int height, const int stride)
		{
			moveLeft(x, y, dir, clockwise);
			return isForeground(x, y, image, width, height, stride);
		}

		inline bool isLeftForwardForeground(int x, int y, int dir, bool clockwise, const uint8_t* const image, const int width, const int height, const int stride)
		{
			moveForward(x, y, dir);
			moveLeft(x, y, dir, clockwise);
			return isForeground(x, y, image, width, height, stride);
		}

		inline bool isForwardForeground(int x, int y, int dir, bool clockwise, const uint8_t* const image, const int width, const int height, const int stride)
		{
			moveForward(x, y, dir);
			return isForeground(x, y, image, width, height, stride);
		}

		inline bool isForwardBorder(int x, int y, int dir, int width, int height)
		{
			return dir < 2
				? (dir == 0 ? y == 0 : x == width - 1)
				: (dir == 2 ? y == height - 1 : x == 0);
		}

		inline bool isLeftBorder(int x, int y, int dir, bool clockwise, int width, int height)
		{
			return isForwardBorder(x, y, turnLeft(dir, clockwise), width, height);
		}

		// Analyze if the current edge or an earlier contour-edge of the given pixel is not on the image border
		// by tracing up to 4 steps backward, but only if we stay on the given pixel.
		inline bool hasPixelNonBorderEdgeBackwards(int x, int y, int dir, bool clockwise, const uint8_t* const image, const int width, const int height, const int stride)
		{
			// turn around
			dir = (dir + 2) % 4;
			clockwise = !clockwise;

			for (int step = 0; step < 4; step++)
			{
				// check if current edge is non-border
				if (!isLeftBorder(x, y, dir, clockwise, width, height))
					return true;

				// (rule 1)
				if (isLeftForwardForeground(x, y, dir, clockwise, image, width, height, stride))
				{
					break; // next contour edge is on a different pixel
				}
				// (rule 2)
				else if (isForwardForeground(x, y, dir, clockwise, image, width, height, stride))
				{
					break; // next contour edge is on a different pixel
				}
				// (rule 3)
				else
				{
					dir = turnRight(dir, clockwise);
				}
			}

			return false;
		}

	} // namespace

	struct stop_t
	{
		// Usually the full contour is traced.
		// Sometimes it is useful to limit the length of the contour, e.g. to limit time and memory usage.
		// Set it to zero to only do startup logic like choosing a valid start direction.
		// In: if >= 0 then the maximum allowed contour length
		// Out: number of traced contour pixels including suppressed pixels
		int max_contour_length = -1;

		// Usually tracing stops when the start position is reached.
		// Sometimes it is useful to stop at another known position on the contour.
		// In: if dir is a valid direction 0-3 then (x, y, dir) becomes an additional position to stop tracing
		// Out: (x, y, dir) is the position tracing stopped, e.g. because maximum contour length was reached
		int dir = -1;
		int x;
		int y;
	};



	// @param image Pointer to image memory, 1 byte per pixel, row-major.
	// @param width Width of image, i.e. image dimension in x coordinate.
	// @param height Height of image, i.e. image dimension in y coordinate.
	// @param stride Stride of image, i.e. offset between start of consecutive rows, i.e. width plus padding bytes at the end of the image line.
	// Pixel with non-zero value are foreground. All other pixels including those outside of image are background.
	// 
	// @param x Seed pixel x coordinate.
	// @param y Seed pixel y coordinate.
	// Usually seed pixel (x,y) is taken as the start pixel, but if (x,y) touches the contour only by a corner
	// (but not by an edge), the start pixel is moved one pixel forward in the given (or automatically chosen) direction
	// to ensure the resulting contour is consistently 8-connected thin.
	// The start pixel will be the first pixel in contour, unless it has only contour edges at the image border and do_suppress_border is set.
	//
	// @param dir Direction to start contour tracing with. 0 is up, 1 is right, 2 is down, 3 is left.
	// If value is -1, no direction dir is given and a direction is chosen automatically.
	// This works well if the seed pixel is part of a single contour only.
	// If the object to trace is very narrow and the seed pixel is touching the contour on both sides,
	// the side with the smallest dir is chosen.
	// Note that a seed pixel can be part of up to four different contours, but no more than one of them can be an outer contour.
	// So if you expect an outer contour and an outer contour is found, you are good.
	// Otherwise you need to be more specific.
	//
	// @param clockwise Indicates if outer contours are traced clockwise or counterclockwise.
	// Note that inner contours run in the opposite direction.
	// If tracing is clockwise, the traced edge is to the left of the current pixel (looking in the current direction),
	// otherwise the traced edge is to the right.
	// Set it to false to trace similar to OpenCV cv::findContours.
	//
	// @param do_suppress_border Indicates to omit pixels of the contour that are followed on border edges only.
	// The contour still contains border pixels where it arrives at the image border or where it leaves tha image border,
	// but not those pixel that only follow the border.
	//
	// @param stop Structure to control stop behavior and to return extra information on the state of tracing at the end.
	//
	// @return The total difference between left and right turns done during tracing.
	// If a contour is traced completely, i.e. it is traced until it returns to the start edge,
	// the value is 4 for an outer contour and -4 if it is an inner contour.
	// When tracing stops due to stop.max_contour_length the contour is usually not traced completely.
	// Even if all pixels have been found, up to 3 final edge tracing turns may not have been done,
	// so if you somehow know that all pixels have been found, you can still use the sign of the return value
	// to decide if it is an outer or inner contour.
	template<typename TContour>
	int findContour(TContour& contour, const uint8_t* const image, const int width, const int height, const int stride, int x, int y, int dir = -1, bool clockwise = false, bool do_suppress_border = false, stop_t* stop = NULL)
	{
		FECTS_Assert(0 <= x && x < width && 0 <= y && y < height, "seed pixel is outside of image");
		FECTS_Assert(isForeground(x, y, image, width, height, stride), "seed pixel is not foreground");

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
				if (!isLeftForeground(x, y, dir, clockwise, image, width, height, stride))
					break;
			}

			if (dir == 4)
			{
				for (dir = 0; dir < 4; dir++)
				{
					if (!isLeftForwardForeground(x, y, dir, clockwise, image, width, height, stride))
						break;
				}
			}

			FECTS_Assert(dir < 4, "bad seed pixel");
		}

		if (isLeftForeground(x, y, dir, clockwise, image, width, height, stride) &&
			isForwardForeground(x, y, dir, clockwise, image, width, height, stride))
		{
			moveForward(x, y, dir);
		}

		FECTS_Assert(!isLeftForeground(x, y, dir, clockwise, image, width, height, stride), "bad seed direction");

		const int start_x = x;
		const int start_y = y;
		const int start_dir = dir;

		const bool is_stop_in = stop != NULL && stop->dir >= 0 && stop->dir < 4;
		if (is_stop_in)
		{
			FECTS_Assert(isForeground(stop->x, stop->y, image, width, height, stride), "stop pixel is not foreground");
			FECTS_Assert(!isLeftForeground(stop->x, stop->y, stop->dir, clockwise, image, width, height, stride), "stop pixel has bad direction");
		}
		const int stop_x = is_stop_in ? stop->x : start_x;
		const int stop_y = is_stop_in ? stop->y : start_y;
		const int stop_dir = is_stop_in ? stop->dir : start_dir;

		const int max_contour_length = stop != NULL && stop->max_contour_length >= 0
			? std::min(stop->max_contour_length, upperLimitContourLength(width, height))
			: upperLimitContourLength(width, height);
		int contour_length = 0;
		int sum_of_turns = 0;

		// If do_suppress_border=true is_pixel_valid indicates if the current pixel has an edge
		// on contour which is inside of the image, i.e. not only edges at image border.
		// Otherwise it is always true.
		bool is_pixel_valid = !do_suppress_border ||
			hasPixelNonBorderEdgeBackwards(x, y, dir, clockwise, image, width, height, stride);

		if (max_contour_length > 0)
		{

#if !FECTS_GENERATOR_OPTIMIZED

			/*
			clockwise rules:
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

			if forward-left pixel is foreground (rule 1)
			    emit current pixel
			    go to checked pixel
			    turn left
			    stop if buffer is full
			    set pixel valid
			else if forward pixel is foreground (rule 2)
			    if pixel is valid
			        emit current pixel
			    go to checked pixel
			    stop if buffer is full
			    if border is to be suppressed, set pixel valid if left is not border
			else (rule 3)
			    turn right
			    set pixel valid if left is not border

			In case of counterclockwise tracing the rules are the same except that left and right are exchanged.
			*/

			do
			{
				// (rule 1)
				if (isLeftForwardForeground(x, y, dir, clockwise, image, width, height, stride))
				{
					contour.emplace_back(x, y);
					moveForward(x, y, dir);
					moveLeft(x, y, dir, clockwise);
					dir = turnLeft(dir, clockwise);
					--sum_of_turns;
					if (++contour_length >= max_contour_length)
						break;
					is_pixel_valid = true;
				}
				// (rule 2)
				else if (isForwardForeground(x, y, dir, clockwise, image, width, height, stride))
				{
					if (is_pixel_valid)
					{
						contour.emplace_back(x, y);
					}
					moveForward(x, y, dir);
					if (++contour_length >= max_contour_length) // contour_length is the unsuppressed length
						break;
					if (do_suppress_border)
						is_pixel_valid = !isLeftBorder(x, y, dir, clockwise, width, height);
				}
				// (rule 3)
				else
				{
					dir = turnRight(dir, clockwise);
					++sum_of_turns;
					if (!is_pixel_valid)
						is_pixel_valid = !isLeftBorder(x, y, dir, clockwise, width, height);
				}
			} while ((x != start_x || y != start_y || dir != start_dir)
			         && (!is_stop_in || x != stop_x || y != stop_y || dir != stop_dir));

#else

			// index of current pixel in image
			size_t pixel = x + y * stride;

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

			const int width_m1 = width - 1;
			const int height_m1 = height - 1;

			int sum_of_turn_overflows = 0;

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
						    turn left
						    stop if buffer is full
						    set pixel valid
						else if forward pixel is foreground (rule 2)
						    if pixel is valid
						        emit current pixel
						    go to checked pixel
						    stop if buffer is full
						    if border is to be suppressed, set pixel valid if left is not border
						else (rule 3)
						    turn right
						    set pixel valid if left is not border
						*/

						// if forward is border (rule 0)
						if (y == 0)
						{
						    // turn right
						    dir = 1;
						}
						// else if left is not border and forward-left pixel is foreground (rule 1)
						else if (x != 0 && bittest(image, pixel + off_mm))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_mm;
						    --x;
						    --y;
						    // turn left
						    dir = 3;
						    --sum_of_turn_overflows;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_0m))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_0m;
						    --y;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length) // contour_length is the unsuppressed length
						        break;
						    // if border is to be suppressed, set pixel valid if left is not border
						    if (do_suppress_border)
						        is_pixel_valid = x != 0;
						}
						// else (rule 3)
						else
						{
						    // turn right
						    dir = 1;
						    // set pixel valid if left is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = y != 0;
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
						else if (y != 0 && bittest(image, pixel + off_pm))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_pm;
						    ++x;
						    --y;
						    // turn left
						    dir = 0;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_p0))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_p0;
						    ++x;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if left is not border
						    if (do_suppress_border)
						        is_pixel_valid = y != 0;
						}
						// else (rule 3)
						else
						{
						    // turn right
						    dir = 2;
						    // set pixel valid if left is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = x != width_m1;
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
						else if (x != width_m1 && bittest(image, pixel + off_pp))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_pp;
						    ++x;
						    ++y;
						    // turn left
						    dir = 1;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_0p))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_0p;
						    ++y;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if left is not border
						    if (do_suppress_border)
						        is_pixel_valid = x != width_m1;
						}
						// else (rule 3)
						else
						{
						    // turn right
						    dir = 3;
						    // set pixel valid if left is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = y != height_m1;
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
						    ++sum_of_turn_overflows;
						}
						// else if left is not border and forward-left pixel is foreground (rule 1)
						else if (y != height_m1 && bittest(image, pixel + off_mp))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_mp;
						    --x;
						    ++y;
						    // turn left
						    dir = 2;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_m0))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_m0;
						    --x;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if left is not border
						    if (do_suppress_border)
						        is_pixel_valid = y != height_m1;
						}
						// else (rule 3)
						else
						{
						    // turn right
						    dir = 0;
						    ++sum_of_turn_overflows;
						    // set pixel valid if left is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = x != 0;
						}
					}
				} while ((x != start_x || y != start_y || dir != start_dir)
				         && (!is_stop_in || x != stop_x || y != stop_y || dir != stop_dir));
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
						    turn right
						    stop if buffer is full
						    set pixel valid
						else if forward pixel is foreground (rule 2)
						    if pixel is valid
						        emit current pixel
						    go to checked pixel
						    stop if buffer is full
						    if border is to be suppressed, set pixel valid if right is not border
						else (rule 3)
						    turn left
						    set pixel valid if right is not border
						*/

						// if forward is border (rule 0)
						if (y == 0)
						{
						    // turn left
						    dir = 3;
						    ++sum_of_turn_overflows;
						}
						// else if right is not border and forward-right pixel is foreground (rule 1)
						else if (x != width_m1 && bittest(image, pixel + off_pm))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_pm;
						    ++x;
						    --y;
						    // turn right
						    dir = 1;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_0m))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_0m;
						    --y;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if right is not border
						    if (do_suppress_border)
						        is_pixel_valid = x != width_m1;
						}
						// else (rule 3)
						else
						{
						    // turn left
						    dir = 3;
						    ++sum_of_turn_overflows;
						    // set pixel valid if right is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = y != 0;
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
						else if (y != height_m1 && bittest(image, pixel + off_pp))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_pp;
						    ++x;
						    ++y;
						    // turn right
						    dir = 2;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_p0))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_p0;
						    ++x;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if right is not border
						    if (do_suppress_border)
						        is_pixel_valid = y != height_m1;
						}
						// else (rule 3)
						else
						{
						    // turn left
						    dir = 0;
						    // set pixel valid if right is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = x != width_m1;
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
						else if (x != 0 && bittest(image, pixel + off_mp))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_mp;
						    --x;
						    ++y;
						    // turn right
						    dir = 3;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_0p))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_0p;
						    ++y;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if right is not border
						    if (do_suppress_border)
						        is_pixel_valid = x != 0;
						}
						// else (rule 3)
						else
						{
						    // turn left
						    dir = 1;
						    // set pixel valid if right is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = y != height_m1;
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
						else if (y != 0 && bittest(image, pixel + off_mm))
						{
						    // emit current pixel
						    contour.emplace_back(x, y);
						    // go to checked pixel
						    pixel += off_mm;
						    --x;
						    --y;
						    // turn right
						    dir = 0;
						    --sum_of_turn_overflows;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // set pixel valid
						    is_pixel_valid = true;
						}
						// else if forward pixel is foreground (rule 2)
						else if (bittest(image, pixel + off_m0))
						{
						    // if pixel is valid
						    if (is_pixel_valid)
						    {
						        // emit current pixel
						        contour.emplace_back(x, y);
						    }
						    // go to checked pixel
						    pixel += off_m0;
						    --x;
						    // stop if buffer is full
						    if (++contour_length >= max_contour_length)
						        break;
						    // if border is to be suppressed, set pixel valid if right is not border
						    if (do_suppress_border)
						        is_pixel_valid = y != 0;
						}
						// else (rule 3)
						else
						{
						    // turn left
						    dir = 2;
						    // set pixel valid if right is not border
						    if (!is_pixel_valid)
						        is_pixel_valid = x != 0;
						}
					}
				} while ((x != start_x || y != start_y || dir != start_dir)
				         && (!is_stop_in || x != stop_x || y != stop_y || dir != stop_dir));
			}

			sum_of_turns = sum_of_turn_overflows * 4 + (clockwise ? dir - start_dir : start_dir - dir);

#endif // FECTS_GENERATOR_OPTIMIZED

			if (contour_length == 0)
			{
				// contour object is a single isolated pixel
				if (is_pixel_valid)
				{
					contour.emplace_back(start_x, start_y);
				}
				++contour_length; // contour_length is the unsuppressed length
			}
		}

		if (stop != NULL)
		{
			stop->max_contour_length = contour_length; // unsuppressed contour length
			stop->x = x;
			stop->y = y;
			stop->dir = dir;
		}

		return sum_of_turns;
	}

} // namespace FECTS_B
