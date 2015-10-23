##
# encrypted_packet.rb
# Created October, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Builds and parses encrypted dnscat2 packets.
##

class EncryptedPacket
  attr_reader :nonce, :packet

  def initialize(nonce, packet)
    if(nonce.length != 8)
      raise(DnscatException, "Invalid nonce: #{nonce}")
    end
  end

  def EncryptedPacket.parse(data, their_mac_key, their_write_key, options = nil)
    header, signature, nonce, encrypted_body = data.unpack("a5a6a2a*")

    # Pad the nonce to 8 bytes (64-bits)
    # TODO: Document this
    nonce = "\0\0\0\0\0\0" + nonce

    # Put together the data to sign
    signed_data = header + nonce + encrypted_body

    # Check the signature
    correct_signature = Digest::SHA3.new(256).digest(their_mac_key + signed_data)
    if(correct_signature[0,6] != signature)
      raise(DnscatException, "Invalid signature on message!")
    end

    # Decrypt the body
    body = Salsa20.new(their_write_key, nonce).decrypt(encrypted_body)

    return EncryptedPacket.new(nonce, Packet.parse(header + body, options))
  end

  def to_s()
    return "[Will be encrypted 0x%x] %s" % [@nonce, @packet.to_s]
  end

  def to_bytes(our_mac_key, our_write_key)
    # Split the packet into a header and a body
    header, body = @packet.to_bytes().unpack("a5a*")

    # Encrypt the body
    encrypted_body = Salsa20.new(our_write_key, @nonce).encrypt(body)

    # Put together the data to sign
    signed_data = header + @nonce + encrypted_body

    # Sign it
    signature = Digest::SHA3.new(256).digest(our_mac_key + signed_data)

    # Arrange things appropriately
    return [header, signature[0,6], @nonce[6,2], body].pack("a5a6a2a*")
  end
end
