#!/bin/bash
# Wrapper: run Ghidra headless with the Homebrew JDK 21.
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
export PATH="$JAVA_HOME/bin:$PATH"
exec "$(brew --prefix ghidra)/libexec/support/analyzeHeadless" "$@"
