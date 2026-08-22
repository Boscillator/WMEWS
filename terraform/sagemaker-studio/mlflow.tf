resource "aws_sagemaker_mlflow_app" "workspace" {
  name                   = var.mlflow_app_name
  artifact_store_uri     = "s3://${aws_s3_bucket.artifacts.bucket}/mlflow"
  role_arn               = aws_iam_role.mlflow_app.arn
  default_domain_id_list = [aws_sagemaker_domain.workspace.id]
  account_default_status = "ENABLED"
}
