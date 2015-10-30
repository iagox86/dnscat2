##
# encryptor.rb
# Created October, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'ecdsa'
require 'salsa20'
require 'securerandom'
require 'sha3'

require 'controller/encryptor_sas'
require 'libs/dnscat_exception'

class Encryptor
  include EncryptorSAS

  ECDH_GROUP = ECDSA::Group::Nistp256

  class Error < StandardError
  end

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
    @authenticated = false

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
    @old_keys = nil
  end

  def set_their_public_key(their_public_key_x, their_public_key_y)
    @old_keys = @keys

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

  def set_their_authenticator(their_authenticator)
    if(!@keys[:their_authenticator])
      raise(DnscatException, "We weren't ready to set an authenticator!")
    end

    if(@keys[:their_authenticator] != their_authenticator)
      raise(Encryptor::Error, "Authenticator (pre-shared secret) doesn't match!")
    end

    @authenticated = true
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

  # We use this special internal function so we can try decrypting with different keys
  def _decrypt_packet_internal(keys, data)
    # Don't decrypt if we don't have a key set
    if(!ready?(keys))
      return data
    end

    # Parse out the important fields
    header, signature, nonce, encrypted_body = data.unpack("a5a6a2a*")

    # Check and update their nonce as soon as we can
    nonce_int = nonce.unpack("n").pop()
    if(nonce_int < keys[:their_nonce])
      return false
    end
    keys[:their_nonce] = nonce_int

    # Put together the data to sign
    signed_data = header + nonce + encrypted_body

    # Check the signature
    correct_signature = SHA3::Digest::SHA256.digest(keys[:their_mac_key] + signed_data)
    if(correct_signature[0,6] != signature)
      raise(Encryptor::Error, "Invalid signature!")
    end

    # Decrypt the body
    body = Salsa20.new(keys[:their_write_key], nonce.rjust(8, "\0")).decrypt(encrypted_body)

    return header+body
  end

  def decrypt_packet(data)
    begin
      data = _decrypt_packet_internal(@keys, data)

      # If it was successfully decrypted, make sure the @old_keys will no longer work
      @old_keys = nil
    rescue Encryptor::Error => e
      # Attempt to fall back to old keys
      if(@old_keys.nil?)
        raise(e)
      end

      puts("SUCCESSFULLY DECRYPTED W/ OLD KEY") # TODO: Delete
      data = Encryptor._decrypt_packet_internal(@old_keys, data)
    end

    return data
  end

  def encrypt_packet(data, old = false)
    keys = @keys
    if(old && @old_keys)
      keys = @old_keys
    end

    # Don't encrypt if we don't have a key set
    if(!ready?(keys))
      return data
    end

    # Split the packet into a header and a body
    header, body = data.unpack("a5a*")

    # Encode the nonce properly
    nonce = [keys[:my_nonce]].pack("n")

    # Encrypt the body
    encrypted_body = Salsa20.new(keys[:my_write_key], nonce.rjust(8, "\0")).encrypt(body)

    # Sign it
    signature = SHA3::Digest::SHA256.digest(keys[:my_mac_key] + header + nonce + encrypted_body)

    # Arrange things appropriately
    return [header, signature[0,6], nonce, encrypted_body].pack("a5a6a2a*")
  end

  def my_authenticator()
    return @keys[:my_authenticator]
  end

  def ready?(keys = nil)
    return !(keys || @keys)[:shared_secret].nil?
  end

  def authenticated?()
    return @authenticated
  end
end
