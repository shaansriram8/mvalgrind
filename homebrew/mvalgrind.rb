class Mvalgrind < Formula
  desc "Valgrind for macOS — runs Valgrind in a local Docker container"
  homepage "https://github.com/shaansriram/mvalgrind"
  license "MIT"
  version "0.1.0"

  on_arm do
    url "https://github.com/shaansriram/mvalgrind/releases/download/v0.1.0/mvalgrind-0.1.0-arm64-apple-darwin.tar.gz"
    sha256 "PLACEHOLDER_ARM64_SHA256"
  end

  on_intel do
    url "https://github.com/shaansriram/mvalgrind/releases/download/v0.1.0/mvalgrind-0.1.0-x86_64-apple-darwin.tar.gz"
    sha256 "PLACEHOLDER_X86_64_SHA256"
  end

  # 'docker' installs the Docker CLI.  Docker Desktop (a cask) provides the
  # container runtime — see the caveats block below.
  depends_on "docker"

  def install
    bin.install "mvalgrind"
  end

  def caveats
    <<~EOS
      mvalgrind launches containers using the local Docker daemon.
      Make sure Docker Desktop is installed and running before use:

        https://www.docker.com/products/docker-desktop/

      The `docker` formula installed by Homebrew provides only the CLI client.
      Docker Desktop provides the daemon that actually runs containers.

      On the first invocation, mvalgrind will build a small Ubuntu image
      (~200 MB download).  Subsequent runs reuse the cached image and start
      in seconds.
    EOS
  end

  test do
    output = shell_output("#{bin}/mvalgrind --version")
    assert_match "mvalgrind", output
  end
end
