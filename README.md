# Fast Edge-Based Contour Tracing from Seed-Pixel (FECTS)

C++ implementation of my Algorithm to trace contours in binary images in a way that is compatible with OpenCV contour functionality.

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

The resulting algorithm is quite similar to Theo Pavlidis' algorithm, but it is simpler, does fewer pixel-tests, and it can start on any pixel of the contour.

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
If value is -1, no direction dir is given and a direction is chosen automatically.
This works well if the seed pixel is part of a single contour only.
If the object to trace is very narrow and the seed pixel is touching the contour on both sides,
the side with the smallest dir is chosen.  
Note that a seed pixel can be part of up to four different contours, but no more than one of them can be an outer contour.
So if you expect an outer contour and an outer contour is found, you are good.
Otherwise you need to be more specific.

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
__In:__ if dir is a valid direction 0-3 then (x, y, dir) becomes an additional position to stop tracing  
__Out:__ (x, y, dir) is the position tracing stopped, e.g. because maximum contour length was reached.

## Definition of direction dir 0 to 3 and its offset vector (dx, dy):
```
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

For a comparison let's first summarize both algorithms in a comparable form. We follow A. Ghuneim and &mdash; without loss of generality &mdash; discuss only the case of clockwise tracing.

### Theo Pavlidis' Rules
**State** of tracing is the center of the current pixel (x,y) and the current 90° viewing direction up, right, down, or left.

Note that viewing direction "left" and "right" are relative to the image.
In all other cases, "left" and "right" are relative to the current viewing direction.
Just as "forward" &mdash; sometimes also called "ahead" &mdash; is always relative to the current viewing direction. 

**Tested Pixels** are forward-left (P1), forward (P2), and forward-right (P3).

**Rule 1 "P1":** if forward-left pixel is foreground, move to the tested pixel and turn left  
**Rule 2 "P2":** else if forward pixel is foreground, move to the tested pixel (do not turn)  
**Rule 3 "P3":** else if forward-right pixel is foreground, move to the tested pixel (do not turn)  
**Rule 4 "PN":** else turn right (do not move)

**Termination:** Tracing ends when we return to the start pixel.
Special handling of single-pixel contour objects is needed, so tracing also ends if it turned right 3 times in a row on the same pixel.
(Some implementations use [alternative termination tests](https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/theomain.html#an).)

### FECTS' Rules

**State** of tracing is one of the four edges of the current pixel (x, y) in a given 90° viewing direction.
We are located on the left side edge of the pixel.

**Tested Pixels** are forward-left and forward.

**Rule 1 "EL":** if forward-left pixel is foreground, move to the left edge (i.e. move to the tested pixel, turn left)  
**Rule 2 "EF":** else if forward pixel is foreground, move to the forward edge (i.e. move to the tested pixel, do not turn)  
**Rule 3 "ER":** else move to the right edge (i.e. stay on the same pixel, turn right)

**Termination:** Tracing ends when we reach the edge we started from (i.e. we are on the same pixel in the same viewing direction).

### Comparison

For this comparison the rules were named distinctly using the pixel or edge the rule moves to. **P1** to **P3** are the rules that move to tested **P**ixel P**1**, P**2**, and P**3** respectively. **PN** is the rule that does **N**ot move. **EL** is the rule that moves to the **L**eft **E**dge, **EF** moves to the **F**orward edge, **ER** to the **R**ight edge.

Rules P1 and EL are basically the same.

Rules P2 and EF are basically the same.

Rules PN and ER are basically the same.  

**Rule P3 is the oddball one.** FECTS would do ER instead and turn right &mdash; which makes P3 become the new forward-left pixel. For rule P3 to apply, the original forward-right pixel P3 has to be foreground, so for FECTS the new forward-left pixel is foreground and rule EL applies next. EL turns left and moves to P3. FECTS ends up in the same state as Pavlidis' rule P3.

Viewed from the edge-based point of view, rule P3 is a shortcut to do ER+EL in a single step. It is redundant, and what is more it prevents the use of a clean and simple edge-based termination criteria, because it follows two edges in a single step and therefore may skip the start edge.

Compared to P3, ER+EL does one extra step, but it does the same number of pixel-tests.

| Pavlidis | Pixel-Tests   |         FECTS | Pixel-Tests               |
|---------:|:--------------|--------------:|:--------------------------|
|       P1 |   1           |            EL | 1                         |
|       P2 |   2           |            EF | 2                         |
|       P3 |   3           | 💡&nbsp;ER+EL | 3&nbsp;=&nbsp;2+1&nbsp;✔️ |
|       PN | **3**&nbsp;❌ |           ER | **2**&nbsp;✔️             |
|      Sum | **9**         |           Sum | **8**                     |

From this table it is obvious that FECTS does fewer pixel-tests, because it can turn right with just 2 pixel-tests.

The saved pixel-tests are redundant.
For example look at ER+EF compared to PN+P2. PN+P2 does one extra pixel-test, because in this case the PN tests P3 as background, turns right &mdash; which makes pixel P3 become P1 &mdash; and then rule P2 tests pixel P1 a second time.

For the same reason PN+P1 will never happen, because if PN applies, pixel P3 is background, P3 becomes P1, and the test for rule P1 is done unnecessarily.

### Theoretical Performance Gain

How many pixel-tests are saved and how many extra steps are needed depends on the frequency of the rules.
Let's look for models that allow to do a statistical analysis.

#### Random Pixel Model

First let's assume that **for each step the pixels are randomly set** with a 50% probability.
This is a simple and not very realistic model.
White noise images are actually not very good for testing contour tracing.
The implementation was tested using a more complex random image generation scheme,
which produces more structured foreground and background regions (see examples below).

But this simple model is good for a quick first estimate,
because it gives us an independent probability for each rule.
For example rule P3 is only activated in case P1 and P2 are background and P3 is foreground.
The probability for this is now 0.5³ or 1/8th.
So based on this model, **rule P3 has a probability of about 13%** only.

Using the probability of each rule as a weight,
we get the average number of steps and the average number of pixel-tests per edge.
Then we can compare the relative amount of steps and pixel-tests between Pavlidis and FECTS.

|             Pavlidis | Steps |         Edges | Pixel-Tests   | Weight | |                FECTS | Steps | Edges | Pixel-Tests    | Weight |
|---------------------:|:-----:|--------------:|:--------------|:-------|-|---------------------:|:-----:|------:|:---------------|:-------|
|                   P1 |   1   |             1 | 1             | 1/2    | |                   EL |   1   |     1 | 1              | 1/2    |
|                   P2 |   1   |             1 | 2             | 1/4    | |                   EF |   1   |     1 | 2              | 1/4    |
|                   P3 |   1   | ✔️&nbsp;**2** | 3             | 1/8    | |                      |       |       |                |        |
|                   PN |   1   |             1 | **3**&nbsp;❌ | 1/8    | |                   ER |   1   |     1 | **2**&nbsp;✔️  | 1/4    |
| **Weighted Average** |   1   |         1.125 | 1.75          | ∑=1    | | **Weighted Average** |   1   |     1 | 1.5            | ∑=1    |
|    **Avg. per Edge** |  0.89 |             1 | 1.555         |        | |    **Avg. per Edge** |   1   |     1 | 1.5            |        |
|            **Ratio** |   1   |               | 1             |        | |            **Ratio** | 1.125 |       | 0.964          |        |
|          **Ratio-1** |   0   |               | 0             |        | |            **Ratio** | 0.125 |       | -0.036        |        |

<!--
>>> avg_edges_p = 1/2 + 1/4 + 2/8 + 1/8
>>> avg_edges_f = 1/2 + 1/4 + 1/4
>>> avg_tests_p = 1/2 + 2/4 + 3/8 + 3/8
>>> avg_tests_f = 1/2 + 2/4 + 2/4
>>> avg_edges_p, avg_tests_p, avg_edges_f, avg_tests_f
(1.125, 1.75, 1.0, 1.5)
>>> step_per_edge_p = 1/avg_edges_p
>>> tests_per_edge_p = avg_tests_p/avg_edges_p
>>> step_per_edge_f = 1/avg_edges_f
>>> tests_per_edge_f = avg_tests_f/avg_edges_f
>>> step_per_edge_p, tests_per_edge_p, step_per_edge_f, tests_per_edge_f
(0.8888888888888888, 1.5555555555555556, 1.0, 1.5)
>>> step_per_edge_f / step_per_edge_p, tests_per_edge_f / tests_per_edge_p
(1.125, 0.9642857142857143)
>>> step_per_edge_f / step_per_edge_p - 1, tests_per_edge_f / tests_per_edge_p - 1
(0.125, -0.0357142857142857)
-->

So our quick estimate is: **FECTS performs 4% fewer pixel-tests, but does 13% more steps**.
That looks promising, but for a clearer conclusion a more refined analysis may be in order.

#### Random Next Edge Model

The above simple model implied that all rules are independent.
As we have seen above, Pavlidis' rules are *not* independent.
For an improved estimate, let's assume that FECTS' rules are independent and have the same frequency.
In other words: **in every step the probability to go left, forward, or right is the same**.
While this assumption is not true for some artificial shapes like an axis-aligned rectangle,
for noisy real world shapes it is plausible to assume that we go forward in about 1/3rd of the steps.
In a closed contour, left and right turns are very much balanced,
so the two cases of going left or right get half of the remaining 2/3rd probability, i.e. 1/3rd each.

For a statistical analysis we now try to generate all possible infinite sequences of EL, EF, and ER from a small set of unambiguous finite sequences.
These finite sequences are concatenated randomly according to their probability.
Longer sequences have lower probability.
Every sequence that ends with ER may possibly continue with EL and because ER+EL is P3 it is ambiguous and needs to be split into three longer sequences.
If the sequence does not end with ER, it has an unambiguous equivalent sequence in P1, P2, P3, and PN, and we can stop making it longer.
 We will stop after four right turns, because this can happen only for a single-pixel contour object,
 i.e. ER+ER+ER+ER can not happen in an infinite sequence.
 The probability weight of ER+ER+ER+ER is adjusted from 1.3% to zero.

|       FECTS Sequence |     Steps | Pixel-Tests                  | |    Pavlidis Sequence |     Steps | Pixel-Tests                     | Weight  |
|:---------------------|----------:|:-----------------------------|-|:---------------------|----------:|:--------------------------------|:--------|
|                   EL |         1 | 1                            | |                   P1 |         1 | 1                               | ⅓       |
|                   EF |         1 | 2                            | |                   P2 |         1 | 2                               | ⅓       |
|                ER+EL | ❌&nbsp;2 | 3&nbsp;=&nbsp;2+1            | |              **P3** | ✔️&nbsp;1 | 3                               | ⅓·⅓     |
|                ER+EF |         2 | 4&nbsp;=&nbsp;2+2&nbsp;✔️    | |               PN+P2 |          2 | 5&nbsp;=&nbsp;3+2&nbsp;❌      | ⅓·⅓     |
|             ER+ER+EL | ❌&nbsp;3 | 5&nbsp;=&nbsp;2+2+1&nbsp;✔️ | |            PN+**P3** | ✔️&nbsp;2 | 6&nbsp;=&nbsp;3+3&nbsp;❌      | ⅓·⅓·⅓   |
|             ER+ER+EF |         3 | 6&nbsp;=&nbsp;2+2+2&nbsp;✔️  | |            PN+PN+P2 |          3 | 8&nbsp;=&nbsp;3+3+2&nbsp;❌    | ⅓·⅓·⅓   |
|          ER+ER+ER+EL |         4 | 7&nbsp;=&nbsp;2+2+2+1&nbsp;✔️| |        PN+PN+**P3** |          3 | 9&nbsp;=&nbsp;3+3+3&nbsp;❌    | ⅓·⅓·⅓·½ |
|          ER+ER+ER+EF |         4 | 8&nbsp;=&nbsp;2+2+2+2&nbsp;✔️| |         PN+PN+PN+P2 |          4 | 11&nbsp;=&nbsp;3+3+3+2&nbsp;❌ | ⅓·⅓·⅓·½ |
|          ER+ER+ER+ER |         4 | 8&nbsp;=&nbsp;2+2+2+2&nbsp;✔️| |         PN+PN+PN+PN |          4 | 12&nbsp;=&nbsp;3+3+3+3&nbsp;❌ | ⅓·⅓·⅓·0 |
| **Weighted Average** |     1.481 | 2.463                        | | **Weighted Average** |     1.333 | 2.778                           | ∑=1     |
| **Ratio**            |     1.111 | 0.887                        | | **Ratio**            |         1 | 1                               |         |
| **Ratio-1**          |     0.111 | -0.113                       | | **Ratio-1**          |         0 | 0                               |         |

<!--
>>> ptf = (1+2)/3+(3+4)/9+(5+6)/27+(7+8)/(27*2)  # weighted pixel-tests FECTS
>>> ptp = (1+2)/3+(3+5)/9+(6+8)/27+(9+11)/(27*2)  # weighted pixel-tests Pavlidis
>>> stf = (1+1)/3+(2+2)/9+(3+3)/27+(4+4)/(27*2)  # weighted steps FECTS
>>> stp = (1+1)/3+(1+2)/9+(2+3)/27+(4+4)/(27*2)  # weighted steps Pavlidis
>>> ptf, ptp, stf, stp
(2.462962962962963, 2.7777777777777777, 1.4814814814814816, 1.3333333333333333)
>>> stf/stp, ptf/ptp
(1.1111111111111114, 0.8866666666666666)
>>> stf/stp-1, ptf/ptp-1
(0.11111111111111138, -0.1133333333333334)
-->

In this way we *do* model that PN+P1 will never happen.
Some aspects that are less relevant for this analysis are still not modeled.
Infinite sequences model long contours, obviously. They do not model the closing of the contour.
A generated sequence may have an impossible path, e.g. one that intersects itself or leaves the image.
Also there is a slight asymmetry in forbidding ER+ER+ER+ER and allowing EL+EL+EL+EL,
but at least we keep the re-distributed probability mass in the ER+ER+ER paths.
The impact on the results should be low.

Based on this table we can also see that rule P3 does not happen very often.
It appears only in three sequences.
It can appear on its own with a probability of 1/9th, it can appear in PN+P3 with a probability of 1/36th,
and it can appear in PN+PN+P3 with a probability of 1/72th.
**Rule P3 has a probability of about 15.3%**, which is a little higher than in the simple estimate above,
because probability mass is shifted from PN+P1 to P3,
but rule P3 is still quite infrequent for a rule that causes more trouble than it solves.

So our estimate is: **FECTS performs 11.3% fewer pixel-tests, but does 11.1% more steps**.

Each extra step of FECTS causes an extra termination check.
FECTS may also have to do up to three additional steps to close the contour edge-wise.
But for this small price **FECTS terminates consistently**.

If a pixel-test is slower or faster than a termination check depends very much on the implementation,
so let's call it a draw and say: **FECTS is as fast as Pavlidis**.

### Emitting the Contour

In both algorithms we know a pixel is part of the traced contour when we reach it.
This information is contained in each rule: P1, P2, P3, EL, and EF go to a different pixel, but PN and ER do not.

Pavlidis' algorithm emits pixels when the pixel is reached.
(Look for "Insert [...] in **B**" in the
[algorithm description](https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/theomain.html#alg).)

In the given FECTS implementation a pixel is emitted into the traced contour when we leave the pixel.
This allows for efficient suppression of border-only pixels.
Pixel-based analysis can only suppress pixels that are *at* the border, but
**edge-based analysis can suppress pixels where the contour follows the image border, while keeping pixels where the contour reaches or leaves the image border**.
If you are not interested in this feature, the implementation may be minimally simplified by emitting pixels when they are reached.
(But think about: do you want the start pixel to be at the begin, the end, or both?)

### Choosing the Start State

According to
[A. Ghuneim](https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/theomain.html#restrict)
Pavlidis' algorithm needs a start pixel that is foreground and has a background pixel to its left.
In the edge-based view that means we have to start on an upward edge.
Adjusting the algorithm for other start directions seems possible, but there are still start pixels that will terminate early before the full contour is traced. 

According to the GitHub project
[geocontour](https://github.com/benkrichman/geocontour)
Pavlidis' algorithm also has problems "capturing inside corners".

[Pavlidis' algorithm](https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/theomain.html#alg)
seems to have been designed to only trace the outermost outer contour and start on the first foreground pixel that is found when scanning the image in memory order &mdash;
so technically speaking this point is the only valid start point.

**FECTS can start on any pixel edge of the contour**, no matter if it is an outer contour or an inner contour.

### Conclusion

Pavlidis' third rule is harmful. Less is more.

## Experimental Performance

The inner loop of contour tracing is optionally unrolled and optimized for speed.
To generate this optimized code a Python script is used. Use macro FECTS_GENERATOR_OPTIMIZED to control use of the optimized code.

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

```
    0 (1): 165560900 ns, 296505 pix, 558 ns/pix
    1 (2): 118987900 ns, 246920 pix, 481 ns/pix
    2 (7): 588043300 ns, 1566531 pix, 375 ns/pix
    3 (20): 434627300 ns, 1674345 pix, 259 ns/pix
    4 (54): 337932400 ns, 2192311 pix, 154 ns/pix
    5 (148): 238709100 ns, 2375023 pix, 100 ns/pix
    6 (403): 93557000 ns, 1157859 pix, 80 ns/pix
    7 (1096): 5608300 ns, 84840 pix, 66 ns/pix
    time OpenCV:  3107149100 ns, 9594334 pix, 323 ns/pix
    time FECTS:   1983026200 ns, 9594334 pix, 206 ns/pix
    time ratio: 0.638
