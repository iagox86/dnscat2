# From https://github.com/havenwood/sha3-pure-ruby
#
# Creative Commons
#
# To the extent possible under law, Christian Neukirchen has waived all copyright
# and related or neighboring rights to the source code of the original Ruby
# implementation. Performance enhancements added by Clemens Gruber. Refactored,
# tests added, and cut into a gem by Shannon Skipper. You can copy, modify,
# distribute and perform this work, even for commercial purposes, all without
# asking permission:
#
# https://creativecommons.org/publicdomain/zero/1.0/
require 'digest'

module Digest
  class SHA3 < Digest::Class
    PILN = [10,  7, 11, 17, 18,  3,  5, 16,
             8, 21, 24,  4, 15, 23, 19, 13,
            12,  2, 20, 14, 22,  9,  6,  1]

    ROTC = [ 1,  3,  6, 10, 15, 21, 28, 36,
            45, 55,  2, 14, 27, 41, 56,  8,
            25, 43, 62, 18, 39, 61, 20, 44]

    RNDC = [0x0000000000000001, 0x0000000000008082, 0x800000000000808a,
            0x8000000080008000, 0x000000000000808b, 0x0000000080000001,
            0x8000000080008081, 0x8000000000008009, 0x000000000000008a,
            0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
            0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
            0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
            0x000000000000800a, 0x800000008000000a, 0x8000000080008081,
            0x8000000000008080, 0x0000000080000001, 0x8000000080008008]

    def initialize hash_size = 512
      @size = hash_size / 8
      @buffer = ''
    end

    def << s
      @buffer << s
      self
    end
    alias update <<

    def reset
      @buffer.clear
      self
    end

    def finish
      s = Array.new 25, 0
      width = 200 - @size * 2

      buffer = @buffer
      buffer << "\x06" << "\0" * (width - buffer.size % width)
      buffer[-1] = (buffer[-1].ord | 0x80).chr


      0.step buffer.size - 1, width do |j|
        quads = buffer[j, width].unpack 'Q*'
        (width / 8).times do |i|
          s[i] ^= quads[i]
        end

        keccak s
      end

      s.pack('Q*')[0, @size]
    end

    private
    def keccak s
      24.times.each_with_object [] do |round, a|
        # Theta
        5.times do |i|
          a[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20]
        end

        5.times do |i|
          t = a[(i + 4) % 5] ^ rotate(a[(i + 1) % 5], 1)
          0.step 24, 5 do |j|
            s[j + i] ^= t
          end
        end

        # Rho Pi
        t = s[1]
        24.times do |i|
          j = PILN[i]
          a[0] = s[j]
          s[j] = rotate t, ROTC[i]
          t = a[0]
        end

        # Chi
        0.step 24, 5 do |j|
          5.times do |i|
            a[i] = s[j + i]
          end

          5.times do |i|
            s[j + i] ^= ~a[(i + 1) % 5] & a[(i + 2) % 5]
          end
        end

        # Iota
        s[0] ^= RNDC[round]
      end
    end

    def rotate x, y
      (x << y | x >> 64 - y) & (1 << 64) - 1
    end
  end
end
