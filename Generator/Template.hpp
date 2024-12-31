#pragma once
//
// Copyright 2024 Axel Walthelm
//

//o__#__o//
//o__#__o// This is a template file using generator macros starting with "o__" and ending with "__o".
//o__#__o//
//o__#__o// Lines containing "//o__#__o//" will be removed by generator.
//o__#__o// Such lines can be used to add fake code only used to pacify syntax checkers looking at the template.
#define o__isValueForeground //o__#__o//
#define __o //o__#__o//
#define o__isForwardBorderDir0__o false //o__#__o//
#define o__isForwardBorderDir1__o false //o__#__o//
#define o__isForwardBorderDir2__o false //o__#__o//
#define o__isForwardBorderDir3__o false //o__#__o//
#define o__TRACE_GENERIC_COMMENT__o //o__#__o//
#define o__TRACE_STEP_CW_DIR_0__o //o__#__o//
#define o__TRACE_STEP_CW_DIR_1__o //o__#__o//
#define o__TRACE_STEP_CW_DIR_2__o //o__#__o//
#define o__TRACE_STEP_CW_DIR_3__o //o__#__o//
#define o__TRACE_STEP_CCW_DIR_0__o //o__#__o//
#define o__TRACE_STEP_CCW_DIR_1__o //o__#__o//
#define o__TRACE_STEP_CCW_DIR_2__o //o__#__o//
#define o__TRACE_STEP_CCW_DIR_3__o //o__#__o//
//o__#__o// Preprocessor.py variables that control which variant (e.g. image type) the generated C++ file will be for
#define o__ONE_BYTE_PER_PIXEL__o 1 //o__#__o//
#define o__ONE_BIT_PER_PIXEL__o 0 //o__#__o//
#define o__THRESHOLD_IS_USED__o 0 //o__#__o//
#define o__THRESHOLD_PARAMETER__o //, int threshold //o__#__o//
#define o__IMAGE_PARAMETER__o , const uint8_t* const image, const int width, const int height, const int stride //, const int threshold //o__#__o//
#define o__IMAGE_ARGUMENTS__o , image, width, height, stride //, threshold //o__#__o//
#define o__IMAGE_PTR_ARGUMENTS__o , image_ptr, width, height, stride //, threshold //o__#__o//
//o__#__o//
/*
o__WARNING_CODE_IS_GENERATED__o
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

#ifndef o__NAMESPACE__o_GENERATOR_OPTIMIZED
#define o__NAMESPACE__o_GENERATOR_OPTIMIZED 1
#endif

/*
o__INTRODUCTION__o

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

namespace o__NAMESPACE__o
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
#ifndef o__NAMESPACE__o_Assert
		void defaultErrorHandler(const char* failed_expression, const char* error_message, const char* function_name, const char* file_name, int line_number)
		{
			if (!error_message || !error_message[0])
				error_message = "o__NAMESPACE__o_Assert failed";
			printf("%s: %s in function %s: %s(%d)\n", error_message, failed_expression, function_name, file_name, line_number);
			exit(-1);
		}
#define o__NAMESPACE__o_Assert(expr,msg) do { if(!!(expr)) ; else defaultErrorHandler(#expr, (msg), __func__, __FILE__, __LINE__ ); } while(0)
#endif
#if o__ONE_BIT_PER_PIXEL__o //o__#__o//

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
#endif

		constexpr int dx[] = {0, 1, 0, -1};
		constexpr int dy[] = {-1, 0, 1, 0};

		inline bool isForeground(int x, int y o__IMAGE_PARAMETER__o)
		{
			return x >= 0 && y >= 0 && x < width && y < height && o__isValueForeground(image[x + y * stride])__o;
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

		inline bool isLeftForeground(int x, int y, int dir, bool clockwise o__IMAGE_PARAMETER__o)
		{
			moveLeft(x, y, dir, clockwise);
			return isForeground(x, y o__IMAGE_ARGUMENTS__o);
		}

		inline bool isLeftForwardForeground(int x, int y, int dir, bool clockwise o__IMAGE_PARAMETER__o)
		{
			moveForward(x, y, dir);
			moveLeft(x, y, dir, clockwise);
			return isForeground(x, y o__IMAGE_ARGUMENTS__o);
		}

		inline bool isForwardForeground(int x, int y, int dir, bool clockwise o__IMAGE_PARAMETER__o)
		{
			moveForward(x, y, dir);
			return isForeground(x, y o__IMAGE_ARGUMENTS__o);
		}

		inline bool isForwardBorder(int x, int y, int dir, int width, int height)
		{
			return dir < 2
				? (dir == 0 ? o__isForwardBorderDir0__o : o__isForwardBorderDir1__o)
				: (dir == 2 ? o__isForwardBorderDir2__o : o__isForwardBorderDir3__o);
		}

		inline bool isLeftBorder(int x, int y, int dir, bool clockwise, int width, int height)
		{
			return isForwardBorder(x, y, turnLeft(dir, clockwise), width, height);
		}

		// Analyze if the current edge or an earlier contour-edge of the given pixel is not on the image border
		// by tracing up to 4 steps backward, but only if we stay on the given pixel.
		inline bool hasPixelNonBorderEdgeBackwards(int x, int y, int dir, bool clockwise o__IMAGE_PARAMETER__o)
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
				if (isLeftForwardForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
				{
					break; // next contour edge is on a different pixel
				}
				// (rule 2)
				else if (isForwardForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
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

#if o__ONE_BYTE_PER_PIXEL__o //o__#__o//
	// @param contour Receives the resulting contour points. It should be initially empty if contour tracing starts new (but no check is done).
	// TContour needs to implement a small sub-set of std::vector<cv::Point>:
	//     void TContour::emplace_back(int x, int y)
	//
	// @param image Single channel 8 bit read access to the image to trace contour in.
#if !o__THRESHOLD_IS_USED__o //o__#__o//
	// Pixel with non-zero value are foreground. All other pixels including those outside of image are background.
#else
	// Pixel with value above threshold are foreground. All other pixels including those outside of image are background.
#endif
	// TImage needs to implement a small sub-set of cv::Mat and expects continuous row-major single 8-bit channel raster image memory:
	//     int TImage::rows; // number of rows, i.e. image height
	//     int TImage::cols; // number of columns, i.e. image width
	//     uint8_t* TImage::ptr(int row, int column) // get pointer to pixel in image at row y and column x; row/column counting starts at zero
	// 
#if o__THRESHOLD_IS_USED__o //o__#__o//
	// @param threshold Pixel bytes higher than this value are considered to be foreground.
	// 
#endif
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
	template<typename TContour, typename TImage>
	int findContour(TContour& contour, TImage const& image o__THRESHOLD_PARAMETER__o, int x, int y, int dir = -1, bool clockwise = false, bool do_suppress_border = false, stop_t* stop = NULL)
	{
		o__NAMESPACE__o_Assert(-1 <= dir && dir < 4, "seed direction is invalid");

		// image properties
		const int width = image.cols;
		const int height = image.rows;
		o__NAMESPACE__o_Assert(width > 0 && height > 0, "image is empty");

		const uint8_t* const image_ptr = image.ptr(0, 0);
		const int stride = height == 1 ? width : int(image.ptr(1, 0) - image_ptr);
		o__NAMESPACE__o_Assert(width == 1 || height == 1 || image.ptr(0, 1) - image_ptr == 1, (image.ptr(1, 0) - image_ptr == 1 ? "image is not row-major order" : "pixel is not single byte"));

		return findContour(contour o__IMAGE_PTR_ARGUMENTS__o, x, y, dir, clockwise, do_suppress_border, stop);
	}
#endif o__ONE_BYTE_PER_PIXEL__o


#if o__ONE_BYTE_PER_PIXEL__o //o__#__o//
	// Like findContour above, but with a C-style image.
#endif
	// @param image Pointer to image memory, 1 byte per pixel, row-major.
	// @param width Width of image, i.e. image dimension in x coordinate.
	// @param height Height of image, i.e. image dimension in y coordinate.
	// @param stride Stride of image, i.e. offset between start of consecutive rows, i.e. width plus padding bytes at the end of the image line.
#if !o__THRESHOLD_IS_USED__o //o__#__o//
	// Pixel with non-zero value are foreground. All other pixels including those outside of image are background.
#else
	// @param threshold Threshold to binarize image.
	// Pixel with value above threshold are foreground. All other pixels including those outside of image are background.
#endif
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
	int findContour(TContour& contour o__IMAGE_PARAMETER__o, int x, int y, int dir = -1, bool clockwise = false, bool do_suppress_border = false, stop_t* stop = NULL)
	{
		o__NAMESPACE__o_Assert(0 <= x && x < width && 0 <= y && y < height, "seed pixel is outside of image");
		o__NAMESPACE__o_Assert(isForeground(x, y o__IMAGE_ARGUMENTS__o), "seed pixel is not foreground");

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
				if (!isLeftForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
					break;
			}

			if (dir == 4)
			{
				for (dir = 0; dir < 4; dir++)
				{
					if (!isLeftForwardForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
						break;
				}
			}

			o__NAMESPACE__o_Assert(dir < 4, "bad seed pixel");
		}

		if (isLeftForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o) &&
			isForwardForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
		{
			moveForward(x, y, dir);
		}

		o__NAMESPACE__o_Assert(!isLeftForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o), "bad seed direction");

		const int start_x = x;
		const int start_y = y;
		const int start_dir = dir;

		const bool is_stop_in = stop != NULL && stop->dir >= 0 && stop->dir < 4;
		if (is_stop_in)
		{
			o__NAMESPACE__o_Assert(isForeground(stop->x, stop->y o__IMAGE_ARGUMENTS__o), "stop pixel is not foreground");
			o__NAMESPACE__o_Assert(!isLeftForeground(stop->x, stop->y, stop->dir, clockwise o__IMAGE_ARGUMENTS__o), "stop pixel has bad direction");
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
			hasPixelNonBorderEdgeBackwards(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o);

		if (max_contour_length > 0)
		{

#if !o__NAMESPACE__o_GENERATOR_OPTIMIZED

			//o__#__o// o__TRACE_GENERIC_COMMENT__o;
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
				if (isLeftForwardForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
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
				else if (isForwardForeground(x, y, dir, clockwise o__IMAGE_ARGUMENTS__o))
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
			} while (o__TRACE_CONTINUE_CONDITION__o);

#else

#if !o__ONE_BIT_PER_PIXEL__o //o__#__o//
			// pointer to current pixel
			const uint8_t* pixel = &image[x + y * stride];
#else
			// index of current pixel in image
			size_t pixel = x + y * stride;
#endif

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
			//o__#__o//
			//o__#__o// from here on we can use all pixel accessing generator macros
			//o__#__o//

			int sum_of_turn_overflows = 0;

			if (clockwise)
			{
				do
				{
					if (dir == 0)
					{
						o__TRACE_STEP_CW_DIR_0__o;
					}
					else if (dir == 1)
					{
						o__TRACE_STEP_CW_DIR_1__o;
					}
					else if (dir == 2)
					{
						o__TRACE_STEP_CW_DIR_2__o;
					}
					else
					{
						assert(dir == 3);
						o__TRACE_STEP_CW_DIR_3__o;
					}
				} while (o__TRACE_CONTINUE_CONDITION__o);
			}
			else
			{
				do
				{
					if (dir == 0)
					{
						o__TRACE_STEP_CCW_DIR_0__o;
					}
					else if (dir == 1)
					{
						o__TRACE_STEP_CCW_DIR_1__o;
					}
					else if (dir == 2)
					{
						o__TRACE_STEP_CCW_DIR_2__o;
					}
					else
					{
						assert(dir == 3);
						o__TRACE_STEP_CCW_DIR_3__o;
					}
				} while (o__TRACE_CONTINUE_CONDITION__o);
			}

			sum_of_turns = sum_of_turn_overflows * 4 + (clockwise ? dir - start_dir : start_dir - dir);

#endif // o__NAMESPACE__o_GENERATOR_OPTIMIZED

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

} // namespace o__NAMESPACE__o
