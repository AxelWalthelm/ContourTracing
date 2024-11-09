#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include "HighResolutionTimer.h"

#define SAVE_IMAGES 0
#if SAVE_IMAGES
#include <direct.h>
#endif

static bool TEST_expects_error = false;
void TEST_ErrorHandler(const char* failed_expression, const char* error_message, const char* function_name, const char* file_name, int line_number)
{
	if (!error_message || !error_message[0])
		error_message = "FEPCT_Assert failed";
	if (!TEST_expects_error)
		printf("%s: %s in function %s: %s(%d)\n", error_message, failed_expression, function_name, file_name, line_number);
	throw std::exception(error_message);
	exit(-1);
}
#define FEPCT_Assert(expr,msg) do { if(!!(expr)) ; else TEST_ErrorHandler(#expr, (msg), __func__, __FILE__, __LINE__ ); } while(0)

/*
Speed tests on "Intel(R) Celeron(R) CPU J1900 1.99GHz" show that
hand/script optimized code is (only) a few percent faster, especially on longer contours.
Other processors and architectures may benefit more from optimized code. (ARM, smaller RISC processors, ...?)
*/
#define FEPCT_GENERATOR_OPTIMIZED 1
#include "ContourTracing.hpp"

#include "ContourChainApproxSimple.hpp"

static bool TEST_failed = false;

void TEST_FailHandler(const char* failed_expression, const char* function_name, const char* file_name, int line_number)
{
	TEST_failed = true;
	printf("TEST failed: %s in function %s: %s(%d)\n", failed_expression, function_name, file_name, line_number);
}

#define TEST(expr) do { if(TEST_failed || !!(expr)) ; else TEST_FailHandler(#expr, __func__, __FILE__, __LINE__ ); } while(0)

struct TEST_ERROR_Guard
{
	TEST_ERROR_Guard() { TEST_expects_error = true; }
	~TEST_ERROR_Guard() { TEST_expects_error = false; }
};

#define TEST_ERROR(code,message) do{TEST_ERROR_Guard _;if(!TEST_failed){bool throws=false;std::string error;try{code;}catch(std::exception&x){throws=true;error=x.what();}catch(...){throws=true;};TEST(throws && error == message);if(TEST_failed){if(throws)printf("Error: \"%s\".\n",error.c_str());else printf("No error.\n");}}}while(0)
#define TEST_NO_ERROR(code) do{TEST_ERROR_Guard _;if(!TEST_failed){bool throws=false;std::string error;try{code;}catch(std::exception&x){throws=true;error=x.what();}catch(...){throws=true;};TEST(!throws);if(TEST_failed)printf("Error: \"%s\".\n",error.c_str());}}while(0)


template <typename TData>
bool is_contained(TData const& item, std::vector<TData> const& vector)
{
	return std::find(vector.begin(), vector.end(), item) != vector.end();
}

template <typename TData>
bool have_equal_items(std::vector<TData> const& vector1, std::vector<TData> const& vector2)
{
	if (vector1.size() != vector2.size())
		return false;

	for (auto const& p: vector1)
		if (!is_contained(p, vector2))
			return false;

	for (auto const& p: vector2)
		if (!is_contained(p, vector1))
			return false;

	return true;
}

bool is_direct_neighbor_or_equal(cv::Point const& p1, cv::Point const& p2)
{
	int dx = std::abs(p1.x - p2.x);
	int dy = std::abs(p1.y - p2.y);
	return std::min(dx, dy) == 0 && std::max(dx, dy) <= 1;
}

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	int size = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
	if( size <= 0 )
		throw std::runtime_error( "string_format error" );
	std::unique_ptr<char[]> buf( new char[ size_t(size) ] );
	std::snprintf( buf.get(), size_t(size), format.c_str(), args ... );
	return std::string( buf.get(), buf.get() + size_t(size) - 1 );
}

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
	const double join_pixel_fraction = 0.5)
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

bool TEST_showFailed(cv::Mat& image, const std::vector<cv::Point>& contour, const std::vector<cv::Point>& expected_contour, int contour_index, bool do_animate_both = false)
{
	if (!TEST_failed)
		return false;

	TEST_showFailed(image, contour, expected_contour, contour_index, do_animate_both);
	return true;
}

