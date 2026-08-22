variable "aws_region" {
  description = "AWS region in which to create the SageMaker Studio resources."
  type        = string
  default     = "us-east-1"
}

variable "domain_name" {
  description = "Name of the SageMaker Studio domain."
  type        = string
  default     = "wmews-studio"
}

variable "user_profile_name" {
  description = "Name of the single SageMaker Studio user profile."
  type        = string
  default     = "wmews-user"
}

variable "space_name" {
  description = "Name of the private JupyterLab space."
  type        = string
  default     = "wmews-jupyter"
}

variable "mlflow_app_name" {
  description = "Name of the SageMaker serverless MLflow app."
  type        = string
  default     = "wmews-mlflow"

  validation {
    condition     = can(regex("^[A-Za-z0-9]([A-Za-z0-9-]{0,254}[A-Za-z0-9])?$", var.mlflow_app_name))
    error_message = "mlflow_app_name must be 1-256 alphanumeric or hyphen characters and start and end with an alphanumeric character."
  }
}

variable "github_repository" {
  description = "GitHub owner/repository allowed to publish wmews-cli images through OIDC."
  type        = string
  default     = "Boscillator/WMEWS"

  validation {
    condition     = can(regex("^[^/[:space:]]+/[^/[:space:]]+$", var.github_repository))
    error_message = "github_repository must be an owner/repository value."
  }
}

variable "raw_data_bucket_name" {
  description = "Existing data-collection bucket to expose read-only to Studio."
  type        = string

  validation {
    condition     = length(trimspace(var.raw_data_bucket_name)) > 0
    error_message = "raw_data_bucket_name must not be blank."
  }
}

variable "budget_alert_email" {
  description = "Email address that receives required SageMaker budget alerts."
  type        = string

  validation {
    condition     = can(regex("^[^@[:space:]]+@[^@[:space:]]+\\.[^@[:space:]]+$", var.budget_alert_email))
    error_message = "budget_alert_email must be a valid email address."
  }
}

variable "monthly_budget_usd" {
  description = "Monthly Amazon SageMaker cost-alert threshold in USD."
  type        = number
  default     = 10

  validation {
    condition     = var.monthly_budget_usd > 0
    error_message = "monthly_budget_usd must be greater than zero."
  }
}

variable "studio_access_principal_arn" {
  description = "IAM user or role ARN that may assume the restricted Studio access role."
  type        = string

  validation {
    condition     = can(regex("^arn:[^:]+:iam::[0-9]{12}:(user|role)/.+$", var.studio_access_principal_arn))
    error_message = "studio_access_principal_arn must be an IAM user or role ARN."
  }
}

variable "vpc_id" {
  description = "Existing VPC for the domain. Leave null to use the account default VPC."
  type        = string
  default     = null
  nullable    = true
}

variable "subnet_ids" {
  description = "Available subnets in vpc_id. Leave empty to use all default-VPC subnets."
  type        = list(string)
  default     = []

  validation {
    condition     = (var.vpc_id == null && length(var.subnet_ids) == 0) || (var.vpc_id != null && length(var.subnet_ids) > 0)
    error_message = "Set both vpc_id and a non-empty subnet_ids list, or leave both unset to use the default VPC."
  }
}
