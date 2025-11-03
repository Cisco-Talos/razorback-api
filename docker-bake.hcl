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

target "api" {
    dockerfile = "Dockerfile"
    args = {
        BASE_IMAGE = "${BASE_IMAGE}"
        VERSION    = "${VERSION}"
    }
    tags = [
        "${REPO}pyrazorback:${VERSION}",
    ]
}
