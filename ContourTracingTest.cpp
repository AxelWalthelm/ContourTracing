#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include "HighResolutionTimer.h"

/*
Speed tests on "Intel(R) Celeron(R) CPU J1900 1.99GHz" show that
hand/script optimized code is (only) a few percent faster, especially on longer contours.
Other processors and architectures may benefit more from optimized code. (ARM, smaller RISC processors, ...?)
*/
#define FEPCT_GENERATOR_OPTIMIZED 1
#include "ContourTracing.hpp"


static bool TEST_failed = false;

void TEST_ErrorHandler(const char* failed_expression, const char* function_name, const char* file_name, int line_number)
{
	TEST_failed = true;
	printf("TEST failed: %s in function %s: %s(%d)\n", failed_expression, function_name, file_name, line_number);
}

#define TEST(expr) do { if(TEST_failed || !!(expr)) ; else TEST_ErrorHandler(#expr, __func__, __FILE__, __LINE__ ); } while(0)


int getPixel(const cv::Mat& image, int x, int y, int border = -1)
{
	if (x < 0 || y < 0 || x >= image.cols || y >= image.rows)
		return border;

	return image.at<uint8_t>(y, x);
}

void setPixel(cv::Mat& image, int x, int y, int value)
{
	if (x < 0 || y < 0 || x >= image.cols || y >= image.rows)
		return;

	image.at<uint8_t>(y, x) = value;
}

void setRandom(cv::Mat& image,
	const double seed_zero_probability = 0.65,
	const double join_pixel_fraction = 0.1)
{
	uint8_t* stop = &image.ptr()[image.rows * image.cols];
	for (uint8_t* p = image.ptr(); p < stop; p++)
	{
		*p = std::rand() < (RAND_MAX + 1) * seed_zero_probability ? 0 : 255;
	}

	for (int todo = int(image.rows * image.cols * join_pixel_fraction); todo > 0; todo--)
	{
		int x = std::rand() % image.cols;
		int y = std::rand() % image.rows;

		int sum = 0;
		int count = 0;
		for (int dx = -1; dx <= 1; dx++)
		{
			for (int dy = -1; dy <= 1; dy++)
			{
				int v = getPixel(image, x + dx, y + dy);
				if (v < 0)
					continue;

				sum += v > 0;
				count += 1;
			}
		}

		setPixel(image, x, y, sum * 2 > count ? 255 : 0);
	}
}

int rand_int(int maximum)
{
	return int((std::rand() / double(RAND_MAX + 1)) * (maximum + 1));
}

cv::Scalar randomSaturatedColor()
{
	int rgb[3] = {0, 255, rand_int(255)};
	int i = rand_int(1);
	if (i < 1)
		std::swap(rgb[0], rgb[1]);
	i = rand_int(2);
	if (i < 2)
		std::swap(rgb[i], rgb[2]);

	return cv::Scalar(rgb[2], rgb[1], rgb[0]);
}

/* [...] For each i-th contour contours[i], the elements
hierarchy[i][0] , hierarchy[i][1] , hierarchy[i][2] , and hierarchy[i][3] are set to 0-based indices
in contours of the next and previous contours at the same hierarchical level, the first child
contour and the parent contour, respectively. If for the contour i there are no next, previous,
parent, or nested contours, the corresponding elements of hierarchy[i] will be negative. */
enum hierarchy_members
{
	hierarchy_next = 0,
	hierarchy_previous = 1,
	hierarchy_first_child = 2,
	hierarchy_parent = 3,
};

int hierachy_level(const std::vector<cv::Vec4i>& hierarchy, int contour_index)
{
	int level = 0;

	while (true)
	{
		int parent_index = hierarchy[contour_index][hierarchy_parent];
		if (parent_index < 0)
			return level;

		++level;
		contour_index = parent_index;
	}
}

