variable "aws_region" {
  description = "AWS region in which to create the WMEWS resources."
  type        = string
  default     = "us-east-1"
}

variable "lambda_function_name" {
  description = "Name of the ingest Lambda function."
  type        = string
  default     = "wmews-ingest-api"
}

variable "lambda_memory_size" {
  description = "Memory available to the Lambda function in MB."
  type        = number
  default     = 256

  validation {
    condition     = var.lambda_memory_size >= 128 && var.lambda_memory_size <= 10240
    error_message = "lambda_memory_size must be between 128 and 10240 MB."
  }
}

variable "lambda_timeout" {
  description = "Maximum Lambda invocation duration in seconds."
  type        = number
  default     = 30

  validation {
    condition     = var.lambda_timeout >= 1 && var.lambda_timeout <= 900
    error_message = "lambda_timeout must be between 1 and 900 seconds."
  }
}

variable "lambda_reserved_concurrent_executions" {
  description = "Maximum concurrent executions reserved for the ingest Lambda to limit billing exposure."
  type        = number
  default     = 2

  validation {
    condition     = var.lambda_reserved_concurrent_executions >= 1
    error_message = "lambda_reserved_concurrent_executions must be at least 1."
  }
}

variable "raw_data_bucket_prefix" {
  description = "Prefix for the private raw-data bucket; an eight-character hex suffix is added."
  type        = string
  default     = "wmews-raw-data"

  validation {
    condition     = can(regex("^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$", var.raw_data_bucket_prefix)) && length(var.raw_data_bucket_prefix) <= 54
    error_message = "raw_data_bucket_prefix must use lowercase letters, numbers, and hyphens, start and end with an alphanumeric character, and be at most 54 characters."
  }
}

variable "wmews_api_secret" {
  description = "Shared secret required by devices calling the ingest API."
  type        = string
  sensitive   = true

  validation {
    condition     = length(trimspace(var.wmews_api_secret)) > 0
    error_message = "wmews_api_secret must not be blank."
  }
}
