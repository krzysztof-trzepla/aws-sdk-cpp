/*
* Copyright 2010-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
#pragma once
#include <aws/cloudformation/CloudFormation_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>

namespace Aws
{
namespace CloudFormation
{
namespace Model
{
  enum class ChangeSetStatus
  {
    NOT_SET,
    CREATE_PENDING,
    CREATE_IN_PROGRESS,
    CREATE_COMPLETE,
    DELETE_COMPLETE,
    FAILED
  };

namespace ChangeSetStatusMapper
{
AWS_CLOUDFORMATION_API ChangeSetStatus GetChangeSetStatusForName(const Aws::String& name);

AWS_CLOUDFORMATION_API Aws::String GetNameForChangeSetStatus(ChangeSetStatus value);
} // namespace ChangeSetStatusMapper
} // namespace Model
} // namespace CloudFormation
} // namespace Aws
