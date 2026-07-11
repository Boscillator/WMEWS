output "raw_data_bucket_name" {
  description = "Name of the private bucket that receives raw device data."
  value       = aws_s3_bucket.raw_data.bucket
}

output "lambda_function_name" {
  description = "Name of the deployed ingest Lambda function."
  value       = aws_lambda_function.ingest_api.function_name
}

output "lambda_function_url" {
  description = "Public Function URL for the ingest API."
  value       = aws_lambda_function_url.ingest_api.function_url
}
