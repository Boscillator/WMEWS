data "aws_iam_policy_document" "sagemaker_assume_role" {
  statement {
    effect  = "Allow"
    actions = ["sts:AssumeRole"]

    principals {
      type        = "Service"
      identifiers = ["sagemaker.amazonaws.com"]
    }
  }
}

resource "aws_iam_role" "execution" {
  name               = "${var.domain_name}-execution"
  assume_role_policy = data.aws_iam_policy_document.sagemaker_assume_role.json
}

data "aws_iam_policy_document" "execution" {
  statement {
    sid       = "ReadRawCollectionData"
    effect    = "Allow"
    actions   = ["s3:GetBucketLocation", "s3:ListBucket"]
    resources = ["arn:aws:s3:::${var.raw_data_bucket_name}"]
  }

  statement {
    sid       = "ReadRawCollectionObjects"
    effect    = "Allow"
    actions   = ["s3:GetObject"]
    resources = ["arn:aws:s3:::${var.raw_data_bucket_name}/*"]
  }

  statement {
    sid       = "ManageTrainingArtifacts"
    effect    = "Allow"
    actions   = ["s3:GetBucketLocation", "s3:ListBucket"]
    resources = [aws_s3_bucket.artifacts.arn]
  }

  statement {
    sid    = "ManageTrainingArtifactObjects"
    effect = "Allow"
    actions = [
      "s3:AbortMultipartUpload",
      "s3:DeleteObject",
      "s3:GetObject",
      "s3:ListMultipartUploadParts",
      "s3:PutObject",
    ]
    resources = ["${aws_s3_bucket.artifacts.arn}/*"]
  }

  statement {
    sid    = "PullTrainingImages"
    effect = "Allow"
    actions = [
      "ecr:BatchCheckLayerAvailability",
      "ecr:BatchGetImage",
      "ecr:GetAuthorizationToken",
      "ecr:GetDownloadUrlForLayer",
    ]
    resources = ["*"]
  }

  statement {
    sid    = "WriteTrainingLogs"
    effect = "Allow"
    actions = [
      "logs:CreateLogGroup",
      "logs:CreateLogStream",
      "logs:PutLogEvents",
    ]
    resources = ["*"]
  }

  statement {
    sid       = "CreateBoundedTrainingJobs"
    effect    = "Allow"
    actions   = ["sagemaker:CreateTrainingJob"]
    resources = [local.training_job_arn]

    condition {
      test     = "StringEquals"
      variable = "sagemaker:InstanceTypes"
      values   = ["ml.m5.large"]
    }

    condition {
      test     = "NumericLessThanEquals"
      variable = "sagemaker:MaxRuntimeInSeconds"
      values   = ["3600"]
    }
  }

  statement {
    sid    = "ObserveAndStopTrainingJobs"
    effect = "Allow"
    actions = [
      "sagemaker:DescribeTrainingJob",
      "sagemaker:StopTrainingJob",
    ]
    resources = [local.training_job_arn]
  }

  statement {
    sid       = "ListTrainingJobs"
    effect    = "Allow"
    actions   = ["sagemaker:ListTrainingJobs"]
    resources = ["*"]
  }

  # Studio assumes this role after the presigned URL has been accepted.
  statement {
    sid       = "CreatePresignedUrlForManagedProfile"
    effect    = "Allow"
    actions   = ["sagemaker:CreatePresignedDomainUrl"]
    resources = [local.user_profile_arn]
  }

  statement {
    sid    = "ReadManagedStudioWorkspace"
    effect = "Allow"
    actions = [
      "sagemaker:DescribeApp",
      "sagemaker:DescribeDomain",
      "sagemaker:DescribeSpace",
      "sagemaker:DescribeUserProfile",
    ]
    resources = [
      local.domain_arn,
      local.user_profile_arn,
      local.user_profile_app_arn,
      local.space_arn,
      "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:app/${aws_sagemaker_domain.workspace.id}/${aws_sagemaker_space.jupyter.space_name}/*",
    ]
  }

  statement {
    sid       = "UpdateManagedJupyterLabSpace"
    effect    = "Allow"
    actions   = ["sagemaker:UpdateSpace"]
    resources = [local.space_arn]
  }

  statement {
    sid    = "ListStudioWorkspaceResources"
    effect = "Allow"
    actions = [
      "sagemaker:ListApps",
      "sagemaker:ListDomains",
      "sagemaker:ListMlflowApps",
      "sagemaker:ListImageVersions",
      "sagemaker:ListImages",
      "sagemaker:ListSpaces",
      "sagemaker:ListUserProfiles",
    ]
    resources = ["*"]
  }

  statement {
    sid     = "RunOnlyTheManagedJupyterLabSpace"
    effect  = "Allow"
    actions = ["sagemaker:CreateApp", "sagemaker:DeleteApp"]
    resources = [
      "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:app/${aws_sagemaker_domain.workspace.id}/${aws_sagemaker_space.jupyter.space_name}/*",
    ]
  }

  statement {
    sid       = "TagManagedStudioApps"
    effect    = "Allow"
    actions   = ["sagemaker:AddTags"]
    resources = ["*"]
  }

  statement {
    sid    = "AccessServerlessMlflowApp"
    effect = "Allow"
    actions = [
      "sagemaker:CreatePresignedMlflowAppUrl",
      "sagemaker:DescribeMlflowApp",
    ]
    resources = [aws_sagemaker_mlflow_app.workspace.arn]
  }

  statement {
    sid       = "UseServerlessMlflowApi"
    effect    = "Allow"
    actions   = ["sagemaker-mlflow:*"]
    resources = [aws_sagemaker_mlflow_app.workspace.arn]
  }

  statement {
    sid       = "PassOnlyThisRoleToSageMaker"
    effect    = "Allow"
    actions   = ["iam:PassRole"]
    resources = [aws_iam_role.execution.arn]

    condition {
      test     = "StringEquals"
      variable = "iam:PassedToService"
      values   = ["sagemaker.amazonaws.com"]
    }
  }
}

