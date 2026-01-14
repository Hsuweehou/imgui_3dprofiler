/*******************************************************************************
 *  FILENAME:      test_context.h
 *
 *  AUTHORS:       Jia Liang    START DATE: Wednesday July 10th 2024
 *
 *   LAST MODIFIED: Saturday, April 12th 2025, 3:32:43 pm
 *
 *  Copyright (c) 2023 - 2024 DepthVision Limited
 *
 *  CONTACT:       liangjia@depthvision3d.com
 *******************************************************************************/

#ifndef ABFA5BA2_A777_4FD8_896B_1EE0F1086BEB
#define ABFA5BA2_A777_4FD8_896B_1EE0F1086BEB
#include <string>
#include <map>
#include <gtest/gtest.h>
#include <algorithm>

namespace test {

const std::string kTestDataDir = "test_data_dir";
const std::string kWorkSpaceDir = "workspace_dir";
// const std::string kTestALlExeName = "test_all.exe";

class TestEnv {
   public:
    static TestEnv& GetInstance() {
        static TestEnv instance;
        return instance;
    }
};

}  // namespace test

#endif /* ABFA5BA2_A777_4FD8_896B_1EE0F1086BEB */
