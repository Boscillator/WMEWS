provider "aws" {
  region = var.aws_region
}

locals {
  api_directory     = abspath("${path.module}/../api")
  build_directory   = "${path.module}/.build"
  package_directory = "${local.build_directory}/package"
  package_zip       = "${local.build_directory}/wmews-ingest-api.zip"
  api_build_inputs = {
    main       = filesha256("${local.api_directory}/main.py")
    pyproject  = filesha256("${local.api_directory}/pyproject.toml")
    lockfile   = filesha256("${local.api_directory}/uv.lock")
  }
}

resource "terraform_data" "lambda_package" {
  triggers_replace = local.api_build_inputs

  provisioner "local-exec" {
    command     = "${path.module}/package_lambda.sh"
    interpreter = ["/usr/bin/env", "bash"]
    working_dir = path.module
  }
}

data "archive_file" "lambda" {
  type        = "zip"
  source_dir  = local.package_directory
  output_path = local.package_zip

  depends_on = [terraform_data.lambda_package]
}
