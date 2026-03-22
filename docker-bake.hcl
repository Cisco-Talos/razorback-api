# Copyright (c) 2011-2026 Cisco Systems, Inc.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

group "default" {
    targets = [
        "api",
    ]
}

variable "BASE_IMAGE" {
    default = "debian:12-slim"
}

variable "REPO" {
    default = "harbor.vrt.sourcefire.com/pinkhat/"
}

variable "VERSION" {
    default = "0.0.0"
}

variable "OPENTELEMETRY_CPP_VERSION" {
    default = "1.26.0"
}

variable "OPENTELEMETRY_CPP_SHA256" {
    default = "8a878777a18a013e0ee6604629d1b5f29b162354c14489ad1dccd370f14ac372"
}

target "api" {
    dockerfile = "Dockerfile"
    args = {
        BASE_IMAGE               = "${BASE_IMAGE}"
        VERSION                  = "${VERSION}"
        OPENTELEMETRY_CPP_VERSION = "${OPENTELEMETRY_CPP_VERSION}"
        OPENTELEMETRY_CPP_SHA256  = "${OPENTELEMETRY_CPP_SHA256}"
    }
    tags = [
        "${REPO}razorback-api:${VERSION}",
    ]
}
