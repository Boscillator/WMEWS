# WMEWS OpenTofu Deployment

This directory deploys the FastAPI/Mangum ingest API as an AWS Lambda Function
URL and creates a private S3 bucket for future device uploads.

## Prerequisites

Install the following before deploying:

- [OpenTofu](https://opentofu.org/docs/intro/install/) (the `tofu` command)
- AWS CLI v2
- [uv](https://docs.astral.sh/uv/)
- Python 3.11 (uv can download the required build interpreter when necessary)

Configure an AWS CLI profile or SSO session, then confirm the active identity
before any cloud operations:

```bash
aws configure sso --profile wmews
aws sso login --profile wmews
aws sts get-caller-identity --profile wmews
```

Either export `AWS_PROFILE=wmews` or pass `-var='aws_region=us-east-1'` and use
your usual AWS credential environment variables. Choose the desired region by
setting `aws_region` in a `terraform.tfvars` file or as a `-var` argument.

## Deploy

From this directory, run the local configuration checks:

```bash
tofu fmt -check
tofu init
tofu validate
```

`tofu fmt -check`, `tofu init`, and `tofu validate` do not need AWS credentials.
`tofu plan` and `tofu apply` do need AWS credentials and may query AWS:

```bash
tofu plan
tofu apply
```

Review the plan before confirming. The bucket name uses the stable state-held
suffix and will match `wmews-raw-data-` followed by eight lowercase hex digits.
Discover the generated values with:

```bash
tofu output
tofu output -raw raw_data_bucket_name
tofu output -raw lambda_function_url
```

Verify the current API contract after deployment:

```bash
curl "$(tofu output -raw lambda_function_url)upload-url"
```

The Function URL is intentionally public in this scaffold. Use IAM
authorization or API Gateway before exposing device traffic in production.

## Update And Teardown

Changing `../api/main.py`, `../api/pyproject.toml`, or `../api/uv.lock` rebuilds
the Lambda package on the next apply:

```bash
tofu apply
```

For a team or production deployment, configure encrypted remote state with
locking before sharing this configuration. The local state contains the stable
bucket suffix.

The bucket is protected from force deletion. Empty it before tearing down:

```bash
aws s3 rm "s3://$(tofu output -raw raw_data_bucket_name)" --recursive
tofu destroy
```
