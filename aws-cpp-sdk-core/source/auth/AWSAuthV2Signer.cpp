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
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/DateTime.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/crypto/Sha1HMAC.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/memory/AWSMemory.h>

#include <cstdio>
#include <iomanip>
#include <math.h>
#include <string.h>

using namespace Aws;
using namespace Aws::Client;
using namespace Aws::Auth;
using namespace Aws::Http;
using namespace Aws::Utils;
using namespace Aws::Utils::Logging;

static const char *NEWLINE = "\n";
static const char *LONG_DATE_FORMAT_STR = "%a, %d %b %Y %H:%M:%S GMT";

static const char *v2LogTag = "AWSAuthV2Signer";

static Http::HeaderValueCollection
CanonicalizeHeaders(Http::HeaderValueCollection &&headers) {
  Http::HeaderValueCollection canonicalHeaders;
  for (const auto &header : headers) {
    auto trimmedHeaderName = StringUtils::Trim(header.first.c_str());
    auto trimmedHeaderValue = StringUtils::Trim(header.second.c_str());
    canonicalHeaders[trimmedHeaderName] = trimmedHeaderValue;
  }

  return canonicalHeaders;
}

AWSAuthV2Signer::AWSAuthV2Signer(
    const std::shared_ptr<Auth::AWSCredentialsProvider> &credentialsProvider)
    : m_credentialsProvider(credentialsProvider),
      m_HMAC(Aws::MakeUnique<Aws::Utils::Crypto::Sha1HMAC>(v2LogTag)) {}

AWSAuthV2Signer::~AWSAuthV2Signer() {
  // empty destructor in .cpp file to keep from needing the implementation of
  // (AWSCredentialsProvider, Sha256, Sha256HMAC) in the header file
}

bool AWSAuthV2Signer::ShouldSignHeader(const Aws::String &header) const {
  return header.size() >= 6 && header.compare(0, 6, "x-amz-") == 0;
}

bool AWSAuthV2Signer::Sign(Aws::Http::HttpRequest &request,
                           long long expires) const {
  AWSCredentials credentials = m_credentialsProvider->GetAWSCredentials();

  // don't sign anonymous requests
  if (credentials.GetAWSAccessKeyId().empty() ||
      credentials.GetAWSSecretKey().empty()) {
    return true;
  }

  Aws::StringStream signingStringStream;
  signingStringStream << HttpMethodMapper::GetNameForHttpMethod(
                             request.GetMethod())
                      << NEWLINE;
  if (request.HasHeader(Aws::Http::CONTENT_MD5_HEADER)) {
    signingStringStream << request.GetHeaderValue(Aws::Http::CONTENT_MD5_HEADER)
                        << NEWLINE;
  } else {
    signingStringStream << NEWLINE;
  }
  if (request.HasHeader(Aws::Http::CONTENT_TYPE_HEADER)) {
    signingStringStream << request.GetHeaderValue(
                               Aws::Http::CONTENT_TYPE_HEADER)
                        << NEWLINE;
  } else {
    signingStringStream << NEWLINE;
  }
  if (expires >= 0) {
    signingStringStream << expires << NEWLINE;
  } else {
    signingStringStream << NEWLINE;
    if (!request.HasHeader(AWS_DATE_HEADER)) {
      DateTime now = GetSigningTimestamp();
      Aws::String dateHeaderValue = now.ToGmtString(LONG_DATE_FORMAT_STR);
      request.SetHeaderValue(AWS_DATE_HEADER, dateHeaderValue);
    }
  }

  for (const auto &header : CanonicalizeHeaders(request.GetHeaders())) {
    if (ShouldSignHeader(header.first)) {
      signingStringStream << header.first.c_str() << ":"
                          << header.second.c_str() << NEWLINE;
    }
  }

  URI uriCpy = request.GetUri();
  signingStringStream << uriCpy.GetURLEncodedPath();

  Aws::String signingString = signingStringStream.str();
  AWS_LOGSTREAM_DEBUG(v2LogTag, "Signing String: " << signingString);

  // now compute sha1 on that request string
  auto hashResult = m_HMAC->Calculate(
      ByteBuffer((unsigned char *)signingString.c_str(),
                 signingString.length()),
      ByteBuffer((unsigned char *)credentials.GetAWSSecretKey().c_str(),
                 credentials.GetAWSSecretKey().length()));
  if (!hashResult.IsSuccess()) {
    AWS_LOGSTREAM_ERROR(v2LogTag, "Failed to hash (sha1) request string \""
                                      << signingString << "\"");
    return false;
  }

  auto shaDigest = hashResult.GetResult();
  Aws::String signature = HashingUtils::Base64Encode(shaDigest);
  Aws::StringStream ss;
  ss << "AWS"
     << " " << credentials.GetAWSAccessKeyId() << ":" << signature;

  auto awsAuthString = ss.str();
  AWS_LOGSTREAM_DEBUG(v2LogTag, "Signing request with: " << awsAuthString);
  request.SetAuthorization(awsAuthString);

  return true;
}

bool AWSAuthV2Signer::SignRequest(Aws::Http::HttpRequest &request) const {
  return Sign(request, -1);
}

bool AWSAuthV2Signer::PresignRequest(Aws::Http::HttpRequest &request,
                                     long long expirationTimeInSeconds) const {
  return PresignRequest(request, "", expirationTimeInSeconds);
}

bool AWSAuthV2Signer::PresignRequest(Aws::Http::HttpRequest &request,
                                     const char *region,
                                     long long expirationInSeconds) const {
  return PresignRequest(request, region, "", expirationInSeconds);
}

bool AWSAuthV2Signer::PresignRequest(Aws::Http::HttpRequest &request,
                                     const char *, const char *,
                                     long long expirationTimeInSeconds) const {
  long long epoch =
      (long long)(GetSigningTimestamp().SecondsWithMSPrecision()) +
      expirationTimeInSeconds;
  if (!Sign(request, epoch)) {
    return false;
  }
  if (!request.HasHeader(Aws::Http::AUTHORIZATION_HEADER)) {
    return true;
  }

  Aws::StringStream expireStream;
  expireStream << epoch;

  // add that the signature to the query string
  Aws::String signature = request.GetAuthorization().substr(4);
  Aws::Vector<Aws::String> parts = StringUtils::Split(signature, ':');
  request.AddQueryStringParameter("Signature", parts[1]);
  request.AddQueryStringParameter("AWSAccessKeyId", parts[0]);
  request.AddQueryStringParameter("Expires", expireStream.str());
  AWS_LOGSTREAM_DEBUG(v2LogTag,
                      "Presigned request: " << request.GetURIString(true));

  return true;
}
