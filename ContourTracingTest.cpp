#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include "ContourTracing.hpp"

int main()
{
	std::srand(471142);
	cv::Mat image(100, 200, CV_8UC1);
	uint8_t* stop = &image.ptr()[image.rows * image.cols];
	for (uint8_t* p = image.ptr(); p < stop; p++)
	{
		*p = std::rand() < RAND_MAX * 0.75 ? 0 : 255;
	}

	std::vector<cv::Point> contour;
	FEPCT::findContour(contour, image, 0, 0, -1, true);

	cv::namedWindow("image", cv::WINDOW_NORMAL);
	cv::imshow("image", image);
	cv::waitKey();
	return 0;
}

