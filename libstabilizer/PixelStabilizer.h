//=======================================================================
// Copyright Lars Mescheder 2015.
// Distributed under the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#ifndef PIXELSTABILIZER_H
#define PIXELSTABILIZER_H

#include "opencv2/opencv.hpp"
#include "Stabilizer.h"

class PixelStabilizer : public Stabilizer
{
public:
  PixelStabilizer(const cv::Mat &frame_0, Warping warping);


protected:
  virtual cv::Mat get_next_homography(const cv::Mat& next_frame);
  virtual void create_visualization();

 private:
  using Vec8f = cv::Vec<float, 8>;
  using Matx88f = cv::Matx<float, 8, 8>;

  Matx88f Ainv_;
  cv::Mat_<Vec8f> Ji_;

private:
  void init (const cv::Mat& frame);

  static constexpr float EPS = 1e-5;
  static constexpr int MAXITER = 10000;
};

#endif // PIXELSTABILIZER_H
