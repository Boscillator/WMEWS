resource "aws_sagemaker_domain" "workspace" {
  domain_name             = var.domain_name
  auth_mode               = "IAM"
  app_network_access_type = "PublicInternetOnly"
  vpc_id                  = local.selected_vpc_id
  subnet_ids              = local.selected_subnet_ids
  tag_propagation         = "ENABLED"

  default_user_settings {
    execution_role      = aws_iam_role.execution.arn
    security_groups     = [aws_security_group.studio.id]
    studio_web_portal   = "ENABLED"
    default_landing_uri = "studio::"

    jupyter_lab_app_settings {
      default_resource_spec {
        instance_type = "ml.t3.medium"
      }

      app_lifecycle_management {
        idle_settings {
          lifecycle_management        = "ENABLED"
          idle_timeout_in_minutes     = 60
          min_idle_timeout_in_minutes = 60
          max_idle_timeout_in_minutes = 60
        }
      }
    }

    space_storage_settings {
      default_ebs_storage_settings {
        default_ebs_volume_size_in_gb = 5
        maximum_ebs_volume_size_in_gb = 5
      }
    }
  }

  default_space_settings {
    execution_role  = aws_iam_role.execution.arn
    security_groups = [aws_security_group.studio.id]

    jupyter_lab_app_settings {
      default_resource_spec {
        instance_type = "ml.t3.medium"
      }

      app_lifecycle_management {
        idle_settings {
          lifecycle_management        = "ENABLED"
          idle_timeout_in_minutes     = 60
          min_idle_timeout_in_minutes = 60
          max_idle_timeout_in_minutes = 60
        }
      }
    }

    space_storage_settings {
      default_ebs_storage_settings {
        default_ebs_volume_size_in_gb = 5
        maximum_ebs_volume_size_in_gb = 5
      }
    }
  }

  retention_policy {
    home_efs_file_system = "Delete"
  }
}

resource "aws_sagemaker_user_profile" "user" {
  domain_id         = aws_sagemaker_domain.workspace.id
  user_profile_name = var.user_profile_name

  user_settings {
    execution_role      = aws_iam_role.execution.arn
    security_groups     = [aws_security_group.studio.id]
    studio_web_portal   = "ENABLED"
    default_landing_uri = "studio::"

    jupyter_lab_app_settings {
      default_resource_spec {
        instance_type = "ml.t3.medium"
      }

      app_lifecycle_management {
        idle_settings {
          lifecycle_management        = "ENABLED"
          idle_timeout_in_minutes     = 60
          min_idle_timeout_in_minutes = 60
          max_idle_timeout_in_minutes = 60
        }
      }
    }

    space_storage_settings {
      default_ebs_storage_settings {
        default_ebs_volume_size_in_gb = 5
        maximum_ebs_volume_size_in_gb = 5
      }
    }
  }
}

resource "aws_sagemaker_space" "jupyter" {
  domain_id  = aws_sagemaker_domain.workspace.id
  space_name = var.space_name

  ownership_settings {
    owner_user_profile_name = aws_sagemaker_user_profile.user.user_profile_name
  }

  space_sharing_settings {
    sharing_type = "Private"
  }

  space_settings {
    app_type = "JupyterLab"

    jupyter_lab_app_settings {
      default_resource_spec {
        instance_type = "ml.t3.medium"
      }

      app_lifecycle_management {
        idle_settings {
          idle_timeout_in_minutes = 60
        }
      }
    }

    space_storage_settings {
      ebs_storage_settings {
        ebs_volume_size_in_gb = 5
      }
    }
  }
}
