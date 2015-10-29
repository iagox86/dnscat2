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
    return SHA3::Digest::SHA256.digest(Encryptor.bignum_to_binary(@keys[:shared_secret]) + key_name)
  end

  def _create_authenticator(name, preshared_secret)
    return SHA3::Digest::SHA256.digest(name +
      Encryptor.bignum_to_binary(@keys[:shared_secret]) +
      Encryptor.bignum_to_binary(@keys[:their_public_key].x) +
      Encryptor.bignum_to_binary(@keys[:their_public_key].y) +
      Encryptor.bignum_to_binary(@keys[:my_public_key].x) +
      Encryptor.bignum_to_binary(@keys[:my_public_key].y) +
      preshared_secret
    )
  end

  def initialize(preshared_secret)
    @preshared_secret = preshared_secret

    # Start with encryption turned off
    @keys = {
      :my_nonce            => -1,
      :their_nonce         => -1,
      :my_private_key      => nil,
      :my_public_key       => nil,
      :their_public_key    => nil,
      :shared_secret       => nil,
      :their_authenticator => nil,
      :my_authenticator    => nil,
      :their_write_key     => nil,
      :their_mac_key       => nil,
      :my_write_key        => nil,
      :my_mac_key          => nil,
    }
    @old_keys = @keys.clone()
  end

  def set_their_public_key(their_public_key_x, their_public_key_y)
    @old_keys = @keys.clone()

    @keys = {
      :my_nonce => -1,
      :their_nonce => -1,
    }

    @keys[:my_private_key]      = 1 + SecureRandom.random_number(ECDH_GROUP.order - 1)
    @keys[:my_public_key]       = ECDH_GROUP.generator.multiply_by_scalar(@keys[:my_private_key])
    @keys[:their_public_key]    = ECDSA::Point.new(ECDH_GROUP, their_public_key_x, their_public_key_y)

    @keys[:shared_secret]       = @keys[:their_public_key].multiply_by_scalar(@keys[:my_private_key]).x

    @keys[:their_authenticator] = _create_authenticator("client", @preshared_secret)
    @keys[:my_authenticator]    = _create_authenticator("server", @preshared_secret)

    @keys[:their_write_key]     = _create_key("client_write_key")
    @keys[:their_mac_key]       = _create_key("client_mac_key")
    @keys[:my_write_key]        = _create_key("server_write_key")
    @keys[:my_mac_key]          = _create_key("server_mac_key")
  end

  def to_s()
    out = []
    out << "My private key:       #{Encryptor.bignum_to_text(@keys[:my_private_key])}"
    out << "My public key [x]:    #{Encryptor.bignum_to_text(@keys[:my_public_key].x)}"
    out << "My public key [y]:    #{Encryptor.bignum_to_text(@keys[:my_public_key].y)}"
    out << "Their public key [x]: #{Encryptor.bignum_to_text(@keys[:their_public_key].x)}"
    out << "Their public key [y]: #{Encryptor.bignum_to_text(@keys[:their_public_key].y)}"
    out << "Shared secret:        #{Encryptor.bignum_to_text(@keys[:shared_secret])}"
    out << ""
    out << "Their authenticator:  #{@keys[:their_authenticator].unpack("H*")}"
    out << "My authenticator:     #{@keys[:my_authenticator].unpack("H*")}"
    out << ""
    out << "Their write key: #{@keys[:their_write_key].unpack("H*")}"
    out << "Their mac key:   #{@keys[:their_mac_key].unpack("H*")}"
    out << "My write key:    #{@keys[:my_write_key].unpack("H*")}"
    out << "My mac key:      #{@keys[:my_mac_key].unpack("H*")}"
    out << ""
    out << "SAS: #{get_sas()}"

    return out.join("\n")
  end

  def my_public_key_x()
    return @keys[:my_public_key].x
  end

  def my_public_key_x_s()
    return Encryptor.bignum_to_binary(@keys[:my_public_key].x)
  end

  def my_public_key_y()
    return @keys[:my_public_key].y
  end

  def my_public_key_y_s()
    return Encryptor.bignum_to_binary(@keys[:my_public_key].y)
  end

  def my_nonce()
    return @keys[:my_nonce] += 1
  end

  def _is_their_nonce_valid?(their_nonce)
    if(their_nonce < @keys[:their_nonce])
      return false
    end

    @keys[:their_nonce] = their_nonce
    return true
  end

  def decrypt_packet(data, options)
    encrypted_packet = EncryptedPacket.parse(data, @keys[:their_mac_key], @keys[:their_write_key], options)

    if(!_is_their_nonce_valid?(encrypted_packet.nonce.unpack("n").pop))
      return nil
    end

    return encrypted_packet.packet
  end

  def encrypt_packet(packet, options)
    EncryptedPacket.new([my_nonce()].pack("n"), packet).to_bytes(@keys[:my_mac_key], @keys[:my_write_key])
  end

  def their_authenticator()
    return @keys[:their_authenticator]
  end

  def my_authenticator()
    return @keys[:my_authenticator]
  end
end
