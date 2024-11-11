# Fast Edge-Oriented Contour Tracing from Seed-Pixel (FECTS)

Implementation of my Algorithm to trace contours in binary images in C++ in a way that is compatible with OpenCV contour functionality.

## Introduction

[Wikipedia](https://en.wikipedia.org/wiki/Boundary_tracing) also calls it "boundary tracing".


It is also known as "border following" or "boundary following" according to
the
[Online Tutorial](https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/index.html)
published by Abeer George Ghuneim.

An interesting overview is presented in the paper ["Fast Contour-Tracing Algorithm Based on a Pixel-Following Method for Image Sensors"](https://pmc.ncbi.nlm.nih.gov/articles/PMC4813928/) published 2016 by Jonghoon Seo, Seungho Chae, Jinwook Shim, Dongchul Kim, Cheolho Cheong, and Tack-Don Han.

[OpenCV]()
has a function
[cv::findContours()](https://docs.opencv.org/4.10.0/d3/dc0/group__imgproc__shape.html#gadf1ad6a0b82947fa1fe3c3d497f260e0)
that implements several algorithms to trace all 8-connected contours in a binary image. Its usage is described in this [tutorial](https://docs.opencv.org/4.10.0/df/d0d/tutorial_find_contours.html).

I was not able to find a free C/C++ implementation of a function that traces a single contour starting from a seed point. After getting tired of searching the internet, I did my own, published it here, and I guess it may be better than many other implementations. This topic has been researched for decades, much of it in the bad old days when "paper with code" was unheard of and papers were hidden behind pay-walls or not online at all. The key words for searching were different, and they probably are different again in patent lawyer language, so it is hard to know for sure. But my implementation is online, it is fast, and it is tested against OpenCV.

The basic idea is to trace along edges instead of jumping from pixel to pixel following not always very intuitive rules. We follow along the border lines of the current foreground pixel and the background and decide onto which edge we will go next. So at every corner junction where four pixels join, we have to decide: go left, go forward, or go right. Only three options. And once we have defined that the foreground is 8-connected, the contour line is clearly defined and easy to follow. However we do not output the edges we travel on, we only emit the foreground pixels we encounter on this journey. The tracing is finished when we reach the start edge again. That's all, basically.

The resulting algorithm is quite similar to Theo Pavlidis' algorithm, but it is simpler, does fewer pixel tests, and it can start on any pixel of the contour.

## Function

The contour tracing function is implemented in ContourTracing.hpp mostly using C style.
It is a C++ template function to allow it to be easily used with OpenCV and STL,
but turning it into a pure C function should be easy enough.

```
template<typename TContour, typename TImage>
int findContour(
    TContour& contour, // output
    TImage const& image, // binary image
    int x, int y, int dir = -1, bool clockwise = false, // oriented seed edge
    bool do_suppress_border = false, // omit border from contour
    stop_t* stop = NULL) // optional advanced termination control
```

**contour:** Receives the resulting contour points. It should be initially empty if contour tracing starts new (but no check is done).
**TContour** needs to implement a small sub-set of std::vector&lt;cv::Point&gt;:
```
    void TContour::emplace_back(int x, int y)
```
**image:** Single channel 8 bit read access to the image to trace contour in.
Pixel with non-zero value are foreground. All other pixels including those outside of image are background.
**TImage** needs to implement a small sub-set of cv::Mat and expects continuous row-major single 8-bit channel raster image memory:
```
    int TImage::rows; // number of rows, i.e. image height
    int TImage::cols; // number of columns, i.e. image width
    uint8_t* TImage::ptr(int row, int column) // get pointer to pixel in image at row y and column x; row/column counting starts at zero
```
**x, y:** Seed pixel coordinate.
Usually seed pixel (x,y) is taken as the start pixel, but if (x,y) touches the contour only by a corner
(but not by an edge), the start pixel is moved one pixel forward in the given (or automatically chosen) direction
to ensure the resulting contour is consistently 8-connected thin.
The start pixel will be the first pixel in contour, unless it has only contour edges at the image border and do_suppress_border is set.

**dir:** Direction to start contour tracing with. 0 is up, 1 is right, 2 is down, 3 is left.
If value is -1, no direction dir is given and a direction is chosen automatically,
which usually gives you what you expect, unless the object to trace is very narrow and the
seed pixel is touching the contour on both sides, in which case tracing will start on the side with the smallest dir.

**clockwise:** Indicates if outer contours are traced clockwise or counterclockwise.
Note that inner contours run in the opposite direction.
If tracing is clockwise, the traced edge is to the left of the current pixel (looking in the current direction),
otherwise the traced edge is to the right.
Set it to false to trace similar to OpenCV cv::findContours.

**do_suppress_border:** Indicates to omit pixels of the contour that are followed on border edges only.
The contour still contains border pixels where it arrives at the image border or where it leaves tha image border,
but not those pixel that only follow the border.

**stop:** Structure to control stop behavior and to return extra information on the state of tracing at the end.

**Return value:** The total difference between left and right turns done during tracing is returned.
If a contour is traced completely, i.e. it is traced until it returns to the start edge,
the value is 4 for an outer contour and -4 if it is an inner contour.
When tracing stops due to stop.max_contour_length the contour is usually not traced completely.
Even if all pixels have been found, up to 3 final edge tracing turns may not have been done,
so if you somehow know that all pixels have been found, you can still use the sign of the return value
to decide if it is an outer or inner contour.

```
struct stop_t
{
    int max_contour_length = -1;

    int dir = -1;
    int x;
    int y;
};
```
**max_contour_length:**
Usually the full contour is traced. Sometimes it is useful to limit the length of the contour, e.g. to limit time and memory usage. Set it to zero to only do startup logic like choosing a valid start direction.  
__In:__ if >= 0 then the maximum allowed contour length  
__Out:__ number of traced contour pixels including suppressed pixels

**x, y, dir:**
Usually tracing stops when the start position is reached.
Sometimes it is useful to stop at another known position on the contour.  
__In:__ if dir is a valid direction 0-3 then (x, y, dir) becomes the position to stop tracing - be careful!  
__Out:__ (x, y, dir) is the position tracing stopped, e.g. because maximum contour length was reached.

## Definition of direction dir 0 to 3 and its offset vector (dx, dy):
```
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
```

## Tracing Rules

```
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
```

## Comparison with Theo Pavlidis' Algorithm

The book
["Algorithms for Graphics and Image Processing"](https://doi.org/10.1007/978-3-642-93208-3)
from 1982 is not online, but there is a nice description of
[Theo Pavlidis' algorithm](https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/theo.html)
by A. Ghuneim.

TODO

According to the GitHub project [geocontour](https://github.com/benkrichman/geocontour) Pavlidis' algorithm has problems "capturing inside corners".

## Performance

The inner loop of contour tracing is optionally unrolled and optimized for speed.
To generate this optimized code a Python script is used. Use macro FEPCT_GENERATOR_OPTIMIZED to control use of the optimized code.

To minimize border checking operations in the optimized code, a border test is done as a first step called rule 0:

```
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
```

On some CPU architectures this optimization results in only moderate speed-up. Best speed-up is achieved on long contours.

Speed tests on "Intel(R) Celeron(R) CPU J1900 1.99GHz" using OpenCV 4.3.0 without GPU and compiled with Visual Studio Community 2015 still shows that tracing all image contours in a randomly generated image is 30% faster with the current implementation than OpenCV (provided seed points of all contours are known):

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


## Functional Testing

ContourTracingTest.cpp implements tests, including extensive tests to check that tracing results are the same as in OpenCV using random images.
Green pixel show initial contour as found by OpenCV. Red shows step by step the contour of our algorithm. They are identical in the end. More saturated color indicates that a pixel is in the contour more than once.

![Random test image with an animated example of contour tracing comparison.](./README/test/examples/outer-contour-00427.apng)

More examples for outer contours:

![Example](./README/test/examples/outer-contour-00365-cropped.apng)
![Example](./README/test/examples/outer-contour-00314-cropped.apng)
![Example](./README/test/examples/outer-contour-00241-cropped.apng)
![Example](./README/test/examples/outer-contour-00213-cropped.apng)
![Example](./README/test/examples/outer-contour-00156-cropped.apng)
![Example](./README/test/examples/outer-contour-00152-cropped.apng)
![Example](./README/test/examples/outer-contour-00133-cropped.apng)
![Example](./README/test/examples/outer-contour-00207-cropped.apng)
![Example](./README/test/examples/outer-contour-00084-cropped.apng)
![Example](./README/test/examples/outer-contour-00078-cropped.apng)
![Example](./README/test/examples/outer-contour-00028-cropped.apng)
![Example](./README/test/examples/outer-contour-00023-cropped.apng)
![Example](./README/test/examples/outer-contour-00015-cropped.apng)
![Example](./README/test/examples/outer-contour-00012-cropped.apng)
![Example](./README/test/examples/outer-contour-00010-cropped.apng)

Some examples for inner contours:

![Example](./README/test/examples/inner-contour-00044-cropped.apng)
![Example](./README/test/examples/inner-contour-00038-cropped.apng)
![Example](./README/test/examples/inner-contour-00033-cropped.apng)
![Example](./README/test/examples/inner-contour-00019-cropped.apng)
![Example](./README/test/examples/inner-contour-00016-cropped.apng)
![Example](./README/test/examples/inner-contour-00015-cropped.apng)
![Example](./README/test/examples/inner-contour-00014-cropped.apng)
![Example](./README/test/examples/inner-contour-00009-cropped.apng)
![Example](./README/test/examples/inner-contour-00007-cropped.apng)
![Example](./README/test/examples/inner-contour-00006-cropped.apng)
![Example](./README/test/examples/inner-contour-00004-cropped.apng)


## Tracing contour of a 4-connected object

The current implementation does not support it.
To trace contour pixel of a 4-connected foreground area, the rules need to be changed.
For clockwise tracing they could become something like:
```
if forward pixel is not foreground
    turn right
else if forward-left pixel is foreground
    turn left
else
    move ahead 
```

Pixel emission would also change a little. Rules could be like:
```
if forward pixel is not foreground
    turn right
else if forward-left pixel is foreground
    emit current pixel
    emit forward pixel (only if you want a 4-connected contour)
    move to forward-left pixel
    turn left
else
    emit current pixel
    move ahead
```

For counterclockwise tracing the rules change similarly in that left is swapped with right.
The rules for border suppression and optimized border checking should be similar too.

Since OpenCV did not see any need to support 4-connected object contour tracing,
a different way of testing the result needs to be found.

As a workaround you might consider to invert the image and trace the background contour.
The resulting contour line is still 8-connected, but the contour line goes around the 4-connected object and all contour pixel are background, i.e. the contour will be "grown" outwards.
Maybe your application would work better with eroding the inverted mask one pixel,
but it still wouldn't be exactly the same result in the end.
