# Syntax highlighting test for Terraform: providers, variables, locals,
# resources, data sources, modules, outputs, interpolation and expressions.
// Both comment styles are valid in HCL.
/* A block comment
   spanning lines. */

terraform {
  required_version = ">= 1.5.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.region
  default_tags {
    tags = merge(local.common_tags, { Environment = var.environment })
  }
}

variable "region" {
  type        = string
  default     = "eu-west-1"
  description = "AWS region to deploy into"
}

variable "instance_count" {
  type    = number
  default = 2
  validation {
    condition     = var.instance_count > 0 && var.instance_count <= 10
    error_message = "Use between 1 and 10 instances."
  }
}

locals {
  common_tags = {
    Project = "sourcerer"
    Owner   = "platform"
  }
  names = [for i in range(var.instance_count) : "web-${i}"]
  subnets = { for k, v in data.aws_subnets.default.ids : k => v if k != "" }
  enabled = true ? 1 : 0
  ratio   = 0.75 * 4 - 1 / 2
}

data "aws_ami" "ubuntu" {
  most_recent = true
  owners      = ["099720109477"]

  filter {
    name   = "name"
    values = ["ubuntu/images/hvm-ssd/ubuntu-jammy-22.04-amd64-server-*"]
  }
}

resource "aws_instance" "web" {
  count         = var.instance_count
  ami           = data.aws_ami.ubuntu.id
  instance_type = "t3.micro"
  subnet_id     = element(local.subnets, count.index)

  user_data = <<-EOT
    #!/bin/bash
    echo "hello from ${var.region}"
  EOT

  tags = merge(local.common_tags, {
    Name = local.names[count.index]
  })

  lifecycle {
    create_before_destroy = true
  }
}

module "network" {
  source  = "./modules/network"
  cidr    = "10.0.0.0/16"
  zones   = ["a", "b"]
  enabled = !var.disabled && var.instance_count >= 1
}

output "instance_ips" {
  description = "Public IPs of the web servers"
  value       = aws_instance.web[*].public_ip
  sensitive   = false
}
