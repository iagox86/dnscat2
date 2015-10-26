##
# encrypted_packet.rb
# Created October, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Builds and parses encrypted dnscat2 packets.
##

require 'salsa20'

class EncryptedPacket
  attr_reader :nonce, :packet

  #@@crypto_status = SWindow.new(nil, false, { :noinput => true, :id => "crypto", :name => "Crypto status"})

  def initialize(nonce, packet)
    # TODO: Make the nonce into an integer
    if(nonce.length != 2)
      raise(DnscatException, "Invalid nonce: #{nonce}")
    end

    @nonce  = nonce
    @packet = packet
  end

  def EncryptedPacket.parse(data, their_mac_key, their_write_key, options = nil)
    header, signature, nonce, encrypted_body = data.unpack("a5a6a2a*")

    # Put together the data to sign
    signed_data = header + nonce + encrypted_body

    # Check the signature
    correct_signature = SHA3::Digest::SHA256.digest(their_mac_key + signed_data)
    if(correct_signature[0,6] != signature)
      raise(DnscatException, "Invalid signature on message!")
    end

    # Decrypt the body
    body = Salsa20.new(their_write_key, nonce.rjust(8, "\0")).decrypt(encrypted_body)

    return EncryptedPacket.new(nonce, Packet.parse(header + body, options))
  end

  def to_s()
    #return "[Will be encrypted 0x%x] %s" % [@nonce, @packet.to_s]
  end

  def to_bytes(our_mac_key, our_write_key)
    # Split the packet into a header and a body
    header, body = @packet.to_bytes().unpack("a5a*")

    # Encrypt the body
    encrypted_body = Salsa20.new(our_write_key, @nonce.rjust(8, "\0")).encrypt(body)

    # Sign it
    signature = SHA3::Digest::SHA256.digest(our_mac_key + header + @nonce + encrypted_body)

    # Arrange things appropriately
    return [header, signature[0,6], @nonce, encrypted_body].pack("a5a6a2a*")
  end
end
