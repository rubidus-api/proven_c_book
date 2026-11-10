proven vendored snapshot: v0.112.5 (2026-11-10, from local proven_c_lib 1206c540)

Contents: the library core of that tag (`include/`, `src/`, `platform/`) with `LICENSE` and
`THIRD_PARTY_NOTICES.md`, left out where this book does not use it:

- the network layer (`net/`, header `proven_net.h`);
- cryptography: `aes`, `aead`, `hmac`, `kex`, `sign`, `der`, `hash_legacy` and the internal
  `crypto_*`, `dh` units; platform `proven_sys_aes`, `proven_sys_chacha`, `proven_sys_cpu`;
- compression: `deflate`, `zstd`, `brotli` (with its RFC 7932 tables);
- certificates and URLs: `cert`, `cert_issue`, `cert_system`, `url`; platform `proven_sys_trust`.

Every file that is here is byte-for-byte the upstream file, except `include/proven.h`, whose
includes of the left-out headers are removed (one comment line marks the place).
`include/proven/alias_xcv.h` still defines aliases for the left-out functions; they are macros
only and nothing here uses them.
