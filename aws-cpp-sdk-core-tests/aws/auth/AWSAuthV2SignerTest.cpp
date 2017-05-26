/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <aws/core/auth/AWSAuthSigner.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/http/standard/StandardHttpRequest.h>
#include <aws/core/http/standard/StandardHttpResponse.h>
#include <aws/core/platform/FileSystem.h>
#include <aws/core/platform/Platform.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/external/gtest.h>
#include <fstream>

using namespace Aws::Client;
using namespace Aws::Utils;
using namespace Aws::Http;

static const char *ALLOC_TAG = "AwsAuthV2SignerTest";

static Standard::StandardHttpRequest
ParseHttpRequest(Aws::IOStream &inputStream) {
  Aws::String methodLine;
  std::getline(inputStream, methodLine);
  auto firstSpaceIter = methodLine.find_first_of(' ');
  auto secondSpaceIter = methodLine.find(' ', firstSpaceIter + 1);
  Aws::String methodStr = methodLine.substr(0, firstSpaceIter);
  Aws::String pathStr = methodLine.substr(firstSpaceIter + 1,
                                          secondSpaceIter - firstSpaceIter - 1);
  HeaderValueCollection headers;

  Aws::String pairLine;
  Aws::String currentHeader;

  while (std::getline(inputStream, pairLine)) {
    if (!pairLine.empty()) {
      auto pair = StringUtils::Split(pairLine, ':');
      if (pair.size() == 2) {
        currentHeader = pair[0].c_str();
        headers[currentHeader] = pair[1].c_str();
      }
    }
  }

  auto pathAndQuery = StringUtils::Split(pathStr, '?');
  auto pathOnly = pathAndQuery[0];
  URI uri;
  uri.SetScheme(Scheme::HTTP);
  uri.SetAuthority("example.amazonaws.com");
  uri.SetPath(pathOnly);
  if (pathAndQuery.size() > 1) {
    uri.SetQueryString(pathAndQuery[1]);
  }

  HttpMethod method = HttpMethod::HTTP_GET;
  if (methodStr == "GET") {
    method = HttpMethod::HTTP_GET;
  } else if (methodStr == "POST") {
    method = HttpMethod::HTTP_POST;
  }

  Standard::StandardHttpRequest request(uri, method);
  for (auto &header : headers) {
    request.SetHeaderValue(header.first, header.second);
  }

  return request;
}

static Aws::String MakeSigV2ResourceFilePath(const Aws::String &testName,
                                             const Aws::String &fileSuffix) {
  Aws::StringStream fileName;
#ifdef __ANDROID__
  fileName << Aws::Platform::GetCacheDirectory() << "resources"
           << Aws::FileSystem::PATH_DELIM;
#else
  fileName << "./aws2_testsuite/aws2_testsuite/";
#endif

  fileName << testName << Aws::FileSystem::PATH_DELIM << testName << "."
           << fileSuffix;

  return fileName.str();
}

static void RunV2TestCase(const char *testCaseName) {
  Aws::String requestFileName = MakeSigV2ResourceFilePath(testCaseName, "req");
  Aws::FStream requestFile(requestFileName.c_str(), std::ios::in);
  ASSERT_TRUE(requestFile.good());

  Aws::String expectedSignatureFileName =
      MakeSigV2ResourceFilePath(testCaseName, "authz");
  Aws::FStream signatureFile(expectedSignatureFileName.c_str(), std::ios::in);
  ASSERT_TRUE(signatureFile.good());

  std::shared_ptr<Aws::Auth::AWSCredentialsProvider> credProvider =
      Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>(
          ALLOC_TAG, "AKIDEXAMPLE", "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY");

  auto httpRequestToMake = ParseHttpRequest(requestFile);
  AWSAuthV2Signer signer(credProvider);

  bool successfullySigned = signer.SignRequest(httpRequestToMake);
  ASSERT_TRUE(successfullySigned);

  Aws::String expectedSignature;
  std::getline(signatureFile, expectedSignature);

  ASSERT_STREQ(expectedSignature.c_str(),
               httpRequestToMake.GetAwsAuthorization().c_str());
}

TEST(AWSAuthV2SignerTest, GetVanilla) { RunV2TestCase("get-vanilla"); }

TEST(AWSAuthV2SignerTest, GetCustomXAMZ) { RunV2TestCase("get-custom-x-amz"); }

TEST(AWSAuthV2SignerTest, GetNonXAMZ) { RunV2TestCase("get-non-x-amz"); }

TEST(AWSAuthV2SignerTest, GetContentType) { RunV2TestCase("get-content-type"); }

TEST(AWSAuthV2SignerTest, GetContentMD5) { RunV2TestCase("get-content-md5"); }

TEST(AWSAuthV2SignerTest, GetContentBoth) { RunV2TestCase("get-content-both"); }