void drawContourTransparent(cv::Mat& image, const std::vector<cv::Point>& contour, cv::Scalar color, int max_length = -1)
{
	int b = int(color[0]) / 2;
	int g = int(color[1]) / 2;
	int r = int(color[2]) / 2;

	int len = std::min(int(contour.size()), max_length < 0 ? INT_MAX : max_length);
	for (int i = 0; i < len; i++)
	{
		cv::Point p = contour[i];
		cv::Vec3b& pixel = image.at<cv::Vec3b>(p.y, p.x);
		pixel[0] = pixel[0] / 2 + b;
		pixel[1] = pixel[1] / 2 + g;
		pixel[2] = pixel[2] / 2 + r;
	}
}

void drawContourSingleChannel(cv::Mat& image, const std::vector<cv::Point>& contour, int channel_index, int channel_value)
{
	for (cv::Point p: contour)
	{
		cv::Vec3b& pixel = image.at<cv::Vec3b>(p.y, p.x);
		pixel[channel_index] = channel_value;
	}
}

bool TEST_showFailed(cv::Mat& image, const std::vector<cv::Point>& contour, const std::vector<cv::Point>& expected_contour, int contour_index)
{
	if (!TEST_failed)
		return false;

	printf("  contour_index=%d\n", contour_index);

	for (int i = -1; i < int(std::max(expected_contour.size(), contour.size())); i++)
	{
		cv::Mat display;
		cv::cvtColor(image, display, cv::COLOR_GRAY2BGR);

		drawContourTransparent(display, expected_contour, cv::Scalar(0, 255, 0));
		drawContourTransparent(display, contour, cv::Scalar(0, 0, 255), i);
		cv::namedWindow("display", cv::WINDOW_NORMAL);
		cv::imshow("display", display);
		int key = cv::waitKey();
		if (key < 0 || key == 27)
			break;
	}

	return true;
}

constexpr int logBinsMax = 711;  // logBin(std::numeric_limits<double>().max())+1

int logBin(double value)
{
	if (value <= 1.0)
		return 0;
	if (std::isinf(value))
		value = std::numeric_limits<double>().max();
	return int(std::ceil(std::log(value)));
}

double logBinUpperLimit(int bin)
{
	if (bin < 0)
		return 1.0;
	return std::exp(double(bin));
}


