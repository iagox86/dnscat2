##
# crypto_helper.rb
# Created December, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Implements functions that encryptor.rb (packet.rb) need.
##

class CryptoHelper
  def CryptoHelper.bignum_to_binary(bn, size=32)
    if(!bn.is_a?(Integer))
      raise(ArgumentError, "Expected: Integer; received: #{bn.class}")
    end

    return [bn.to_s(16).rjust(size*2, "\0")].pack("H*")
  end

  def CryptoHelper.bignum_to_text(bn, size=32)
    if(!bn.is_a?(Integer))
      raise(ArgumentError, "Expected: Integer; received: #{bn.class}")
    end

    return CryptoHelper.bignum_to_binary(bn, size).unpack("H*").pop()
  end

  def CryptoHelper.binary_to_bignum(binary)
    if(!binary.is_a?(String))
      raise(ArgumentError, "Expected: String; received: #{binary.class}")
    end

    return binary.unpack("H*").pop().to_i(16)
  end
end
