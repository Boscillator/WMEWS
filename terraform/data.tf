resource "random_id" "raw_data_bucket_suffix" {
  byte_length = 4
}

locals {
  raw_data_bucket_name = "${var.raw_data_bucket_prefix}-${random_id.raw_data_bucket_suffix.hex}"
}

resource "aws_s3_bucket" "raw_data" {
  bucket        = local.raw_data_bucket_name
  force_destroy = false
}

resource "aws_s3_bucket_ownership_controls" "raw_data" {
  bucket = aws_s3_bucket.raw_data.id

  rule {
    object_ownership = "BucketOwnerEnforced"
  }
}

resource "aws_s3_bucket_public_access_block" "raw_data" {
  bucket = aws_s3_bucket.raw_data.id

  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}
