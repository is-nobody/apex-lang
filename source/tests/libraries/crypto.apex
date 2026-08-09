import os
import string
import crypto

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function crypto_md5()
    assert_equal(crypto.md5(""), "d41d8cd98f00b204e9800998ecf8427e", "md5 empty")
    assert_equal(crypto.md5("abc"), "900150983cd24fb0d6963f7d28e17f72", "md5 abc")
    assert_equal(crypto.md5("message digest"), "f96b697d7cb7938d525a2f31aaf161d0", "md5 message digest")
    assert_equal(crypto.md5("abcdefghijklmnopqrstuvwxyz"), "c3fcd3d76192e4007dfb496cca67e13b", "md5 alphabet")
    assert_equal(crypto.md5("Hello World!"), "ed076287532e86365e841e92bfc50d8c", "md5 Hello World!")

    test_file = "_apex_test_md5_file.txt"
    os.write(test_file, "Hello World!")
    data = os.read(test_file)
    assert_equal(crypto.md5(data), "ed076287532e86365e841e92bfc50d8c", "md5 file via os.read")
    os.delete(test_file)

function crypto_sha1()
    assert_equal(crypto.sha1(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709", "sha1 empty")
    assert_equal(crypto.sha1("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1 abc")
    assert_equal(crypto.sha1("message digest"), "c12252ceda8be8994d5fa0290a47231c1d16aae3", "sha1 message digest")
    assert_equal(crypto.sha1("abcdefghijklmnopqrstuvwxyz"), "32d10c7b8cf96570ca04ce37f2a19d84240d3a89", "sha1 alphabet")
    assert_equal(crypto.sha1("Hello World!"), "2ef7bde608ce5404e97d5f042f95f89f1c232871", "sha1 Hello World!")

    test_file = "_apex_test_sha1_file.txt"
    os.write(test_file, "Hello World!")
    data = os.read(test_file)
    assert_equal(crypto.sha1(data), "2ef7bde608ce5404e97d5f042f95f89f1c232871", "sha1 file via os.read")
    os.delete(test_file)

function crypto_sha256()
    assert_equal(crypto.sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty")
    assert_equal(crypto.sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 abc")
    assert_equal(crypto.sha256("message digest"), "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650", "sha256 message digest")
    assert_equal(crypto.sha256("abcdefghijklmnopqrstuvwxyz"), "71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73", "sha256 alphabet")
    assert_equal(crypto.sha256("Hello World!"), "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069", "sha256 Hello World!")

    test_file = "_apex_test_sha256_file.txt"
    os.write(test_file, "Hello World!")
    data = os.read(test_file)
    assert_equal(crypto.sha256(data), "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069", "sha256 file via os.read")
    os.delete(test_file)

function crypto_sha512()
    assert_equal(crypto.sha512(""), "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e", "sha512 empty")
    assert_equal(crypto.sha512("abc"), "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", "sha512 abc")
    assert_equal(crypto.sha512("message digest"), "107dbf389d9e9f71a3a95f6c055b9251bc5268c2be16d6c13492ea45b0199f3309e16455ab1e96118e8a905d5597b72038ddb372a89826046de66687bb420e7c", "sha512 message digest")
    assert_equal(crypto.sha512("abcdefghijklmnopqrstuvwxyz"), "4dbff86cc2ca1bae1e16468a05cb9881c97f1753bce3619034898faa1aabe429955a1bf8ec483d7421fe3c1646613a59ed5441fb0f321389f77f48a879c7b1f1", "sha512 alphabet")
    assert_equal(crypto.sha512("Hello World!"), "861844d6704e8573fec34d967e20bcfef3d424cf48be04e6dc08f2bd58c729743371015ead891cc3cf1c9d34b49264b510751b1ff9e537937bc46b5d6ff4ecc8", "sha512 Hello World!")

    test_file = "_apex_test_sha512_file.txt"
    os.write(test_file, "Hello World!")
    data = os.read(test_file)
    assert_equal(crypto.sha512(data), "861844d6704e8573fec34d967e20bcfef3d424cf48be04e6dc08f2bd58c729743371015ead891cc3cf1c9d34b49264b510751b1ff9e537937bc46b5d6ff4ecc8", "sha512 file via os.read")
    os.delete(test_file)

function crypto_hmac_md5()
    assert_equal(crypto.hmac_md5("", ""), "74e6f7298a9c2d168935f58c001bad88", "hmac_md5 empty key+msg")
    assert_equal(crypto.hmac_md5("key", "The quick brown fox jumps over the lazy dog"), "80070713463e7749b90c2dc24911e275", "hmac_md5 key + brown fox")
    assert_equal(crypto.hmac_md5("secret", "Hello World!"), "09185d5e4c0a674aa431cb8411e327ad", "hmac_md5 secret + Hello World!")

function crypto_hmac_sha1()
    assert_equal(crypto.hmac_sha1("", ""), "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d", "hmac_sha1 empty key+msg")
    assert_equal(crypto.hmac_sha1("key", "The quick brown fox jumps over the lazy dog"), "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9", "hmac_sha1 key + brown fox")
    assert_equal(crypto.hmac_sha1("secret", "Hello World!"), "5efed98b0787c83f9cb0135ba283c390ca49320e", "hmac_sha1 secret + Hello World!")

function crypto_hmac_sha256()
    assert_equal(crypto.hmac_sha256("", ""), "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad", "hmac_sha256 empty key+msg")
    assert_equal(crypto.hmac_sha256("key", "The quick brown fox jumps over the lazy dog"), "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8", "hmac_sha256 key + brown fox")
    assert_equal(crypto.hmac_sha256("secret", "Hello World!"), "6fa7b4dea28ee348df10f9bb595ad985ff150a4adfd6131cca677d9acee07dc6", "hmac_sha256 secret + Hello World!")

function crypto_hmac_sha512()
    assert_equal(crypto.hmac_sha512("", ""), "b936cee86c9f87aa5d3c6f2e84cb5a4239a5fe50480a6ec66b70ab5b1f4ac6730c6c515421b327ec1d69402e53dfb49ad7381eb067b338fd7b0cb22247225d47", "hmac_sha512 empty key+msg")
    assert_equal(crypto.hmac_sha512("key", "The quick brown fox jumps over the lazy dog"), "b42af09057bac1e2d41708e48a902e09b5ff7f12ab428a4fe86653c73dd248fb82f948a549f7b791a5b41915ee4d1ec3935357e4e2317250d0372afa2ebeeb3a", "hmac_sha512 key + brown fox")
    assert_equal(crypto.hmac_sha512("secret", "Hello World!"), "fd3716eb8e1903ecb8deb9320f1baecf0f8dbe04e4c4c71e64fbb37080222ca42deb0b646cabeb53e269a2446507fb5442961656f8007959bd330ef48df0e13e", "hmac_sha512 secret + Hello World!")

function crypto_pbkdf2_md5()
    result = crypto.pbkdf2_md5("password", "salt", 1000, 16)
    assert_equal(result, "8d189946a32d883622a16ae18af0632f", "pbkdf2_md5 password salt 1000 16")

function crypto_pbkdf2_sha1()
    result = crypto.pbkdf2_sha1("password", "salt", 1000, 20)
    assert_equal(result, "6e88be8bad7eae9d9e10aa061224034fed48d03f", "pbkdf2_sha1 password salt 1000 20")

function crypto_pbkdf2_sha256()
    result = crypto.pbkdf2_sha256("password", "salt", 1000, 32)
    assert_equal(result, "632c2812e46d4604102ba7618e9d6d7d2f8128f6266b4a03264d2a0460b7dcb3", "pbkdf2_sha256 password salt 1000 32")

function crypto_pbkdf2_sha512()
    result = crypto.pbkdf2_sha512("password", "salt", 1000, 64)
    assert_equal(result, "afe6c5530785b6cc6b1c6453384731bd5ee432ee549fd42fb6695779ad8a1c5bf59de69c48f774efc4007d5298f9033c0241d5ab69305e7b64eceeb8d834cfec", "pbkdf2_sha512 password salt 1000 64")

function crypto_aes128()
    key = "1234567890abcdef1234567890abcdef"
    iv = "000102030405060708090a0b0c0d0e0f"
    plaintext = "Hello World!"
    
    encrypted = crypto.aes128_encrypt(key, plaintext, iv)
    assert_equal(encrypted, "8f759cd03aee323c46ae21845f56e72b", "aes128 encrypt")
    decrypted = crypto.aes128_decrypt(key, encrypted, iv)
    assert_equal(decrypted, plaintext, "aes128 decrypt")
    
    encrypted_empty = crypto.aes128_encrypt(key, "", iv)
    assert_equal(encrypted_empty, "74e150c9013672f5c53ebf95071edc56", "aes128 empty encrypt")
    decrypted_empty = crypto.aes128_decrypt(key, encrypted_empty, iv)
    assert_equal(decrypted_empty, "", "aes128 empty decrypt")
    
    long_msg = "The quick brown fox jumps over the lazy dog"
    encrypted_long = crypto.aes128_encrypt(key, long_msg, iv)
    assert_equal(encrypted_long, "12fecbf064069b916bc6c28616ae9f68bdb5335dc02db091bb858fdd91bcf13eed012960e97f1e3db0b264d9d4333741", "aes128 long encrypt")
    decrypted_long = crypto.aes128_decrypt(key, encrypted_long, iv)
    assert_equal(decrypted_long, long_msg, "aes128 long decrypt")
    
    wrong_key = "0987654321fedcba0987654321fedcba"
    decrypted_wrong = crypto.aes128_decrypt(wrong_key, encrypted, iv)
    assert_equal(decrypted_wrong != plaintext, true, "aes128 wrong key")
    
    iv2 = "0f0e0d0c0b0a09080706050403020100"
    encrypted_iv2 = crypto.aes128_encrypt(key, plaintext, iv2)
    assert_equal(encrypted != encrypted_iv2, true, "aes128 different IVs")
    
    test_file = "_apex_test_aes_file.txt"
    os.write(test_file, "Secret file content!")
    data = os.read(test_file)
    encrypted_file = crypto.aes128_encrypt(key, data)
    decrypted_file = crypto.aes128_decrypt(key, encrypted_file)
    assert_equal(decrypted_file, data, "aes128 file")
    os.delete(test_file)

function token_hex()
    nbytes = 16
    hex_str = crypto.token_hex(nbytes)
    assert_equal(string.length(hex_str), nbytes * 2, "token_hex length")

function secure_randint()
    n = 100
    res = crypto.secure_randint(n)
    is_valid = res >= 0 and res < n
    assert_equal(is_valid, true, "secure_randint range [0,99]")

function compare_digest()
    a = "secret_token_123"
    b = "secret_token_123"
    res = crypto.compare_digest(a, b)
    assert_equal(res, true, "compare_digest identical strings")

crypto_md5()
crypto_sha1()
crypto_sha256()
crypto_sha512()

crypto_hmac_md5()
crypto_hmac_sha1()
crypto_hmac_sha256()
crypto_hmac_sha512()

crypto_pbkdf2_md5()
crypto_pbkdf2_sha1()
crypto_pbkdf2_sha256()
crypto_pbkdf2_sha512()

crypto_aes128()

token_hex()
secure_randint()
compare_digest()

if tests_failed == 0
    os.output("[OK] libraries/crypto.apex")