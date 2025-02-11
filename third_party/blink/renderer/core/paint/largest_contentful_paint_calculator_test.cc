// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"

#include "base/test/simple_test_tick_clock.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class LargestContentfulPaintCalculatorTest : public RenderingTest {
 public:
  void SetUp() override {
    // Advance the clock so we do not assign null TimeTicks.
    simulated_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void SetImage(const char* id, int width, int height) {
    ToHTMLImageElement(GetDocument().getElementById(id))
        ->SetImageForTest(CreateImageForTest(width, height));
  }

  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_resource =
        ImageResourceContent::CreateLoaded(
            StaticBitmapImage::Create(image).get());
    return original_image_resource;
  }

  bool IsLastReportedImage() {
    return GetLargestContentfulPaintCalculator()->last_type_ ==
           LargestContentfulPaintCalculator::LargestContentType::kImage;
  }

  bool IsLastReportedText() {
    return GetLargestContentfulPaintCalculator()->last_type_ ==
           LargestContentfulPaintCalculator::LargestContentType::kText;
  }

  uint64_t LargestImageSize() {
    return GetLargestContentfulPaintCalculator()->LargestImageSize();
  }

  uint64_t LargestTextSize() {
    return GetLargestContentfulPaintCalculator()->LargestTextSize();
  }

  void SimulateImageSwapPromise() {
    auto* image_detector = GetFrame()
                               .View()
                               ->GetPaintTimingDetector()
                               .GetImagePaintTimingDetector();
    image_detector->ReportSwapTime(image_detector->last_registered_frame_index_,
                                   WebWidgetClient::SwapResult::kDidSwap,
                                   simulated_clock_.NowTicks());
  }

  void SimulateTextSwapPromise() {
    auto* text_detector = GetFrame()
                              .View()
                              ->GetPaintTimingDetector()
                              .GetTextPaintTimingDetector();
    text_detector->ReportSwapTime(WebWidgetClient::SwapResult::kDidSwap,
                                  simulated_clock_.NowTicks());
  }

 private:
  LargestContentfulPaintCalculator* GetLargestContentfulPaintCalculator() {
    return GetFrame()
        .View()
        ->GetPaintTimingDetector()
        .GetLargestContentfulPaintCalculator();
  }

  base::SimpleTestTickClock simulated_clock_;
};

TEST_F(LargestContentfulPaintCalculatorTest, SingleImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
  )HTML");
  SetImage("target", 100, 150);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();

  EXPECT_TRUE(IsLastReportedImage());
  EXPECT_EQ(LargestImageSize(), 15000u);
  EXPECT_EQ(LargestTextSize(), 0u);
}

TEST_F(LargestContentfulPaintCalculatorTest, SingleText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <p>This is some text</p>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  SimulateTextSwapPromise();
  EXPECT_TRUE(IsLastReportedText());
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageLargerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  EXPECT_TRUE(IsLastReportedImage());
  SimulateTextSwapPromise();

  EXPECT_TRUE(IsLastReportedText());
  EXPECT_EQ(LargestImageSize(), 9u);
  EXPECT_GT(LargestTextSize(), 9u);
}

TEST_F(LargestContentfulPaintCalculatorTest, ImageSmallerText) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  EXPECT_TRUE(IsLastReportedImage());
  SimulateTextSwapPromise();

  // Text should not be reported, since it is smaller than the image.
  EXPECT_TRUE(IsLastReportedImage());
  EXPECT_EQ(LargestImageSize(), 20000u);
  EXPECT_GT(LargestTextSize(), 0u);
}

TEST_F(LargestContentfulPaintCalculatorTest, TextLargerImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>.</p>
  )HTML");
  SetImage("target", 100, 200);
  UpdateAllLifecyclePhasesForTest();
  SimulateTextSwapPromise();
  EXPECT_TRUE(IsLastReportedText());
  SimulateImageSwapPromise();

  EXPECT_TRUE(IsLastReportedImage());
  EXPECT_EQ(LargestImageSize(), 20000u);
  EXPECT_GT(LargestTextSize(), 0u);
}

TEST_F(LargestContentfulPaintCalculatorTest, TextSmallerImage) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='target'/>
    <p>This text should be larger than the image!!!!</p>
  )HTML");
  SetImage("target", 3, 3);
  UpdateAllLifecyclePhasesForTest();
  SimulateTextSwapPromise();
  EXPECT_TRUE(IsLastReportedText());
  SimulateImageSwapPromise();

  // Image should not be reported, since it is smaller than the text.
  EXPECT_TRUE(IsLastReportedText());
  EXPECT_EQ(LargestImageSize(), 9u);
  EXPECT_GT(LargestTextSize(), 9u);
}

TEST_F(LargestContentfulPaintCalculatorTest, LargestImageRemoved) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='large'/>
    <img id='small'/>
    <p>Larger than the second image</p>
  )HTML");
  SetImage("large", 100, 200);
  SetImage("small", 3, 3);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  SimulateTextSwapPromise();
  // Image is larger than the text.
  EXPECT_TRUE(IsLastReportedImage());
  EXPECT_EQ(LargestImageSize(), 20000u);
  EXPECT_GT(LargestTextSize(), 9u);

  GetDocument().getElementById("large")->remove();
  UpdateAllLifecyclePhasesForTest();
  // The LCP should now be the text because it is larger than the remaining
  // image.
  EXPECT_TRUE(IsLastReportedText());
  EXPECT_EQ(LargestImageSize(), 9u);
  EXPECT_GT(LargestTextSize(), 9u);
}

TEST_F(LargestContentfulPaintCalculatorTest, LargestTextRemoved) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <img id='medium'/>
    <p id='large'>
      This text element should be larger than than the image!\n
      These words ensure that this is the case.\n
      But the image will be larger than the other paragraph!
    </p>
    <p id='small'>.</p>
  )HTML");
  SetImage("medium", 10, 5);
  UpdateAllLifecyclePhasesForTest();
  SimulateImageSwapPromise();
  SimulateTextSwapPromise();
  // Test is larger than the image.
  EXPECT_TRUE(IsLastReportedText());
  EXPECT_EQ(LargestImageSize(), 50u);
  EXPECT_GT(LargestTextSize(), 50u);

  GetDocument().getElementById("large")->remove();
  UpdateAllLifecyclePhasesForTest();
  // The LCP should now be the image because it is larger than the remaining
  // text.
  EXPECT_TRUE(IsLastReportedImage());
  EXPECT_EQ(LargestImageSize(), 50u);
  EXPECT_LT(LargestTextSize(), 50u);
}

}  // namespace blink
