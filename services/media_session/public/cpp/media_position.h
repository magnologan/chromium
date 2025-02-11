// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_POSITION_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_POSITION_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace media_session {

namespace mojom {
class MediaPositionDataView;
}

struct COMPONENT_EXPORT(MEDIA_SESSION_BASE_CPP) MediaPosition {
 public:
  MediaPosition();
  MediaPosition(double playback_rate,
                base::TimeDelta duration,
                base::TimeDelta position);
  ~MediaPosition();

  // Return the duration of the media.
  base::TimeDelta duration() const;

  // Return the current position of the media.
  base::TimeDelta GetPosition() const;

 private:
  friend struct mojo::StructTraits<mojom::MediaPositionDataView, MediaPosition>;
  friend class MediaPositionTest;
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestPositionUpdated);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestPositionUpdatedTwice);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestPositionUpdatedPastDuration);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestNegativePosition);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest,
                           TestPositionUpdatedFasterPlayback);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest,
                           TestPositionUpdatedSlowerPlayback);

  // Return the updated position of the media, assuming current time is
  // |time|.
  base::TimeDelta GetPositionAtTime(base::Time time) const;

  // Playback rate of the media.
  double playback_rate_;

  // Duration of the media.
  base::TimeDelta duration_;

  // Last updated position of the media.
  base::TimeDelta position_;

  // Last time |position_| was updated.
  base::Time last_updated_time_;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_POSITION_H_
