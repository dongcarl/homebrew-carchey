class Carchey < Formula
  desc "Archey script clone for OS X written in C"
  homepage "https://github.com/dongcarl/carchey"
  url "https://github.com/dongcarl/carchey/archive/v1.0.tar.gz"
  sha256 "485275b6178a4b1799d3f40de667d534cbc5b0eea7a61803f90f2a4bc4888fe8"

  def install
    system "make", "install", "PREFIX=#{prefix}"
  end

  test do
    system "false"
  end
end
