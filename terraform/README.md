# WMEWS OpenTofu Roots

Each immediate child directory is an independent OpenTofu root with its own
state and provider lock file. Do not run OpenTofu commands from this directory.

- `data-collection/` deploys the device ingest Lambda and raw-data bucket.
- `sagemaker-studio/` deploys the cost-controlled SageMaker Studio workspace.

Use `tofu -chdir=terraform/<root> ...` or change into the intended root before
running a command. Deploy data collection first, then pass its bucket name to
the Studio root:

```bash
tofu -chdir=terraform/data-collection output -raw raw_data_bucket_name
```

The Studio root receives only that bucket name as an input; it does not read
the data-collection state. Either root can be destroyed without changing the
other root's state.
