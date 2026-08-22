# WMEWS Cost-Controlled SageMaker Studio

This root creates a single-user SageMaker Studio domain, private JupyterLab
space, read-only raw-data access, a private artifact bucket, and a required
monthly SageMaker budget alert. It does not start a billable JupyterLab app.

## Prerequisites

Install OpenTofu and AWS CLI v2. Configure the AWS identity that will create
the resources, then identify the IAM user or role that will assume the
restricted Studio access role.

The data-collection root must already have been deployed. Obtain its bucket:

```bash
tofu -chdir=terraform/data-collection output -raw raw_data_bucket_name
```

Copy `terraform.tfvars.example` to ignored `terraform.tfvars` and set the raw
bucket name, budget alert email, and `studio_access_principal_arn`. Set
`github_repository` if the default repository name is not correct. The AWS
Budgets email subscription must be confirmed after deployment.

The root uses the account's default VPC and all available default-VPC subnets.
If the account has no default VPC, provide both `vpc_id` and `subnet_ids`.

## Validate and deploy

From this directory, run:

```bash
tofu fmt -check
tofu init
tofu validate
```

Run `tofu plan` and `tofu apply` yourself only after reviewing the result.
This configuration deliberately uses `PublicInternetOnly` networking to avoid
NAT gateway and interface-endpoint charges.

## GitHub Actions container publishing

The root creates an OIDC role restricted to the configured repository's `main`
branch. After applying the reviewed configuration, set the
`WMEWS_CLI_ECR_PUBLISHER_ROLE_ARN` GitHub Actions repository variable to:

```bash
tofu output -raw github_actions_ecr_publisher_role_arn
```

The CLI workflow exchanges its GitHub OIDC token for this role and then obtains
a short-lived ECR authorization token; do not create or store long-lived AWS
access keys in GitHub. Published images receive an immutable commit-SHA tag and
a mutable `latest` tag. The repository URI is available from
`tofu output -raw wmews_cli_ecr_repository_url`.

## Open JupyterLab

Assume the `studio_access_role_arn` output using your authorized IAM identity.
Then create a presigned URL using the `studio_url_command` output:

```bash
tofu output -raw studio_url_command
```

Open the returned URL, select the `wmews-jupyter` private space, and run it.
The space defaults to `ml.t3.medium` with a fixed 5 GB EBS volume. No app is
created by OpenTofu, so compute billing begins only when you run the space.

JupyterLab stops after 60 idle minutes only when no kernel and no terminal are
active. Stop the app manually when you finish work; do not rely on the timer.

## Training cost limits

The execution role can read the raw-data bucket and write only to the artifact
bucket. It can create training jobs only on `ml.m5.large` with a maximum
runtime of one hour. It cannot create endpoints or other hosted resources.

Use one instance and Managed Spot Training for every training job. A SageMaker
SDK estimator should set `instance_count=1`, `instance_type="ml.m5.large"`,
`use_spot_instances=True`, `max_run=3600`, a bounded `max_wait`, and checkpoint
output beneath `s3://<artifact-bucket>/checkpoints/`.

The IAM policy enforces instance type and runtime. AWS does not provide a
condition key that proves a job enabled Spot or limited instance count, so the
budget and operating convention remain important controls.

## Teardown

Stop and delete any running JupyterLab app before teardown. The repository
intentionally refuses to be destroyed while it contains images. Delete it and
all of its images first:

```bash
aws ecr delete-repository --repository-name wmews-cli --force
```

Then, from this directory, run `tofu destroy` yourself after review. Destroying
this root deletes the JupyterLab EBS data, domain home EFS data, artifact-bucket
objects, ECR repository, and GitHub Actions OIDC role. It does not change the
data-collection Lambda or raw-data bucket.
