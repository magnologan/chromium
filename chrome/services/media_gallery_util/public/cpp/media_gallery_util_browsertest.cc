// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/services/media_gallery_util/public/cpp/media_parser_provider.h"
#include "chrome/services/media_gallery_util/public/mojom/constants.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/system_connector.h"
#include "media/media_buildflags.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_FFMPEG)
extern "C" {
#include <libavutil/cpu.h>
}
#endif

namespace {

using MediaGalleryUtilBrowserTest = InProcessBrowserTest;

class TestMediaParserProvider : public MediaParserProvider {
 public:
  TestMediaParserProvider() = default;

  chrome::mojom::MediaParser* GetMediaParser(
      service_manager::Connector* connector) {
    DCHECK(!quit_loop_);
    base::RunLoop run_loop;
    quit_loop_ = run_loop.QuitClosure();
    RetrieveMediaParser(connector);
    run_loop.Run();
    return media_parser();
  }

 private:
  void OnMediaParserCreated() override { std::move(quit_loop_).Run(); }

  void OnConnectionError() override { std::move(quit_loop_).Run(); }

  base::Closure quit_loop_;
};

}  // namespace

// Tests that the MediaParserProvider class used by the client library classes
// does initialize the CPU info correctly.
IN_PROC_BROWSER_TEST_F(MediaGalleryUtilBrowserTest, TestThirdPartyCpuInfo) {
  TestMediaParserProvider media_parser_provider;
  chrome::mojom::MediaParser* media_parser =
      media_parser_provider.GetMediaParser(content::GetSystemConnector());

  base::RunLoop run_loop;
  media_parser->GetCpuInfo(base::BindOnce(
      [](base::Closure quit_closure, int64_t libyuv_cpu_flags,
         int64_t ffmpeg_cpu_flags) {
        int64_t expected_ffmpeg_cpu_flags = 0;
#if BUILDFLAG(ENABLE_FFMPEG)
        expected_ffmpeg_cpu_flags = av_get_cpu_flags();
#endif
        EXPECT_EQ(expected_ffmpeg_cpu_flags, ffmpeg_cpu_flags);
        EXPECT_EQ(libyuv::InitCpuFlags(), libyuv_cpu_flags);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}