```

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


## Compress Contours like cv::CHAIN_APPROX_SIMPLE

If you need fast on-the-fly post-processing of the contour, you should consider to implement a filtering container instead of modifying the algorithm code.

ContourChainApproxSimple.hpp implements an example of a filtering container:

```
template<typename TVector>
class ContourChainApproxSimple
```
TVector needs to implement a small sub-set of std::vector:
```
void TVector::emplace_back(int x, int y)
```

It can be used to create contours like OpenCV does with option [cv::CHAIN_APPROX_SIMPLE](https://docs.opencv.org/4.10.0/d3/dc0/group__imgproc__shape.html#ga4303f45752694956374734a03c54d5ff).

If successive points are on a vertical, horizontal, or diagonal line, only start and end point of the line are stored.
Since some data needs to be buffered while filtering the contour, the result must be accessed only after all points were added.
Start point will always be first point in the resulting contour, but you can query if it could be suppressed and do it yourself.


Example:
```
ContourChainApproxSimple<std::vector<cv::Point>> contour;
FECTS::findContour(contour, image, start.x, start.y, -1);
std::vector<cv::Point>& contour_points = contour.get();
if (contour.do_suppress_start())
    contour_points.erase(contour_points.begin());
```

ContourTracingTest.cpp contains tests for ContourChainApproxSimple.

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
