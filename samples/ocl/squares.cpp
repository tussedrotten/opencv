// The "Square Detector" program.
// It loads several images sequentially and tries to find squares in
// each image

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/ocl/ocl.hpp"

#include <iostream>
#include <math.h>
#include <string.h>

using namespace cv;
using namespace std;

void help()
{
    cout <<
        "\nA program using OCL module pyramid scaling, Canny, dilate functions, threshold, split; cpu contours, contour simpification and\n"
        "memory storage (it's got it all folks) to find\n"
        "squares in a list of images pic1-6.png\n"
        "Returns sequence of squares detected on the image.\n"
        "the sequence is stored in the specified memory storage\n"
        "Call:\n"
        "./squares\n"
        "Using OpenCV version %s\n" << CV_VERSION << "\n" << endl;
}


int thresh = 50, N = 11;
const char* wndname = "OpenCL Square Detection Demo";

// helper function:
// finds a cosine of angle between vectors
// from pt0->pt1 and from pt0->pt2
double angle( Point pt1, Point pt2, Point pt0 )
{
    double dx1 = pt1.x - pt0.x;
    double dy1 = pt1.y - pt0.y;
    double dx2 = pt2.x - pt0.x;
    double dy2 = pt2.y - pt0.y;
    return (dx1*dx2 + dy1*dy2)/sqrt((dx1*dx1 + dy1*dy1)*(dx2*dx2 + dy2*dy2) + 1e-10);
}

// returns sequence of squares detected on the image.
// the sequence is stored in the specified memory storage
void findSquares( const Mat& image, vector<vector<Point> >& squares )
{
    squares.clear();

    Mat gray;
    cv::ocl::oclMat pyr_ocl, timg_ocl, gray0_ocl, gray_ocl;

    // down-scale and upscale the image to filter out the noise
    ocl::pyrDown(ocl::oclMat(image), pyr_ocl);
    ocl::pyrUp(pyr_ocl, timg_ocl);

    vector<vector<Point> > contours;
    vector<cv::ocl::oclMat> gray0s;
    ocl::split(timg_ocl, gray0s); // split 3 channels into a vector of oclMat
    // find squares in every color plane of the image
    for( int c = 0; c < 3; c++ )
    {
        gray0_ocl = gray0s[c];
        // try several threshold levels
        for( int l = 0; l < N; l++ )
        {
            // hack: use Canny instead of zero threshold level.
            // Canny helps to catch squares with gradient shading
            if( l == 0 )
            {
                // do canny on OpenCL device
                // apply Canny. Take the upper threshold from slider
                // and set the lower to 0 (which forces edges merging)
                cv::ocl::Canny(gray0_ocl, gray_ocl, 0, thresh, 5);
                // dilate canny output to remove potential
                // holes between edge segments
                ocl::dilate(gray_ocl, gray_ocl, Mat(), Point(-1,-1));
                gray = Mat(gray_ocl);
            }
            else
            {
                // apply threshold if l!=0:
                //     tgray(x,y) = gray(x,y) < (l+1)*255/N ? 255 : 0
                cv::ocl::threshold(gray0_ocl, gray_ocl, (l+1)*255/N, 255, THRESH_BINARY);
                gray = gray_ocl;
            }

            // find contours and store them all as a list
            findContours(gray, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

            vector<Point> approx;

            // test each contour
            for( size_t i = 0; i < contours.size(); i++ )
            {
                // approximate contour with accuracy proportional
                // to the contour perimeter
                approxPolyDP(Mat(contours[i]), approx, arcLength(Mat(contours[i]), true)*0.02, true);

                // square contours should have 4 vertices after approximation
                // relatively large area (to filter out noisy contours)
                // and be convex.
                // Note: absolute value of an area is used because
                // area may be positive or negative - in accordance with the
                // contour orientation
                if( approx.size() == 4 &&
                    fabs(contourArea(Mat(approx))) > 1000 &&
                    isContourConvex(Mat(approx)) )
                {
                    double maxCosine = 0;

                    for( int j = 2; j < 5; j++ )
                    {
                        // find the maximum cosine of the angle between joint edges
                        double cosine = fabs(angle(approx[j%4], approx[j-2], approx[j-1]));
                        maxCosine = MAX(maxCosine, cosine);
                    }

                    // if cosines of all angles are small
                    // (all angles are ~90 degree) then write quandrange
                    // vertices to resultant sequence
                    if( maxCosine < 0.3 )
                        squares.push_back(approx);
                }
            }
        }
    }
}


// the function draws all the squares in the image
void drawSquares( Mat& image, const vector<vector<Point> >& squares )
{
    for( size_t i = 0; i < squares.size(); i++ )
    {
        const Point* p = &squares[i][0];
        int n = (int)squares[i].size();
        polylines(image, &p, &n, 1, true, Scalar(0,255,0), 3, CV_AA);
    }

    imshow(wndname, image);
}


int main(int /*argc*/, char** /*argv*/)
{

    //ocl::setBinpath("F:/kernel_bin");
    vector<ocl::Info> info;
    CV_Assert(ocl::getDevice(info));

    static const char* names[] = { "pic1.png", "pic2.png", "pic3.png",
        "pic4.png", "pic5.png", "pic6.png", 0 };
    help();
    namedWindow( wndname, 1 );
    vector<vector<Point> > squares;

    for( int i = 0; names[i] != 0; i++ )
    {
        Mat image = imread(names[i], 1);
        if( image.empty() )
        {
            cout << "Couldn't load " << names[i] << endl;
            continue;
        }

        findSquares(image, squares);
        drawSquares(image, squares);

        int c = waitKey();
        if( (char)c == 27 )
            break;
    }

    return 0;
}