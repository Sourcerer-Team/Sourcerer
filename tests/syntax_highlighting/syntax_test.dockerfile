# syntax=docker/dockerfile:1
# Syntax highlighting test for Dockerfiles: multi-stage builds, instructions,
# arguments, environment variables, JSON and shell forms, heredocs and comments.

ARG GO_VERSION=1.22
ARG BASE_IMAGE="golang:${GO_VERSION}-alpine"

FROM ${BASE_IMAGE} AS builder
LABEL maintainer="platform@example.com" version='1.0'
WORKDIR /src

ENV CGO_ENABLED=0 \
    GOOS=linux
COPY go.mod go.sum ./
RUN --mount=type=cache,target=/go/pkg/mod \
    go mod download
COPY . .
RUN go build -o /out/app ./cmd/app \
 && strip /out/app

# A second stage, from a pinned digest.
FROM alpine:3.19@sha256:c5b1261d6d3e43071626931fc004f70149baeba2c8ec672bd4f27761f8e1ad6b
RUN apk add --no-cache ca-certificates tzdata
USER 65532:65532
COPY --from=builder /out/app /usr/local/bin/app
EXPOSE 8080/tcp 9090/udp
VOLUME ["/data"]
HEALTHCHECK --interval=30s --timeout=3s CMD wget -qO- http://localhost:8080/health || exit 1
STOPSIGNAL SIGTERM
ENTRYPOINT ["/usr/local/bin/app"]
CMD ["--port", "8080"]

ADD https://example.com/config.tar.gz /tmp/
ONBUILD RUN echo "built child image"
SHELL ["/bin/sh", "-c"]
RUN <<EOF
echo "heredoc line one"
echo "heredoc with $HOME"
EOF
