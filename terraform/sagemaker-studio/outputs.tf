output "sagemaker_domain_id" {
  description = "ID of the SageMaker Studio domain."
  value       = aws_sagemaker_domain.workspace.id
}

output "sagemaker_domain_arn" {
  description = "ARN of the SageMaker Studio domain."
  value       = aws_sagemaker_domain.workspace.arn
}

output "sagemaker_user_profile_name" {
  description = "Name of the SageMaker Studio user profile."
  value       = aws_sagemaker_user_profile.user.user_profile_name
}

output "sagemaker_space_name" {
  description = "Name of the private JupyterLab space."
  value       = aws_sagemaker_space.jupyter.space_name
}

output "studio_access_role_arn" {
  description = "Role to assume before generating a restricted Studio URL."
  value       = aws_iam_role.studio_access.arn
}

output "sagemaker_execution_role_arn" {
  description = "Scoped execution role used by the Studio profile and space."
  value       = aws_iam_role.execution.arn
}

output "artifact_bucket_name" {
  description = "Private bucket for training artifacts and checkpoints."
  value       = aws_s3_bucket.artifacts.bucket
}

output "mlflow_app_arn" {
  description = "ARN of the SageMaker serverless MLflow app."
  value       = aws_sagemaker_mlflow_app.workspace.arn
}

output "mlflow_tracking_uri" {
  description = "MLflow tracking URI for the SageMaker serverless MLflow app."
  value       = aws_sagemaker_mlflow_app.workspace.arn
}

output "raw_data_bucket_name" {
  description = "Read-only raw-data bucket supplied to this root."
  value       = var.raw_data_bucket_name
}

output "monthly_budget_usd" {
  description = "Monthly Amazon SageMaker cost-alert threshold."
  value       = var.monthly_budget_usd
}

output "studio_url_command" {
  description = "Run this while using the restricted Studio access role."
  value       = "aws sagemaker create-presigned-domain-url --region ${data.aws_region.current.region} --domain-id ${aws_sagemaker_domain.workspace.id} --user-profile-name ${aws_sagemaker_user_profile.user.user_profile_name} --landing-uri studio::"
}
