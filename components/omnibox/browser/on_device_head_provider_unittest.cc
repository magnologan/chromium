// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_provider.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/on_device_head_serving.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

class OnDeviceHeadProviderTest : public testing::Test,
                                 public AutocompleteProviderListener {
 protected:
  void SetUp() override {
    client_.reset(new FakeAutocompleteProviderClient());
    SetTestOnDeviceHeadModel();
    provider_ = OnDeviceHeadProvider::Create(client_.get(), this);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    provider_ = nullptr;
    client_.reset();
    scoped_task_environment_.RunUntilIdle();
  }

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override {
    // No action required.
  }

  void SetTestOnDeviceHeadModel() {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    // The same test model also used in ./on_device_head_serving_unittest.cc.
    file_path = file_path.AppendASCII("components/test/data/omnibox");
    ASSERT_TRUE(base::PathExists(file_path));
    OnDeviceHeadProvider::OverrideEnumDirOnDeviceHeadSuggestForTest(file_path);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<OnDeviceHeadProvider> provider_;
};

TEST_F(OnDeviceHeadProviderTest, ServingInstanceNotCreated) {
  AutocompleteInput input(base::UTF8ToUTF16("a"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillOnce(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled()).WillOnce(Return(true));

  provider_->Start(input, false);
  if (!provider_->done())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, RejectSynchronousRequest) {
  AutocompleteInput input(base::UTF8ToUTF16("a"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(false);

  provider_->Start(input, false);
  if (!provider_->done())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, RejectIncognito) {
  AutocompleteInput input(base::UTF8ToUTF16("a"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillOnce(Return(true));

  provider_->Start(input, false);
  if (!provider_->done())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, NoMatches) {
  AutocompleteInput input(base::UTF8ToUTF16("b"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillOnce(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled()).WillOnce(Return(true));

  provider_->Start(input, false);
  if (!provider_->done())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, HasMatches) {
  AutocompleteInput input(base::UTF8ToUTF16("M"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillOnce(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled()).WillOnce(Return(true));

  provider_->Start(input, false);
  if (!provider_->done())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(base::UTF8ToUTF16("maps"), provider_->matches()[0].contents);
  EXPECT_EQ(base::UTF8ToUTF16("mail"), provider_->matches()[1].contents);
  EXPECT_EQ(base::UTF8ToUTF16("map"), provider_->matches()[2].contents);
}

TEST_F(OnDeviceHeadProviderTest, CancelInProgressRequest) {
  AutocompleteInput input1(base::UTF8ToUTF16("g"),
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input1.set_want_asynchronous_matches(true);
  AutocompleteInput input2(base::UTF8ToUTF16("m"),
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input2.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  provider_->Start(input1, false);
  EXPECT_FALSE(provider_->done());
  provider_->Start(input2, false);

  if (!provider_->done())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(base::UTF8ToUTF16("maps"), provider_->matches()[0].contents);
  EXPECT_EQ(base::UTF8ToUTF16("mail"), provider_->matches()[1].contents);
  EXPECT_EQ(base::UTF8ToUTF16("map"), provider_->matches()[2].contents);
}
