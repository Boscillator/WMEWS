resource "aws_security_group" "studio" {
  name        = "${var.domain_name}-studio"
  description = "NFS connectivity for the WMEWS SageMaker Studio domain."
  vpc_id      = local.selected_vpc_id
}

resource "aws_vpc_security_group_ingress_rule" "studio_nfs" {
  security_group_id            = aws_security_group.studio.id
  referenced_security_group_id = aws_security_group.studio.id
  ip_protocol                  = "tcp"
  from_port                    = 2049
  to_port                      = 2049
  description                  = "Allow NFS between Studio resources."
}

resource "aws_vpc_security_group_egress_rule" "studio_nfs" {
  security_group_id            = aws_security_group.studio.id
  referenced_security_group_id = aws_security_group.studio.id
  ip_protocol                  = "tcp"
  from_port                    = 2049
  to_port                      = 2049
  description                  = "Allow NFS to the Studio home EFS volume."
}

resource "aws_vpc_security_group_egress_rule" "studio_internet" {
  security_group_id = aws_security_group.studio.id
  ip_protocol       = "-1"
  cidr_ipv4         = "0.0.0.0/0"
  description       = "Allow Studio access to managed public networking."
}
