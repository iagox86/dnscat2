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
require 'libs/swindow'

class Encryptor
  include EncryptorSAS

  ECDH_GROUP = ECDSA::Group::Nistp256

  @@window = SWindow.new(WINDOW, false, { :noinput => true, :id => "crypto-debug", :name => "Debug window for crypto stuff"})
  @@window.puts("This window is for debugging encryption problems!")
  @@window.puts("In general, you can ignore it. :)")
  @@window.puts()
  @@window.puts("One thing to note: you'll see a lot of meaningless errors here,")
  @@window.puts("because of retransmissions and such. They don't necessarily mean")
  @@window.puts("anything!")
  @@window.puts()
  @@window.puts("But if you ARE having crypto problems, please send me these")
  @@window.puts("logs! Don't worry too much about the private keys; they're")
  @@window.puts("session-specific and won't harm anything in the future")
  @@window.puts()

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
    @@window.puts("Creating Encryptor with secret: #{preshared_secret}")

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

  # Returns true if something was changed
  def set_their_public_key(their_public_key_x, their_public_key_y)
    # Check if we're actually changing anything
    if(@keys[:their_public_key_x] == their_public_key_x && @keys[:their_public_key_y] == their_public_key_y)
      @@window.puts("Attempted to set the same public key!")
      return false
    end

    @old_keys = @keys

    @keys = {
      :my_nonce => -1,
      :their_nonce => -1,
    }

    if(ready?())
      @@window.puts("Wow, this session is old (or the client is needy)! Key re-negotiation requested!")
    end

    @keys[:my_private_key]      = 1 + SecureRandom.random_number(ECDH_GROUP.order - 1)
    @keys[:my_public_key]       = ECDH_GROUP.generator.multiply_by_scalar(@keys[:my_private_key])
    @keys[:their_public_key_x]  = their_public_key_x
    @keys[:their_public_key_y]  = their_public_key_y
    @keys[:their_public_key]    = ECDSA::Point.new(ECDH_GROUP, their_public_key_x, their_public_key_y)

    @keys[:shared_secret]       = @keys[:their_public_key].multiply_by_scalar(@keys[:my_private_key]).x

    @keys[:their_authenticator] = _create_authenticator("client", @preshared_secret)
    @keys[:my_authenticator]    = _create_authenticator("server", @preshared_secret)

    @keys[:their_write_key]     = _create_key("client_write_key")
    @keys[:their_mac_key]       = _create_key("client_mac_key")
    @keys[:my_write_key]        = _create_key("server_write_key")
    @keys[:my_mac_key]          = _create_key("server_mac_key")

    @@window.puts("Setting their public key: #{Encryptor.bignum_to_text(@keys[:their_public_key_x])} #{Encryptor.bignum_to_text(@keys[:their_public_key_y])}")
    @@window.puts("Setting my public key: #{Encryptor.bignum_to_text(@keys[:my_public_key].x)} #{Encryptor.bignum_to_text(@keys[:my_public_key].y)}")

    return true
  end

  def set_their_authenticator(their_authenticator)
    if(!@keys[:their_authenticator])
      @@window.puts("Tried to set an authenticator too early")
      raise(DnscatException, "We weren't ready to set an authenticator!")
    end

    if(@keys[:their_authenticator] != their_authenticator)
      @@window.puts("Tried to set a bad authenticator")
      raise(Encryptor::Error, "Authenticator (pre-shared secret) doesn't match!")
    end

    @@window.puts("Successfully authenticated the session")
    @authenticated = true
  end

  def to_s(keys = nil)
    keys = keys || @keys

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
      @@window.puts("Not decrypting data (incoming data seemed to be cleartext): #{data.unpack("H*")}")
      return data
    end

    # Parse out the important fields
    header, signature, nonce, encrypted_body = data.unpack("a5a6a2a*")

    # Put together the data to sign
    signed_data = header + nonce + encrypted_body

    # Check the signature
    correct_signature = SHA3::Digest::SHA256.digest(keys[:their_mac_key] + signed_data)
    if(correct_signature[0,6] != signature)
      @@window.puts("Couldn't verify packet signature!")
      raise(Encryptor::Error, "Invalid signature!")
    end

    # Check the nonce *after* checking the signature (otherwise, we might update the nonce to a bad value and Bad Stuff happens)
    nonce_int = nonce.unpack("n").pop()
    if(nonce_int < keys[:their_nonce])
      @@window.puts("Client tried to use an invalid nonce: #{nonce_int} < #{keys[:their_nonce]}")
      raise(Encryptor::Error, "Invalid nonce!")
    end
    keys[:their_nonce] = nonce_int

    # Decrypt the body
    body = Salsa20.new(keys[:their_write_key], nonce.rjust(8, "\0")).decrypt(encrypted_body)

    #@@window.puts("Decryption successful")
    return header+body
  end

  # By doing this as a single operation, we can always be sure that we're encrypting data
  # with the same key the client use to encrypt data
  def decrypt_and_encrypt(data)
    ## ** Decrypt
    keys = @keys
    begin
      #@@window.puts("Attempting to decrypt with primary key")
      data = _decrypt_packet_internal(keys, data)
      #@@window.puts("Successfully decrypted with primary key")

      # If it was successfully decrypted, make sure the @old_keys will no longer work
      @old_keys = nil
    rescue Encryptor::Error => e
      # Attempt to fall back to old keys
      if(@old_keys.nil?)
        @@window.puts("No secondary key to fallback to")
        raise(e)
      end

      @@window.puts("Attempting to decrypt with secondary key")
      keys = @old_keys
      data = _decrypt_packet_internal(@old_keys, data)
      @@window.puts("Successfully decrypted with secondary key")
    end

    # Send the decrypted data up and get the encrypted data back
    data = yield(data, ready?(keys))

    # If there was an error of some sort, return nothing
    if(data.nil? || data == '')
      return ''
    end

    # If encryption is turned off, return unencrypted data
    if(!ready?(keys))
      @@window.puts("Returning an unencrypted response")
      return data
    end

    ## ** Encrypt
    #@@window.puts("Encrypting the response")

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
