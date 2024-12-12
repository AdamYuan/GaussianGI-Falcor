#!/usr/bin/env bash
cd "$(dirname "$0")"
find -type f -name "*.slang*" -exec cp '{}' '../../cmake-build-release/bin/shaders/GaussianGI/{}' ';'
find -type f -name "*.slang*" -exec cp '{}' '../../cmake-build-debug/bin/shaders/GaussianGI/{}' ';'
