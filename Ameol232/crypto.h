#define SECURITY_WIN32 1
#define TLS_MAX_PACKET_SIZE 50000
#include <Sspi.h>

#ifndef CRYPTO_H
#define CRYPTO_H

struct TlsState {
	CredHandle handle;
    CtxtHandle context;
    SecPkgContext_StreamSizes sizes;
    int received;    // byte count in incoming buffer (ciphertext)
    int used;        // byte count used from incoming buffer to decrypt current packet
    int available;   // byte count available for decrypted bytes
    char* decrypted; // points to incoming buffer where data is decrypted inplace
    char incoming[TLS_MAX_PACKET_SIZE];
};

#endif