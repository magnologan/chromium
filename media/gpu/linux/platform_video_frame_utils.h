// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LINUX_PLATFORM_VIDEO_FRAME_UTILS_H_
#define MEDIA_GPU_LINUX_PLATFORM_VIDEO_FRAME_UTILS_H_

#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
struct GpuMemoryBufferHandle;
class NativePixmap;
}  // namespace gfx

namespace media {

// Create platform dependent media::VideoFrame. |buffer_usage| is passed to
// CreateNativePixmap(). See //media/base/video_frame.h for other parameters.
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Get VideoFrameLayout of platform dependent video frame with |pixel_format|,
// |coded_size| and |buffer_usage|. This function is not cost-free as this
// allocates a platform dependent video frame.
MEDIA_GPU_EXPORT base::Optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage);

// Create a shared GPU memory handle to the |video_frame|'s data.
MEDIA_GPU_EXPORT gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame);

// Create a native pixmap from the specified |video_frame|. The video frame's
// data will not be copied, the pixmap will point to the same GPU memory buffer.
scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
    const VideoFrame* video_frame);

}  // namespace media

#endif  // MEDIA_GPU_LINUX_PLATFORM_VIDEO_FRAME_UTILS_H_
