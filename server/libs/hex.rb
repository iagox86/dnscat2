##
# hex.rb
# Created November, 2012
# By Ron Bowes
#
# See: LICENSE.md
#
# This is a simple utility class that converts an arbitrary binary string into
# a user-readable string.
##

class Hex
  BYTE_NUMBER_LENGTH  = 8
  SPACES_BEFORE_HEX   = 2
  SPACES_BEFORE_ASCII = 2
  LINE_LENGTH         = 16

  # Convert the arbitrary binary data, 'data', into a user-readable string.
  def Hex.to_s(data, indent = 0)
    length = data.length
    out = (' ' * indent)

    0.upto(length - 1) do |i|
      if((i % LINE_LENGTH) == 0)
        if(i != 0)
          out = out + "\n" + (' ' * indent)
        end
        out = out + ("%0#{BYTE_NUMBER_LENGTH}X" % i) + " " * SPACES_BEFORE_HEX
      end

      out = out + ("%02X " % data[i].ord)

      if(((i + 1) % LINE_LENGTH) == 0)
        out = out + (" " * SPACES_BEFORE_ASCII)
        LINE_LENGTH.step(1, -1) do |j|
          out = out + ("%c" % ((data[i + 1 - j].ord > 0x20 && data[i + 1 - j].ord < 0x80) ? data[i + 1 - j].ord : ?.))
        end
      end

    end

    (length % LINE_LENGTH).upto(LINE_LENGTH - 1) do |i|
      out = out + ("   ") # The width of a hex character and a space
    end
    out = out + (' ' * SPACES_BEFORE_ASCII)

    (length - (length % LINE_LENGTH)).upto(length - 1) do |i|
      out = out + ("%c" % ((data[i].ord > 0x20 && data[i].ord < 0x80) ? data[i].ord : ?.))
    end

    return out
  end
end
