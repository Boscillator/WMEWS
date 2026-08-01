provider "aws" {
  region = var.aws_region

  default_tags {
    tags = local.common_tags
  }
}

data "aws_caller_identity" "current" {}

data "aws_region" "current" {}

data "aws_vpc" "default" {
  count   = var.vpc_id == null ? 1 : 0
  default = true
}

data "aws_subnets" "default" {
  count = var.vpc_id == null ? 1 : 0

  filter {
    name   = "vpc-id"
    values = [data.aws_vpc.default[0].id]
  }

  filter {
    name   = "state"
    values = ["available"]
  }
}

locals {
  common_tags = {
    Project   = "WMEWS"
    Component = "sagemaker-studio"
    ManagedBy = "OpenTofu"
  }

  selected_vpc_id = var.vpc_id != null ? var.vpc_id : data.aws_vpc.default[0].id
  selected_subnet_ids = length(var.subnet_ids) > 0 ? sort(var.subnet_ids) : sort(
    data.aws_subnets.default[0].ids
  )

  artifact_bucket_name = "wmews-sagemaker-artifacts-${data.aws_caller_identity.current.account_id}-${data.aws_region.current.region}"
  training_job_arn     = "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:training-job/wmews-*"
  domain_arn           = "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:domain/${aws_sagemaker_domain.workspace.id}"
  user_profile_arn     = "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:user-profile/${aws_sagemaker_domain.workspace.id}/${aws_sagemaker_user_profile.user.user_profile_name}"
  space_arn            = "arn:aws:sagemaker:${data.aws_region.current.region}:${data.aws_caller_identity.current.account_id}:space/${aws_sagemaker_domain.workspace.id}/${aws_sagemaker_space.jupyter.space_name}"
}