int main()
{
#if true // test logBin
	{
		for (int i = -1; i < logBinsMax; i++)
		{
			double v = logBinUpperLimit(i);
			int ii = logBin(v);
			printf("%d => %g => %d\n", i, v, ii);
			TEST(ii == std::max(0, i));
			if (std::isinf(v))
			{
				TEST(i == logBinsMax - 1);
				break;
			}
		}

		if (TEST_failed)
		{
			printf("TEST FAILED!\n");
			return 0;
		}

		printf("TEST OK\n");
	}
#endif

	std::srand(471142);
	cv::Mat image(100, 200, CV_8UC1);

	uint64_t duration_OpenCV = 0;
	int duration_OpenCV_count = 0;
	uint64_t duration_FEPCT_bins[logBinsMax] = { 0 };
	int duration_FEPCT_counts[logBinsMax] = { 0 };

	for (int test = 0; test < 1000; test++)
	{
		printf("test=%d\n", test);

		setRandom(image);

		std::vector<std::vector<cv::Point>> contours;
		std::vector<cv::Vec4i> hierarchy;
		HighResolutionTime_t timer_start = GetHighResolutionTime();
		cv::findContours(image, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE); // cv::RETR_CCOMP has similar speed
		duration_OpenCV += GetHighResolutionTimeElapsedNs(timer_start);
		for (std::vector<cv::Point> contour: contours)
			duration_OpenCV_count += int(contour.size());

		printf("contours %d\n", int(contours.size()));
		printf("hierarchy %dx4\n", int(hierarchy.size()));

		for (int contour_index = 0; contour_index < int(contours.size()); contour_index++)
		{
			std::vector<cv::Point> expected_contour = contours[contour_index];
			bool is_outer = hierachy_level(hierarchy, contour_index) % 2 == 0;

			// trace from start point
			/////////////////////////////////////
			{
				cv::Point start = expected_contour[0];
				int dir = is_outer ? 2 : 0;
				bool clockwise = false;
				std::vector<cv::Point> contour;
				FEPCT::stop_t stop;
				bool test_stop = rand_int(1) == 1;
				int bin = logBin(double(expected_contour.size()));
				timer_start = GetHighResolutionTime();
				FEPCT::findContour(contour, image, start.x, start.y, dir, clockwise, false, test_stop ? &stop : NULL);
				duration_FEPCT_bins[bin] += GetHighResolutionTimeElapsedNs(timer_start);
				duration_FEPCT_counts[bin] += int(expected_contour.size());

				TEST(contour.size() == expected_contour.size());
				for (int i = 0; i < int(expected_contour.size() && !TEST_failed); i++)
				{
					TEST(contour[i] == expected_contour[i]);
					if (TEST_failed)
						printf("  i=%d\n", i);
				}

				TEST(stop.max_contour_length == (test_stop ? int(expected_contour.size()) : -1));

				if (TEST_showFailed(image, contour, expected_contour, contour_index))
					break;
			}

			// trace from start in small random steps
			///////////////////////////////////////////
			{
				cv::Point start = expected_contour[0];
				int dir = is_outer ? 2 : 0;
				bool clockwise = false;
				std::vector<cv::Point> contour;

				while (contour.size() < expected_contour.size())
				{
					int before = int(contour.size());
					int step = rand_int(5);
					int after = before + step;
					if (after > int(expected_contour.size()))
					{
						after = int(expected_contour.size());
						step = after - before;
					}
					FEPCT::stop_t stop;
					stop.max_contour_length = step;
					FEPCT::findContour(contour, image, start.x, start.y, dir, clockwise, false, &stop);
					TEST(int(contour.size()) == after);

					for (int i = 0; i < after && !TEST_failed; i++)
					{
						TEST(contour[i] == expected_contour[i]);
						if (TEST_failed)
							printf("  i=%d before=%d step=%d after=%d\n", i, before, step, after);
					}

					TEST(stop.max_contour_length == step);

					if (TEST_showFailed(image, contour, expected_contour, contour_index))
						break;

					start.x = stop.x;
					start.y = stop.y;
					dir = stop.dir;
				}
			}
		}

		uint64_t duration_FEPCT = 0;
		int duration_FEPCT_count = 0;
		int max_bin = logBinsMax - 1;
		while (max_bin >= 0 && duration_FEPCT_counts[max_bin] == 0)
			--max_bin;
		for (int bin = 0; bin <= max_bin; bin++)
		{
			printf("  %d (%d): %lld ns, %d pix, %lld ns/pix\n",
				bin, int(logBinUpperLimit(bin)), duration_FEPCT_bins[bin], duration_FEPCT_counts[bin],
				duration_FEPCT_bins[bin] / duration_FEPCT_counts[bin]);
			duration_FEPCT += duration_FEPCT_bins[bin];
			duration_FEPCT_count += duration_FEPCT_counts[bin];
		}
		printf("time OpenCV: %11lld ns, %d pix, %lld ns/pix\n", duration_OpenCV, duration_OpenCV_count, duration_OpenCV / duration_OpenCV_count);
		printf("time FEPCT:  %11lld ns, %d pix, %lld ns/pix\n", duration_FEPCT, duration_FEPCT_count, duration_FEPCT / duration_FEPCT_count);
		printf("time ratio: %.3f\n", double(duration_FEPCT) / double(duration_OpenCV));

		if (TEST_failed)
			break;
	}

	if (TEST_failed)
		printf("TEST FAILED!\n");
	else
		printf("TEST OK\n");

	return 0;
}

