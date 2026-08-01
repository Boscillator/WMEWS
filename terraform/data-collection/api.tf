data "aws_iam_policy_document" "lambda_s3_write" {
  statement {
    effect    = "Allow"
    actions   = ["s3:PutObject"]
    resources = ["${aws_s3_bucket.raw_data.arn}/*"]
  }
}

data "aws_iam_policy_document" "lambda_assume_role" {
  statement {
    effect  = "Allow"
    actions = ["sts:AssumeRole"]

    principals {
      type        = "Service"
      identifiers = ["lambda.amazonaws.com"]
    }
  }
}

resource "aws_iam_role" "lambda" {
  name               = "${var.lambda_function_name}-role"
  assume_role_policy = data.aws_iam_policy_document.lambda_assume_role.json
}

resource "aws_iam_role_policy_attachment" "lambda_basic_execution" {
  role       = aws_iam_role.lambda.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"
}

resource "aws_iam_role_policy" "lambda_s3_write" {
  name   = "${var.lambda_function_name}-s3-write"
  role   = aws_iam_role.lambda.id
  policy = data.aws_iam_policy_document.lambda_s3_write.json
}

resource "aws_lambda_function" "ingest_api" {
  function_name                  = var.lambda_function_name
  description                    = "WMEWS FastAPI ingest endpoint"
  role                           = aws_iam_role.lambda.arn
  runtime                        = "python3.11"
  architectures                  = ["x86_64"]
  handler                        = "main.handler"
  filename                       = data.archive_file.lambda.output_path
  source_code_hash               = data.archive_file.lambda.output_base64sha256
  memory_size                    = var.lambda_memory_size
  timeout                        = var.lambda_timeout
  reserved_concurrent_executions = var.lambda_reserved_concurrent_executions

  environment {
    variables = {
      RAW_DATA_BUCKET  = aws_s3_bucket.raw_data.bucket
      WMEWS_API_SECRET = var.wmews_api_secret
    }
  }
}

resource "aws_lambda_function_url" "ingest_api" {
  function_name      = aws_lambda_function.ingest_api.function_name
  authorization_type = "NONE"
}

resource "aws_lambda_permission" "function_url" {
  statement_id           = "AllowPublicFunctionUrl"
  action                 = "lambda:InvokeFunctionUrl"
  function_name          = aws_lambda_function.ingest_api.function_name
  principal              = "*"
  function_url_auth_type = "NONE"
}

resource "aws_lambda_permission" "function_url_invoke" {
  statement_id             = "AllowPublicFunctionUrlInvoke"
  action                   = "lambda:InvokeFunction"
  function_name            = aws_lambda_function.ingest_api.function_name
  principal                = "*"
  invoked_via_function_url = true
}
