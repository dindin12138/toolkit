{ pkgs, ... }:
{
  languages.cplusplus.enable = true;
  packages = with pkgs; [
    cmake
    ninja
    bear
    pkg-config
    clang
    lldb
    gcc
    gdb

    # Test framework criterion
    criterion
    boxfort
    libffi
    libgit2
    openssl
    pcre2
    zlib
    nanomsg
  ];
  env.CC = "clang";
  env.CXX = "clang++";
  enterShell = ''
    echo "🚀 Entered Clang C++ development environment."
    echo "📦 Current Compiler: Clang ($(clang --version | head -n 1 | awk '{print $3}'))"
  '';
}
