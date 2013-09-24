// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/err.h"
#include "tools/gn/settings.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/value.h"

/* TODO(brettw) make this compile again
namespace {

class TestTargetManagerDelegate : public TargetManager::Delegate {
 public:
  TestTargetManagerDelegate() {}

  virtual bool ScheduleInvocation(ToolchainManager* toolchain_manager,
                                  const LocationRange& origin,
                                  const Label& toolchain_name,
                                  const SourceDir& dir,
                                  Err* err) OVERRIDE {
    invokes.push_back(dir.value());
    return true;
  }
  virtual void ScheduleTargetFileWrite(const Target* target) {
    writes.push_back(target);
  }

  std::vector<std::string> invokes;
  std::vector<const Target*> writes;
};

}  // namespace

TEST(TargetManager, ResolveDeps) {
  TestTargetManagerDelegate ttmd;
  BuildSettings build_settings(&ttmd);

  TargetManager& target_manager = build_settings.target_manager();

  SourceDir tc_dir("/chrome/");
  std::string tc_name("toolchain");

  // Get a root target. This should not invoke anything.
  Err err;
  Label chromelabel(SourceDir("/chrome/"), "chrome", tc_dir, tc_name);
  Target* chrome = target_manager.GetTarget(
      chromelabel, LocationRange(), NULL, &err);
  EXPECT_EQ(0u, ttmd.invokes.size());

  // Declare it has a dependency on content1 and 2. We should get one
  // invocation of the content build file.
  Label content1label(SourceDir("/content/"), "content1", tc_dir, tc_name);
  Target* content1 = target_manager.GetTarget(
      content1label, LocationRange(), chrome, &err);
  EXPECT_EQ(1u, ttmd.invokes.size());

  Label content2label(SourceDir("/content/"), "content2", tc_dir, tc_name);
  Target* content2 = target_manager.GetTarget(
      content2label, LocationRange(), chrome, &err);
  EXPECT_EQ(2u, ttmd.invokes.size());

  // Declare chrome has a depdency on base, this should load it.
  Label baselabel(SourceDir("/base/"), "base", tc_dir, tc_name);
  Target* base1 = target_manager.GetTarget(
      baselabel, LocationRange(), chrome, &err);
  EXPECT_EQ(3u, ttmd.invokes.size());

  // Declare content1 has a dependency on base.
  Target* base2 = target_manager.GetTarget(
      baselabel, LocationRange(), content1, &err);
  EXPECT_EQ(3u, ttmd.invokes.size());
  EXPECT_EQ(base1, base2);

  // Mark content1 and chrome as done. They have unresolved depdendencies so
  // shouldn't be written out yet.
  target_manager.TargetGenerationComplete(content1label);
  target_manager.TargetGenerationComplete(chromelabel);
  EXPECT_EQ(0u, ttmd.writes.size());

  // Mark content2 as done. It has no dependencies so should be written.
  target_manager.TargetGenerationComplete(content2label);
  ASSERT_EQ(1u, ttmd.writes.size());
  EXPECT_EQ(content2label, ttmd.writes[0]->label());

  // Mark base as complete. It should have caused itself, content1 and then
  // chrome to be written.
  target_manager.TargetGenerationComplete(baselabel);
  ASSERT_EQ(4u, ttmd.writes.size());
  EXPECT_EQ(baselabel, ttmd.writes[1]->label());
  EXPECT_EQ(content1label, ttmd.writes[2]->label());
  EXPECT_EQ(chromelabel, ttmd.writes[3]->label());
}
*/
