##
# encryptor.rb
# Created October, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'ecdsa'
require 'securerandom'
require 'sha3'

require 'controller/encrypted_packet'
require 'controller/encryptor_sas'
require 'libs/dnscat_exception'

class Encryptor
  attr_reader :public_key_x, :public_key_y, :their_authenticator, :my_authenticator

  include EncryptorSAS

  ECDH_GROUP = ECDSA::Group::Nistp256

  def Encryptor.bignum_to_binary(bn, size=32)
    if(!bn.is_a?(Bignum))
      raise(ArgumentError, "Expected: Bignum; received: #{bn.class}")
    end

    return [bn.to_s(16).rjust(size*2, "\0")].pack("H*")
  end

  def Encryptor.bignum_to_text(bn, size=32)
    if(!bn.is_a?(Bignum))
      raise(ArgumentError, "Expected: Bignum; received: #{bn.class}")
    end

    return Encryptor.bignum_to_binary(bn, size).unpack("H*").pop()
  end

  def Encryptor.binary_to_bignum(binary)
    if(!binary.is_a?(String))
      raise(ArgumentError, "Expected: String; received: #{binary.class}")
    end

    return binary.unpack("H*").pop().to_i(16)
  end

  def _create_key(key_name)
    return SHA3::Digest::SHA256.digest(Encryptor.bignum_to_binary(@shared_secret) + key_name)
  end

  def _create_authenticator(name, preshared_secret)
    return SHA3::Digest::SHA256.digest(name +
      Encryptor.bignum_to_binary(@shared_secret) +
      Encryptor.bignum_to_binary(@their_public_key.x) +
      Encryptor.bignum_to_binary(@their_public_key.y) +
      Encryptor.bignum_to_binary(@my_public_key.x) +
      Encryptor.bignum_to_binary(@my_public_key.y) +
      preshared_secret
    )
  end

  def initialize(their_public_key_x, their_public_key_y, preshared_secret)
    @my_nonce = -1
    @their_nonce = -1

    #@my_private_key = 1 + SecureRandom.random_number(ECDH_GROUP.order - 1)
    @my_private_key = 43951174550322390566248264230503370167897136622816423934586799375127212096389
    @my_public_key  = ECDH_GROUP.generator.multiply_by_scalar(@my_private_key)
    @their_public_key = ECDSA::Point.new(ECDH_GROUP, their_public_key_x, their_public_key_y)

    @shared_secret = @their_public_key.multiply_by_scalar(@my_private_key).x

    @their_authenticator = _create_authenticator("client", preshared_secret)
    @my_authenticator    = _create_authenticator("server", preshared_secret)

    @their_write_key  = _create_key("client_write_key")
    @their_mac_key    = _create_key("client_mac_key")
    @my_write_key    = _create_key("server_write_key")
    @my_mac_key      = _create_key("server_mac_key")
  end

  def to_s()
    out = []
    out << "My private key:       #{Encryptor.bignum_to_text(@my_private_key)}"
    out << "My public key [x]:    #{Encryptor.bignum_to_text(@my_public_key.x)}"
    out << "My public key [y]:    #{Encryptor.bignum_to_text(@my_public_key.y)}"
    out << "Their public key [x]: #{Encryptor.bignum_to_text(@their_public_key.x)}"
    out << "Their public key [y]: #{Encryptor.bignum_to_text(@their_public_key.y)}"
    out << "Shared secret:        #{Encryptor.bignum_to_text(@shared_secret)}"
    out << ""
    out << "Their authenticator:  #{@their_authenticator.unpack("H*")}"
    out << "My authenticator:     #{@my_authenticator.unpack("H*")}"
    out << ""
    out << "Their write key: #{@their_write_key.unpack("H*")}"
    out << "Their mac key:   #{@their_mac_key.unpack("H*")}"
    out << "My write key:    #{@my_write_key.unpack("H*")}"
    out << "My mac key:      #{@my_mac_key.unpack("H*")}"
    out << ""
    out << "SAS: #{get_sas()}"

    return out.join("\n")
  end

  def my_public_key_x()
    return @my_public_key.x
  end

  def my_public_key_x_s()
    return Encryptor.bignum_to_binary(@my_public_key.x)
  end

  def my_public_key_y()
    return @my_public_key.y
  end

  def my_public_key_y_s()
    return Encryptor.bignum_to_binary(@my_public_key.y)
  end

  def my_nonce()
    return @my_nonce += 1
  end

  def _is_their_nonce_valid?(their_nonce)
    if(their_nonce < @their_nonce)
      return false
    end

    @their_nonce = their_nonce
    return true
  end

  def decrypt_packet(data, options)
    encrypted_packet = EncryptedPacket.parse(data, @their_mac_key, @their_write_key, options)

    if(!_is_their_nonce_valid?(encrypted_packet.nonce.unpack("n").pop))
      return nil
    end

    return encrypted_packet.packet
  end

  def encrypt_packet(packet, options)
    EncryptedPacket.new([my_nonce()].pack("n"), packet).to_bytes(@my_mac_key, @my_write_key)
  end
end