void TEST_show(cv::Mat& image, const std::vector<cv::Point>& contour, const std::vector<cv::Point>& expected_contour, int contour_index, bool do_animate_both = false
#if SAVE_IMAGES
	, std::string save_path = "", bool do_crop = false
#endif
)
{
	printf("  contour_index=%d traced %d of %d\n", contour_index, int(contour.size()), int(expected_contour.size()));

#if SAVE_IMAGES

	cv::Rect roi;
	if (do_crop)
	{
		cv::Rect bbx1 = cv::boundingRect(contour);
		cv::Rect bbx2 = cv::boundingRect(expected_contour);
		std::vector<cv::Point> points = { bbx1.tl(), bbx1.br(), bbx2.tl(), bbx2.br() };
		roi = cv::boundingRect(points);

		int border = 5;
		roi -= cv::Point(border, border);
		roi += cv::Size(2 * border, 2 * border);

		roi &= cv::Rect(0, 0, image.cols, image.rows);
	}

	if (!save_path.empty())
	{
		int status = _mkdir(save_path.c_str()); // must not exist - delete old folder before new run
		if (status != 0)
			throw std::exception("_mkdir failed");
	}

#endif

	for (int i = -1; i < int(std::max(expected_contour.size(), contour.size())); i++)
	{
		cv::Mat display;
		cv::cvtColor(image, display, cv::COLOR_GRAY2BGR);

		drawContourTransparent(display, expected_contour, cv::Scalar(0, 255, 0), do_animate_both && i >= 0 ? i + 1 : -1);
		drawContourTransparent(display, contour, cv::Scalar(0, 0, 255), i + 1);

#if SAVE_IMAGES
		if (do_crop)
			display = cv::Mat(display, roi);
#endif

		cv::namedWindow("display", cv::WINDOW_NORMAL);
		cv::imshow("display", display);

#if SAVE_IMAGES
		if (!save_path.empty())
		{
			int f = std::min(5, 1024 / display.cols);
			if (f > 0)
				cv::resize(display, display, cv::Size(), f, f, cv::INTER_NEAREST);
			bool ok = cv::imwrite(save_path + string_format("\\image%05d.png", i + 1), display);
			if (!ok)
				throw std::exception("imwrite failed");
#ifndef NDEBUG
			cv::waitKey(100);
#endif
		}
		else
#endif
		{
			int key = cv::waitKey();
			if (key < 0 || key == 27)
				break;
		}
	}
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
	std::srand(471142);

#if false // test logBin
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

	// test some small cases
	//////////////////////////

	int turns = 666;

	// empty image
	if (!TEST_failed)
	{
		{
			cv::Mat image(0, 0, CV_8UC1);
			std::vector<cv::Point> contour;
			TEST_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, -1, false, false), "image is empty");
			TEST(turns == 666);
		}
		{
			cv::Mat image(0, 9, CV_8UC1);
			std::vector<cv::Point> contour;
			TEST_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, -1, false, false), "image is empty");
		}
		{
			cv::Mat image(9, 0, CV_8UC1);
			std::vector<cv::Point> contour;
			TEST_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, -1, false, false), "image is empty");
		}
	}

	// seed pixel is outside of image
	if (!TEST_failed)
	{
		cv::Mat image(1, 1, CV_8UC1, cv::Scalar(255));
		std::vector<cv::Point> contour;
		TEST_ERROR(turns = FEPCT::findContour(contour, image, 0, 2, -1, false, false), "seed pixel is outside of image");
		TEST_ERROR(turns = FEPCT::findContour(contour, image, 2, 0, -1, false, false), "seed pixel is outside of image");
		TEST_ERROR(turns = FEPCT::findContour(contour, image, 0, -2, -1, false, false), "seed pixel is outside of image");
		TEST_ERROR(turns = FEPCT::findContour(contour, image, -2, 0, -1, false, false), "seed pixel is outside of image");
	}

	// single pixel image
	if (!TEST_failed)
	{
		// zero is background
		cv::Mat image(1, 1, CV_8UC1, cv::Scalar(0));
		std::vector<cv::Point> contour;
		TEST_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, -1, false, false), "seed pixel is not foreground");

		// non-zero is foreground
		setPixel(image, 0, 0, 1);
		contour.clear();
		FEPCT::stop_t stop;
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, -1, false, false, &stop));
		TEST(contour.size() == 1);
		TEST(contour[0] == cv::Point(0, 0));
		TEST(stop.max_contour_length == 1);
		TEST(stop.dir == 0);
		TEST(stop.x == 0);
		TEST(stop.y == 0);
		TEST(turns == 4);

		// suppress border
		contour.clear();
		stop = FEPCT::stop_t();
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, -1, false, true, &stop));
		TEST(contour.size() == 0);
		TEST(stop.max_contour_length == 1);
		TEST(stop.dir == 0);
		TEST(stop.x == 0);
		TEST(stop.y == 0);
		TEST(turns == 4);
	}

	// single pixel high image
	if (!TEST_failed)
	{
		// 5x1 image, 3 center pixels are set
		cv::Mat image(1, 5, CV_8UC1, cv::Scalar(255));
		setPixel(image, 0, 0, 0);
		setPixel(image, 4, 0, 0);

		// start in the middle
		std::vector<cv::Point> contour;
		FEPCT::stop_t stop;
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 2, 0, -1, false, false, &stop));
		TEST(contour.size() == 4);
		TEST(contour[0] == cv::Point(2, 0));
		TEST(contour[1] == cv::Point(3, 0));
		TEST(contour[2] == cv::Point(2, 0));
		TEST(contour[3] == cv::Point(1, 0));
		TEST(stop.max_contour_length == 4);
		TEST(stop.dir == 1);
		TEST(stop.x == 2);
		TEST(stop.y == 0);
		TEST(turns == 4);

		// suppress border
		contour.clear();
		stop = FEPCT::stop_t();
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 2, 0, -1, false, true, &stop));
		TEST(contour.size() == 2);
		TEST(contour[0] == cv::Point(3, 0));
		TEST(contour[1] == cv::Point(1, 0));
		TEST(stop.max_contour_length == 4);
		TEST(stop.dir == 1);
		TEST(stop.x == 2);
		TEST(stop.y == 0);
		TEST(turns == 4);

		// suppress border clockwise gives the same result
		contour.clear();
		stop = FEPCT::stop_t();
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 2, 0, -1, true, true, &stop));
		TEST(contour.size() == 2);
		TEST(contour[0] == cv::Point(3, 0));
		TEST(contour[1] == cv::Point(1, 0));
		TEST(stop.max_contour_length == 4);
		TEST(stop.dir == 1);
		TEST(stop.x == 2);
		TEST(stop.y == 0);
		TEST(turns == 4);

		// test cv::CHAIN_APPROX_SIMPLE
		ContourChainApproxSimple<std::vector<cv::Point>> simple_contour;
		TEST_NO_ERROR(turns = FEPCT::findContour(simple_contour, image, 2, 0, -1, false, false));
		std::vector<cv::Point>& contour_points = simple_contour.get();
		if (simple_contour.do_suppress_start())
			contour_points.erase(contour_points.begin());
		TEST(contour.size() == 2);
		TEST(contour[0] == cv::Point(3, 0));
		TEST(contour[1] == cv::Point(1, 0));
		TEST(turns == 4);
		TEST_ERROR(simple_contour.emplace_back(0, 0), "Can't add point to closed contour.");
	}

	// single pixel wide image
	if (!TEST_failed)
	{
		// 1x5 image, 3 center pixels are set
		cv::Mat image(5, 1, CV_8UC1, cv::Scalar(255));
		setPixel(image, 0, 0, 0);
		setPixel(image, 0, 4, 0);

		// start in the middle
		std::vector<cv::Point> contour;
		FEPCT::stop_t stop;
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 2, -1, false, false, &stop));
		TEST(contour.size() == 4);
		TEST(contour[0] == cv::Point(0, 2));
		TEST(contour[1] == cv::Point(0, 1));
		TEST(contour[2] == cv::Point(0, 2));
		TEST(contour[3] == cv::Point(0, 3));
		TEST(stop.max_contour_length == 4);
		TEST(stop.dir == 0);
		TEST(stop.x == 0);
		TEST(stop.y == 2);
		TEST(turns == 4);

		// suppress border
		contour.clear();
		stop = FEPCT::stop_t();
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 2, -1, false, true, &stop));
		TEST(contour.size() == 2);
		TEST(contour[0] == cv::Point(0, 1));
		TEST(contour[1] == cv::Point(0, 3));
		TEST(stop.max_contour_length == 4);
		TEST(stop.dir == 0);
		TEST(stop.x == 0);
		TEST(stop.y == 2);
		TEST(turns == 4);

		// suppress border clockwise gives the same result
		contour.clear();
		stop = FEPCT::stop_t();
		TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 2, -1, true, true, &stop));
		TEST(contour.size() == 2);
		TEST(contour[0] == cv::Point(0, 1));
		TEST(contour[1] == cv::Point(0, 3));
		TEST(stop.max_contour_length == 4);
		TEST(stop.dir == 0);
		TEST(stop.x == 0);
		TEST(stop.y == 2);
		TEST(turns == 4);
	}

	// 3x3 image
	if (!TEST_failed)
	{
		// bad seed pixel
		{
			cv::Mat image(3, 3, CV_8UC1, cv::Scalar(255));

			// start in the middle
			std::vector<cv::Point> contour;
			TEST_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, -1, false, false), "bad seed pixel");
			TEST(contour.size() == 0);
		}

		// bad seed
		{
			cv::Mat image(3, 3, CV_8UC1, cv::Scalar(255));

			// start in the middle
			std::vector<cv::Point> contour;
			TEST_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, 0, false, false), "bad seed");
			TEST(contour.size() == 0);
		}

		// upper-left corner is background
		{
			cv::Mat image(3, 3, CV_8UC1, cv::Scalar(255));
			setPixel(image, 0, 0, 0);

			// start in the middle, trace clockwise, direction not given
			std::vector<cv::Point> contour;
			FEPCT::stop_t stop;
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, -1, true, false, &stop));
			TEST(contour.size() == 7);
			TEST(contour[0] == cv::Point(1, 0));
			TEST(contour[1] == cv::Point(2, 0));
			TEST(contour[2] == cv::Point(2, 1));
			TEST(contour[3] == cv::Point(2, 2));
			TEST(contour[4] == cv::Point(1, 2));
			TEST(contour[5] == cv::Point(0, 2));
			TEST(contour[6] == cv::Point(0, 1));
			TEST(stop.dir == 0);
			TEST(stop.x == 1);
			TEST(stop.y == 0);
			TEST(stop.max_contour_length == 7);
			TEST(turns == 4);

			// start in the middle, trace clockwise, direction given
			contour.clear();
			stop = FEPCT::stop_t();
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, 0, true, false, &stop));
			TEST(contour.size() == 7);
			TEST(contour[0] == cv::Point(1, 0));
			TEST(contour[1] == cv::Point(2, 0));
			TEST(contour[2] == cv::Point(2, 1));
			TEST(contour[3] == cv::Point(2, 2));
			TEST(contour[4] == cv::Point(1, 2));
			TEST(contour[5] == cv::Point(0, 2));
			TEST(contour[6] == cv::Point(0, 1));
			TEST(stop.dir == 0);
			TEST(stop.x == 1);
			TEST(stop.y == 0);
			TEST(stop.max_contour_length == 7);
			TEST(turns == 4);

			// start in the middle, trace clockwise
			contour.clear();
			stop = FEPCT::stop_t();
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, -1, true, false, &stop));
			TEST(contour.size() == 7);
			TEST(contour[0] == cv::Point(1, 0));
			TEST(contour[6] == cv::Point(0, 1));
			TEST(stop.dir == 0);
			TEST(turns == 4);

			// start in the middle, trace clockwise, suppress border
			contour.clear();
			stop = FEPCT::stop_t();
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, 0, true, true, &stop));
			TEST(contour.size() == 2);
			TEST(contour[0] == cv::Point(1, 0));
			TEST(contour[1] == cv::Point(0, 1));
			TEST(stop.dir == 0);
			TEST(stop.max_contour_length == 7);
			TEST(turns == 4);
		}
		// upper-right corner is background
		{
			cv::Mat image(3, 3, CV_8UC1, cv::Scalar(255));
			setPixel(image, 2, 0, 0);

			// start in the middle, trace clockwise
			std::vector<cv::Point> contour;
			FEPCT::stop_t stop;
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, -1, true, false, &stop));
			TEST(contour.size() == 7);
			TEST(stop.dir == 1);
			TEST(turns == 4);
		}
		// lower-right corner is background
		{
			cv::Mat image(3, 3, CV_8UC1, cv::Scalar(255));
			setPixel(image, 2, 2, 0);

			// start in the middle, trace clockwise
			std::vector<cv::Point> contour;
			FEPCT::stop_t stop;
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, -1, true, false, &stop));
			TEST(contour.size() == 7);
			TEST(stop.dir == 2);
			TEST(turns == 4);
		}
		// lower-left corner is background
		{
			cv::Mat image(3, 3, CV_8UC1, cv::Scalar(255));
			setPixel(image, 0, 2, 0);

			// start in the middle, trace clockwise, determine start direction separately
			std::vector<cv::Point> contour;
			FEPCT::stop_t stop;
			stop.max_contour_length = 0;
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, -1, true, false, &stop));
			TEST(contour.size() == 0);
			TEST(stop.dir == 3);
			TEST(stop.x == 0);
			TEST(stop.y == 1);
			TEST(stop.max_contour_length == 0);
			TEST(turns == 0);

			// stop early using max_contour_length
			contour.clear();
			FEPCT::stop_t stop2;
			stop2.max_contour_length = 5;
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, 3, true, false, &stop2));
			TEST(contour.size() == 5);
			TEST(stop2.dir == 2);
			TEST(turns == 3);

			// stop early using (x, y, dir)
			contour.clear();
			stop2.max_contour_length = -1;
			TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 1, 1, 3, true, false, &stop2));
			TEST(contour.size() == 5);
			TEST(stop2.dir == 2);
			TEST(turns == 3);
		}
	}

	// 5x5 inner contour
	if (!TEST_failed)
	{
		cv::Mat image(5, 5, CV_8UC1, cv::Scalar(255));
		setPixel(image, 2, 2, 0);

		std::vector<cv::Point> expected_contour = { {2, 1}, {3, 2}, {2, 3}, {1, 2} };

		for (int start_x = 1; start_x < 4; start_x++)
		{
			for (int start_y = 1; start_y < 4; start_y++)
			{
				if (start_x == 2 && start_y == 2)
				{
					std::vector<cv::Point> contour;
					TEST_ERROR(turns = FEPCT::findContour(contour, image, start_x, start_y, -1, false, false), "seed pixel is not foreground");
					continue;
				}

				// start in the middle, trace clockwise, determine start direction
				{
					std::vector<cv::Point> contour;
					TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, start_x, start_y, -1, true, false));
					TEST(contour.size() == 4);
					TEST(have_equal_items(contour, expected_contour));
					TEST(is_direct_neighbor_or_equal(contour[0], cv::Point(start_x, start_y)));
					TEST(turns == -4); // inner contour
				}

				// start in the middle, trace counterclockwise, determine start direction
				{
					std::vector<cv::Point> contour;
					TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, start_x, start_y, -1, false, false));
					TEST(contour.size() == 4);
					TEST(have_equal_items(contour, expected_contour));
					TEST(is_direct_neighbor_or_equal(contour[0], cv::Point(start_x, start_y)));
					TEST(turns == -4); // inner contour
				}
			}
		}
	}

	// 7x7 spiral
	if (!TEST_failed)
	{
		uint8_t data[7*7] = {
			1, 1, 1, 1, 1, 1, 2,
			0, 0, 0, 0, 0, 0, 2,
			5, 5, 5, 5, 6, 0, 2,
			4, 0, 0, 0, 6, 0, 2,
			4, 0, 9, 7, 7, 0, 2,
			4, 0, 0, 0, 0, 0, 2,
			4, 3, 3, 3, 3, 3, 3,
		};
		cv::Mat image(7, 7, CV_8UC1, data);

		// trace clockwise
		{
			int expected_turns[54] = {
				1, 1, 1, 1, 1, 1,
				2, 2, 2, 2, 2, 2,
				3, 3, 3, 3, 3, 3,
				4, 4, 4, 4,
				5, 5, 5, 5,
				6, 6,
				7, 7,
				9,
				8,
				7, 7, 7,
				6, 6, 6,
				5, 5, 5, 5, 5,
				4, 4, 4, 4, 4,
				3, 3, 3, 3, 3, 3
			};

			// trace from (0, 0) completely
			std::vector<cv::Point> one_step_contour;
			TEST_NO_ERROR(turns = FEPCT::findContour(one_step_contour, image, 0, 0, 0, true, false));
			TEST(turns == 4);
			TEST(one_step_contour.size() == 54);

			// trace from (0, 0) i steps
			{
				for (int i = 0; i < 54; i++)
				{
					std::vector<cv::Point> contour;
					FEPCT::stop_t stop;
					stop.max_contour_length = i + 1;
					TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, 0, true, false, &stop));
					TEST(int(contour.size()) == i + 1);
					TEST(contour[i] == one_step_contour[i]);
					TEST(turns == expected_turns[i]);
				}
			}

			// trace from (0, 0) step by step
			{
				std::vector<cv::Point> contour;
				turns = 0;
				int x = 0;
				int y = 0;
				int dir = 0;
				for (int i = 0; i < 54; i++)
				{
					FEPCT::stop_t stop;
					stop.max_contour_length = 1;
					TEST_NO_ERROR(turns += FEPCT::findContour(contour, image, x, y, dir, true, false, &stop));
					TEST(int(contour.size()) == i + 1);
					TEST(contour[i] == one_step_contour[i]);
					TEST(turns == expected_turns[i]);
					x = stop.x;
					y = stop.y;
					dir = stop.dir;
				}
			}
		}

		// trace counterclockwise
		{
			int expected_turns[54] = {
				1, 1, 1, 1, 1,
				0, 0, 0, 0, 0,
				-1, -1, -1, -1, -1,
				-2, -2, -2,
				-3, -3, -3,
				-4,
				-5, -5,
				-3, -3,
				-2, -2,
				-1, -1, -1, -1,
				0, 0, 0, 0,
				1, 1, 1, 1, 1, 1,
				2, 2, 2, 2, 2, 2,
				3, 3, 3, 3, 3, 3
			};

			// trace from (0, 0) completely
			std::vector<cv::Point> one_step_contour;
			TEST_NO_ERROR(turns = FEPCT::findContour(one_step_contour, image, 0, 0, 2, false, false));
			TEST(turns == 4);
			TEST(one_step_contour.size() == 54);

			// trace from (0, 0) i steps
			{
				for (int i = 0; i < 54; i++)
				{
					std::vector<cv::Point> contour;
					FEPCT::stop_t stop;
					stop.max_contour_length = i + 1;
					TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, 0, 0, 2, false, false, &stop));
					TEST(int(contour.size()) == i + 1);
					TEST(contour[i] == one_step_contour[i]);
					TEST(turns == expected_turns[i]);
					if (TEST_failed)
						printf("  i=%d\n", i);
				}
			}

			// trace from (0, 0) step by step
			{
				std::vector<cv::Point> contour;
				turns = 0;
				int x = 0;
				int y = 0;
				int dir = 2;
				for (int i = 0; i < 54; i++)
				{
					FEPCT::stop_t stop;
					stop.max_contour_length = 1;
					TEST_NO_ERROR(turns += FEPCT::findContour(contour, image, x, y, dir, false, false, &stop));
					TEST(int(contour.size()) == i + 1);
					TEST(contour[i] == one_step_contour[i]);
					TEST(turns == expected_turns[i]);
					x = stop.x;
					y = stop.y;
					dir = stop.dir;
				}
			}
		}
	}

	if (TEST_failed)
	{
		printf("TEST FAILED!\n");
		return -1;
	}

	// compare results with OpenCV using randomly generated images
	/////////////////////////////////////////////////////////////////

	cv::Mat image(100, 200, CV_8UC1);

	uint64_t duration_OpenCV = 0;
	int duration_OpenCV_count = 0;
	uint64_t duration_FEPCT_bins[logBinsMax] = { 0 };
	int duration_FEPCT_counts[logBinsMax] = { 0 };

	for (int test = 0; test < 1000; test++)
	{
		printf("test=%d\n", test);

		setRandom(image);

#if SAVE_IMAGES
		cv::imwrite(string_format("C:\\tmp\\test-image-%05d.png", test), image);
#endif

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
				TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, start.x, start.y, dir, clockwise, false, test_stop ? &stop : NULL));
				duration_FEPCT_bins[bin] += GetHighResolutionTimeElapsedNs(timer_start);
				duration_FEPCT_counts[bin] += int(expected_contour.size());

				TEST(contour.size() == expected_contour.size());
				for (int i = 0; i < int(expected_contour.size()) && !TEST_failed; i++)
				{
					TEST(contour[i] == expected_contour[i]);
					if (TEST_failed)
						printf("  i=%d\n", i);
				}

				TEST(stop.max_contour_length == (test_stop ? int(expected_contour.size()) : -1));

				TEST(turns == (is_outer ? 4 : -4));

