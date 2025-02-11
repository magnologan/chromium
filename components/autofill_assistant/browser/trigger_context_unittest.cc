// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(TriggerContextTest, Empty) {
  auto empty = TriggerContext::CreateEmpty();
  ASSERT_TRUE(empty);

  google::protobuf::RepeatedPtrField<ScriptParameterProto> parameters_proto;
  empty->AddParameters(&parameters_proto);
  EXPECT_THAT(parameters_proto, IsEmpty());

  EXPECT_EQ("", empty->experiment_ids());
}

TEST(TriggerContextTest, Create) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  parameters["c"] = "d";
  auto context = TriggerContext::Create(parameters, "exps");
  ASSERT_TRUE(context);

  google::protobuf::RepeatedPtrField<ScriptParameterProto> parameters_proto;
  context->AddParameters(&parameters_proto);
  EXPECT_THAT(parameters_proto, SizeIs(2));

  EXPECT_EQ("a", parameters_proto.Get(0).name());
  EXPECT_EQ("b", parameters_proto.Get(0).value());
  EXPECT_EQ("c", parameters_proto.Get(1).name());
  EXPECT_EQ("d", parameters_proto.Get(1).value());

  EXPECT_EQ("b", context->GetParameter("a").value_or(""));
  EXPECT_EQ("d", context->GetParameter("c").value_or(""));
  EXPECT_FALSE(context->GetParameter("not_found"));

  EXPECT_EQ("exps", context->experiment_ids());
}

TEST(TriggerContextTest, Merge) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  auto context1 = TriggerContext::Create(parameters, "exp1");

  parameters.clear();
  parameters["c"] = "d";
  auto context2 = TriggerContext::Create(parameters, "exp2");

  // Adding empty to make sure empty contexts are properly skipped
  auto empty = TriggerContext::CreateEmpty();

  auto merged = TriggerContext::Merge(
      {empty.get(), context1.get(), context2.get(), empty.get()});

  ASSERT_TRUE(merged);

  google::protobuf::RepeatedPtrField<ScriptParameterProto> parameters_proto;
  merged->AddParameters(&parameters_proto);
  EXPECT_THAT(parameters_proto, SizeIs(2));

  EXPECT_EQ("a", parameters_proto.Get(0).name());
  EXPECT_EQ("b", parameters_proto.Get(0).value());
  EXPECT_EQ("c", parameters_proto.Get(1).name());
  EXPECT_EQ("d", parameters_proto.Get(1).value());

  EXPECT_EQ("b", merged->GetParameter("a").value_or(""));
  EXPECT_EQ("d", merged->GetParameter("c").value_or(""));
  EXPECT_FALSE(merged->GetParameter("not_found"));

  EXPECT_EQ("exp1,exp2", merged->experiment_ids());
}

}  // namespace
}  // namespace autofill_assistant
