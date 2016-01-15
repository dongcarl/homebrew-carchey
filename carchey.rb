class Carchey < Formula
  desc "Archey script clone for OS X written in C"
  homepage "https://github.com/dongcarl/carchey"
  url "https://github.com/dongcarl/carchey/archive/v1.0.tar.gz"
  sha256 "6e9a4060134d8d5057071925ab77bf492748b275b66b72ed1ef7d2ede456404f"

  def install
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    system "false"
  end
end