resource "aws_iam_role_policy" "execution" {
  name   = "${var.domain_name}-execution"
  role   = aws_iam_role.execution.id
  policy = data.aws_iam_policy_document.execution.json
}

data "aws_iam_policy_document" "mlflow_app" {
  statement {
    sid       = "ListMlflowArtifacts"
    effect    = "Allow"
    actions   = ["s3:ListBucket"]
    resources = [aws_s3_bucket.artifacts.arn]

    condition {
      test     = "StringLike"
      variable = "s3:prefix"
      values   = ["mlflow/*"]
    }
  }

  statement {
    sid       = "LocateArtifactBucket"
    effect    = "Allow"
    actions   = ["s3:GetBucketLocation"]
    resources = [aws_s3_bucket.artifacts.arn]
  }

  statement {
    sid    = "ManageMlflowArtifacts"
    effect = "Allow"
    actions = [
      "s3:AbortMultipartUpload",
      "s3:DeleteObject",
      "s3:GetObject",
      "s3:ListMultipartUploadParts",
      "s3:PutObject",
    ]
    resources = ["${aws_s3_bucket.artifacts.arn}/mlflow/*"]
  }
}

resource "aws_iam_role" "mlflow_app" {
  name               = "${var.domain_name}-mlflow"
  assume_role_policy = data.aws_iam_policy_document.sagemaker_assume_role.json
}

resource "aws_iam_role_policy" "mlflow_app" {
  name   = "${var.domain_name}-mlflow"
  role   = aws_iam_role.mlflow_app.id
  policy = data.aws_iam_policy_document.mlflow_app.json
}

data "aws_iam_policy_document" "studio_access_assume_role" {
  statement {
    effect  = "Allow"
    actions = ["sts:AssumeRole"]

    principals {
      type        = "AWS"
      identifiers = [var.studio_access_principal_arn]
    }
  }
}

resource "aws_iam_role" "studio_access" {
  name                 = "${var.domain_name}-access"
  max_session_duration = 14400
  assume_role_policy   = data.aws_iam_policy_document.studio_access_assume_role.json
}

data "aws_iam_policy_document" "studio_access" {
  statement {
    sid       = "OpenOnlyTheManagedStudioProfile"
    effect    = "Allow"
    actions   = ["sagemaker:CreatePresignedDomainUrl"]
    resources = [local.user_profile_arn]
  }

  statement {
    sid    = "ReadStudioWorkspace"
    effect = "Allow"
    actions = [
      "sagemaker:DescribeApp",
      "sagemaker:DescribeDomain",
      "sagemaker:DescribeSpace",
      "sagemaker:DescribeUserProfile",
      "sagemaker:ListApps",
      "sagemaker:ListDomains",
      "sagemaker:ListImageVersions",
      "sagemaker:ListImages",
      "sagemaker:ListSpaces",
      "sagemaker:ListUserProfiles",
    ]
    resources = ["*"]
  }

  statement {
    sid     = "RunOnlyTheManagedJupyterLabSpace"
    effect  = "Allow"
    actions = ["sagemaker:CreateApp", "sagemaker:DeleteApp"]
    resources = [
      "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:app/${aws_sagemaker_domain.workspace.id}/${aws_sagemaker_space.jupyter.space_name}/*",
    ]

    condition {
      test     = "StringEquals"
      variable = "sagemaker:InstanceTypes"
      values   = ["ml.t3.medium"]
    }
  }

  statement {
    sid       = "TagManagedStudioApps"
    effect    = "Allow"
    actions   = ["sagemaker:AddTags"]
    resources = ["*"]
  }
}

resource "aws_iam_role_policy" "studio_access" {
  name   = "${var.domain_name}-access"
  role   = aws_iam_role.studio_access.id
  policy = data.aws_iam_policy_document.studio_access.json
}
