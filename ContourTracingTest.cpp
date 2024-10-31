#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include "ContourTracing.hpp"

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

void drawContourTransparent(cv::Mat& image, std::vector<cv::Point> contour, cv::Scalar color)
{
	int b = int(color[0]) / 2;
	int g = int(color[1]) / 2;
	int r = int(color[2]) / 2;

	for (cv::Point p: contour)
	{
		cv::Vec3b& pixel = image.at<cv::Vec3b>(p.y, p.x);
		pixel[0] = pixel[0] / 2 + b;
		pixel[1] = pixel[1] / 2 + g;
		pixel[2] = pixel[2] / 2 + r;
	}
}

int main()
{
	std::srand(471142);
	cv::Mat image(100, 200, CV_8UC1);

	while (true)
	{
		setRandom(image);

		std::vector<std::vector<cv::Point>> contours;
		std::vector<cv::Vec4i> hierarchy;
		cv::findContours(image, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);
		printf("contours %d\n", int(contours.size()));
		printf("hierarchy %dx4\n", int(hierarchy.size()));

		cv::Mat display;
		cv::cvtColor(image, display, cv::COLOR_GRAY2BGR);
		//cv::drawContours(display, contours, -1, cv::Scalar(0, 0, 255), 1, cv::LINE_8, hierarchy, 1);

		std::vector<int> contour_indexes;
		for (int contour_index = 0; contour_index < int(contours.size()); contour_index++)
		{
			if (hierachy_level(hierarchy, contour_index) % 2 == 1)
			{
				contour_indexes.push_back(contour_index);
			}
		}

		for (int contour_index: contour_indexes)
		{
			//cv::drawContours(display, contours, contour_index, randomSaturatedColor(), 1, cv::LINE_8);
			drawContourTransparent(display, contours[contour_index], randomSaturatedColor());
		}

		cv::resize(display, display, cv::Size(), 2.0, 2.0, cv::INTER_NEAREST);
		for (int contour_index: contour_indexes)
		{
			cv::Point p = contours[contour_index][0];
			cv::Vec3b& pixel = display.at<cv::Vec3b>(2 * p.y, 2 * p.x);
			pixel[0] = 1;
			pixel[1] = 1;
			pixel[2] = 1;

			if (contours[contour_index].size() > 1)
			{
				cv::Point p = contours[contour_index][1];
				cv::Vec3b& pixel = display.at<cv::Vec3b>(2 * p.y + 1, 2 * p.x);
				pixel[0] = 1;
				pixel[1] = 1;
				pixel[2] = 1;
			}
		}

		std::vector<cv::Point> contour;
		//FEPCT::findContour(contour, image, 0, 0, -1, true);

		if (contour_indexes.size() == 0)
			continue;

		printf("drawn %d\n", int(contour_indexes.size()));

		cv::namedWindow("image", cv::WINDOW_NORMAL);
		cv::imshow("image", image);
		cv::namedWindow("display", cv::WINDOW_NORMAL);
		cv::imshow("display", display);
		int key = cv::waitKey();
		if (key < 0 || key == 27)
			break;
	}

	return 0;
}

