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

Generate a long random device secret once and keep supplying that same value for
later OpenTofu commands. Do not commit it. An ignored `terraform.tfvars` file
is the most convenient local option: copy `terraform.tfvars.example` to
`terraform.tfvars`, then replace its placeholder. A persistent
`TF_VAR_wmews_api_secret` environment variable also works. The export below
lasts only for the current shell, so set it again when using a new shell rather
than generating a new one.

```bash
export TF_VAR_wmews_api_secret="$(openssl rand -hex 32)"
```

OpenTofu records this sensitive input in state so it can detect infrastructure
changes. `sensitive` hides it from normal CLI output but does not encrypt it;
protect the local state file, or use encrypted remote state with restricted
access before deploying for a team or production.

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

Verify the API and upload a small capture after deployment:

```bash
function_url="$(tofu output -raw lambda_function_url)"
bucket="$(tofu output -raw raw_data_bucket_name)"
response="$(curl -sS \
  -H "X-WMEWS-Secret: $TF_VAR_wmews_api_secret" \
  -H "X-WMEWS-Device-Id: 42" \
  "${function_url}upload-url")"
printf '%s\n' "$response"

upload_url="$(printf '%s' "$response" | jq -r '.upload_url')"
object_key="$(printf '%s' "$response" | jq -r '.object_key')"
printf 'test capture\n' > /tmp/wmews-smoke.washcap
curl -sS -X PUT -H 'Content-Type: application/octet-stream' \
  --upload-file /tmp/wmews-smoke.washcap "$upload_url"
aws s3api head-object --bucket "$bucket" --key "$object_key"
```

The Function URL is intentionally public and uses a shared secret only as an
interim control. Use stronger per-device authentication before production.

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