#if SAVE_IMAGES
				static size_t max_size[2] = { 3, 9 };
				if (max_size[is_outer] < contour.size())
				{
					max_size[is_outer] = contour.size();
					printf("max_size[%d]=%zd\n", int(is_outer), max_size[is_outer]);
					TEST_show(image, contour, expected_contour, contour_index, false, string_format("C:\\tmp\\%s-contour-%05zd", is_outer ? "outer" : "inner", max_size[is_outer]));
					TEST_show(image, contour, expected_contour, contour_index, false, string_format("C:\\tmp\\%s-contour-%05zd-cropped", is_outer ? "outer" : "inner", max_size[is_outer]), true);

					// Examples to generate animated PNG:
					// ffmpeg.exe -framerate 5 -i image%05d.png -plays 0 -final_delay 2.0 -f apng -lavfi split[v],palettegen,[v]paletteuse out.apng
					// for /d %d in (C:\tmp\inner-contour-* C:\tmp\outer-contour-*) do ffmpeg.exe -framerate 5 -i %d\image%05d.png -plays 0 -final_delay 2.0 -f apng -lavfi split[v],palettegen,[v]paletteuse %d\..\%~nxd.apng
			}
#endif

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

				turns = 0;

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
					TEST_NO_ERROR(turns += FEPCT::findContour(contour, image, start.x, start.y, dir, clockwise, false, &stop));
					TEST(int(contour.size()) == after);

					for (int i = before; i < after && !TEST_failed; i++)
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

				if (!TEST_failed)
				{
					// We did not fully close the contour, we just traced until we reached the start pixel again,
					// so up to 3 turns at the end of the contour may not have been done.
					if (is_outer)
						TEST(turns > 0);
					else
						TEST(turns < 0);

					if (TEST_failed)
						printf("turns=%d, dir=%d, start_dir=%d\n", turns, dir, is_outer ? 2 : 0);
				}

				if (TEST_showFailed(image, contour, expected_contour, contour_index, true))
					break;
			}

			// trace from start point clockwise
			/////////////////////////////////////
			{
				cv::Point start = expected_contour[0];
				int dir = is_outer ? 0 : 2;
				bool clockwise = true;
				std::vector<cv::Point> contour;
				TEST_NO_ERROR(turns = FEPCT::findContour(contour, image, start.x, start.y, dir, clockwise, false));

				int expected_size = int(expected_contour.size());
				TEST(contour.size() == expected_size);
				for (int i = 0; i < expected_size && !TEST_failed; i++)
				{
					int ii = i == 0 ? 0 : expected_size - i; // corresponding index in clockwise contour
					TEST(contour[ii] == expected_contour[i]);
					if (TEST_failed)
						printf("  i=%d ii=%d\n", i, ii);
				}

				TEST(turns == (is_outer ? 4 : -4));

				if (TEST_showFailed(image, contour, expected_contour, contour_index))
					break;
			}
		}

		// test cv::CHAIN_APPROX_SIMPLE
		//////////////////////////////////
		contours.clear();
		hierarchy.clear();
		cv::findContours(image, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
		for (int contour_index = 0; contour_index < int(contours.size()); contour_index++)
		{
			std::vector<cv::Point> expected_contour = contours[contour_index];
			bool is_outer = hierachy_level(hierarchy, contour_index) % 2 == 0;

			cv::Point start = expected_contour[0];
			int dir = is_outer ? 2 : 0;
			bool clockwise = false; // outer contours are counterclockwise, but inner contours are clockwise
			bool expect_start_suppressed = false; // due to the way OpenCV selects start points of outer contours
			if (!is_outer)
			{
				int dx = expected_contour[0].x - (expected_contour.back()).x;
				int dy = expected_contour[0].y - (expected_contour.back()).y;
				if (dx > 0 && dy == -dx)
				{
					expect_start_suppressed = true; // due to the way OpenCV selects start points of inner contours
					start.x--;
					start.y++;
				}
			}

			ContourChainApproxSimple<std::vector<cv::Point>> simple_contour;
			TEST_NO_ERROR(turns = FEPCT::findContour(simple_contour, image, start.x, start.y, dir, clockwise, false));
			std::vector<cv::Point>& contour = simple_contour.get();
			TEST(simple_contour.do_suppress_start() == expect_start_suppressed);
			if (simple_contour.do_suppress_start())
				contour.erase(contour.begin());
			TEST(contour.size() == expected_contour.size());
			for (int i = 0; i < int(expected_contour.size()) && !TEST_failed; i++)
			{
				TEST(contour[i] == expected_contour[i]);
				if (TEST_failed)
					printf("  i=%d\n", i);
			}

			TEST(turns == (is_outer ? 4 : -4));

			if (TEST_failed)
				printf("  contour_index=%d is_outer=%d expected#=%zd found#=%zd\n", contour_index, is_outer, expected_contour.size(), contour.size());

			if (TEST_showFailed(image, contour, expected_contour, contour_index, true))
				break;
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
				duration_FEPCT_bins[bin] / std::max(1, duration_FEPCT_counts[bin]));
			duration_FEPCT += duration_FEPCT_bins[bin];
			duration_FEPCT_count += duration_FEPCT_counts[bin];
		}
		printf("time OpenCV: %11lld ns, %d pix, %lld ns/pix\n", duration_OpenCV, duration_OpenCV_count, duration_OpenCV / duration_OpenCV_count);
		printf("time FEPCT:  %11lld ns, %d pix, %lld ns/pix\n", duration_FEPCT, duration_FEPCT_count, duration_FEPCT / duration_FEPCT_count);
		printf("time ratio: %.3f\n", double(duration_FEPCT) / double(duration_OpenCV));

		// Speed test on "Intel(R) Celeron(R) CPU J1900 1.99GHz" using OpenCV 4.3.0 without GPU and compiled with Visual Studio Community 2015:
		/*
			0 (1): 165560900 ns, 296505 pix, 558 ns/pix
			1 (2): 118987900 ns, 246920 pix, 481 ns/pix
			2 (7): 588043300 ns, 1566531 pix, 375 ns/pix
			3 (20): 434627300 ns, 1674345 pix, 259 ns/pix
			4 (54): 337932400 ns, 2192311 pix, 154 ns/pix
			5 (148): 238709100 ns, 2375023 pix, 100 ns/pix
			6 (403): 93557000 ns, 1157859 pix, 80 ns/pix
			7 (1096): 5608300 ns, 84840 pix, 66 ns/pix
			time OpenCV:  3107149100 ns, 9594334 pix, 323 ns/pix
			time FEPCT:   1983026200 ns, 9594334 pix, 206 ns/pix
			time ratio: 0.638
		*/

		if (TEST_failed)
			break;
	}

	if (TEST_failed)
		printf("TEST FAILED!\n");
	else
		printf("TEST OK\n");

	return 0;
}

