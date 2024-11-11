#pragma once
//
// Copyright 2024 Axel Walthelm
//

#include <algorithm>

// A filtering container that can be used with FECT::findContour to create contours like OpenCV does with option cv::CHAIN_APPROX_SIMPLE.
// If successive points are on a vertical, horizontal, or diagonal line, only start and end point of the line are stored.
// Successive points are expected to be 8-connected neighbors. If they are not, they are not filtered out.
// Since some data needs to be buffered while filtering the contour, the result must be accessed only after all points were added.
// Start point will always be first point in the resulting contour, but you can query if it could be suppressed and do it yourself.
//
// Example:
//   ContourChainApproxSimple<std::vector<cv::Point>> contour;
//   FECT::findContour(contour, image, start.x, start.y, -1);
//   std::vector<cv::Point>& contour_points = contour.get();
//   if (contour.do_suppress_start())
//       contour_points.erase(contour_points.begin());
//
// TVector needs to implement a small sub-set of std::vector:
//     void TVector::emplace_back(int x, int y)
template<typename TVector>
class ContourChainApproxSimple
{
	struct Point { int x; int y; };

	/*
	  direction codes: dir = dx + 3 * dy
	    \ dx -1, 0, 1
	  dy +--------------
	  -1 |   -4 -3 -2
	   0 |   -1  0  1
	   1 |    2  3  4
	 */
	static constexpr int dir_none = 0; // special value indicating unknown direction or zero move
	int dir = dir_none; // direction code of line; dir_none if line has just started
	int start_size = 0; // number of elements in start
	Point start[2]; // start points; initially empty
	Point last; // last position; initially undefined
	TVector contour; // resulting contour, write-only output
	bool is_closed = false; // indicates that contour has been closed
	bool is_start_suppressable = false; // indicates that start point of contour should have been suppressed

	int direction(const Point& last, int x, int y)
	{
		// get direction vector (dx, dy) using last position (last) and current position (x, y)
		int dx = x - last.x;
		int dy = y - last.y;

		if (std::max(std::abs(dx), std::abs(dy)) > 1)
		{
			return dir_none;
		}

		return dx + 3 * dy;
	}

	int direction(const Point& last, const Point& current)
	{
		return direction(last, current.x, current.y);
	}

public:

	ContourChainApproxSimple()
	{
		assert(direction({0, 0}, {0, 0}) == dir_none);
	};

	void emplace_back(int x, int y)
	{
		if (start_size < 2)
		{
			start[start_size++] = { x, y }; // first 2 points get stored for final checks

			if (start_size == 1) // first point initializes filter
			{
				if (is_closed)
					throw std::logic_error("Can't add point to closed contour.");

				// first call starts line
				last = { x, y };
				return;
			}
		}

		// if line continues then suppress buffered previous point,
		// but never suppress long jumps or zero-moves (usually this should not happen)
		int new_dir = direction(last, x, y);
		if (dir != new_dir || new_dir == dir_none)
		{
			contour.emplace_back(last.x, last.y);
		}

		// update filter state
		dir = new_dir;
		last = { x, y };
	}

private:

	void close()
	{
		if (!is_closed)
		{
			// check if start point should have been suppressed
			int dir1 = direction(last, start[0]);
			int dir2 = direction(start[0], start[1]);
			is_start_suppressable = dir1 == dir2 && dir1 != dir_none;
			// flush buffered last point by going back to start
			emplace_back(start[0].x, start[0].y);
			start_size = 0;
			is_closed = true;
		}
	}

public:

	// Access resulting contour after all points were added.
	TVector& get()
	{
		close();
		return contour;
	}

	// Access information if start point of resulting contour can be suppressed.
	bool do_suppress_start()
	{
		close();
		return is_start_suppressable;
	}
};
