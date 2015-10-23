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

  def initialize(nonce, packet)
    if(nonce.length != 2)
      raise(DnscatException, "Invalid nonce: #{nonce}")
    end

    @nonce  = nonce
    @packet = packet
  end

  def EncryptedPacket.parse(data, their_mac_key, their_write_key, options = nil)
    puts("DATA: #{data.unpack("H*")}")
    header, signature, nonce, encrypted_body = data.unpack("a5a6a2a*")

    puts("VERIFYING THEIR PACKET:")
    puts("mac_key: #{their_mac_key.unpack("H*")}")
    puts("header: #{header.unpack("H*")}")
    puts("nonce+body: #{nonce.unpack("H*")}#{encrypted_body.unpack("H*")}")

    # Put together the data to sign
    signed_data = header + nonce + encrypted_body

    # Check the signature
    correct_signature = SHA3::Digest::SHA256.digest(their_mac_key + signed_data)
    if(correct_signature[0,6] != signature)
      raise(DnscatException, "Invalid signature on message!")
    end

    puts("SIGNATURE WAS GOOD!")

    # Decrypt the body
    body = Salsa20.new(their_write_key, "\0\0\0\0\0\0" + nonce).decrypt(encrypted_body)

    return EncryptedPacket.new(nonce, Packet.parse(header + body, options))
  end

  def to_s()
    #return "[Will be encrypted 0x%x] %s" % [@nonce, @packet.to_s]
  end

  def to_bytes(our_mac_key, our_write_key)
    # Split the packet into a header and a body
    header, body = @packet.to_bytes().unpack("a5a*")

    # Encrypt the body
    puts("\nENCRYPTING")
    puts("Key: #{our_write_key.unpack("H*")}")
    puts("Nonce: #{("\0\0\0\0\0\0" + @nonce).unpack("H*")}")
    puts("Decrypted body: #{body.unpack("H*")}")
    encrypted_body = Salsa20.new(our_write_key, "\0\0\0\0\0\0" + @nonce).encrypt(body)
    puts("Encrypted body: #{encrypted_body.unpack("H*")}")

    puts("SIGNING OUR PACKET:")
    puts("mac_key: #{our_mac_key.unpack("H*")}")
    puts("header: #{header.unpack("H*")}")
    puts("nonce+body: #{@nonce.unpack("H*")}#{encrypted_body.unpack("H*")}")

    # Sign it
    signature = SHA3::Digest::SHA256.digest(our_mac_key + header + @nonce + encrypted_body)
    puts("signature: #{signature.unpack("H*")}")

    # Arrange things appropriately
    return [header, signature[0,6], @nonce, encrypted_body].pack("a5a6a2a*")
  end
end
