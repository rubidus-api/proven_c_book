proven vendored snapshot: v0.114.0 (2026-11-12, from local proven_c_lib 5236974e)

Contents: proven_base of that tag (`include/`, `src/proven/`, `platform/`) with `LICENSE` and
`THIRD_PARTY_NOTICES.md`. From v0.113.0 upstream ships cryptography, certificates and compression
as libraries of their own in the same tree, and this book uses none of them, so they are left out:

- proven_crypto (`crypto/`): SHA-2, HMAC, AES, AEAD, key exchange, signing, DER, X.509;
- proven_codec (`codec/`): DEFLATE with zlib and gzip framing, Zstandard, Brotli;
- proven_net (`net/`): the network layer.

Every file here is byte-for-byte the upstream file. `include/proven.h` includes the crypto, codec
and network umbrella headers only when they are on the include path (`__has_include`), so it needs
no change. `THIRD_PARTY_NOTICES.md` still describes the RFC 7932 Brotli tables, which live in
`codec/` and are not shipped here. `include/proven/alias_xcv.h` defines aliases for functions of the
left-out libraries; they are macros only and nothing here uses them.
