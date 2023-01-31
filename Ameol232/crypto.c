#include "main.h"
#include "crypto.h"

// https://gist.github.com/mmozeiko/c0dfcc8fec527a90a02145d2cc0bfb6d

#define SECURITY_WIN32
#include <sspi.h>
#include <schannel.h>
#include <security.h>
#include <shlwapi.h>

#define TLS_MAX_PACKET_SIZE (16384+512) // payload + extra over head for header/mac/padding (probably an overestimate)



// Will connect to the server such that tls_read and tls_write can function
int tls_connect(SOCKET sock, struct TlsState* s) {
	SCHANNEL_CRED cred;
	CredHandle handle;
	HRESULT result;
	CtxtHandle* context = NULL;

	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;
	cred.grbitEnabledProtocols = 0xc00; // SP_PROT_TLS1_2

	result = AcquireCredentialsHandleA(
		NULL, // pszPrincipal
		UNISP_NAME, // pszPackage
		SECPKG_CRED_OUTBOUND, // fCredentialUse
		NULL, // pvLogonID
		&cred, // pAuthData
		NULL, NULL, &handle, NULL
	);

	if (result != SEC_E_OK) {
		perror("Invalid result");
		abort();
	}
	
	s->received = 0;
	s->used = 0;
	s->available = 0;
    s->decrypted = NULL;







    result = 0;
    for (;;)
    {
		DWORD flags;
		SECURITY_STATUS sec;
        SecBuffer inbuffers[2] = { 0 };
		SecBuffer outbuffers[1] = { 0 };
        SecBufferDesc indesc = { SECBUFFER_VERSION, ARRAYSIZE(inbuffers), inbuffers };
        SecBufferDesc outdesc = { SECBUFFER_VERSION, ARRAYSIZE(outbuffers), outbuffers };
		int r;

        inbuffers[0].BufferType = SECBUFFER_TOKEN;
        inbuffers[0].pvBuffer = s->incoming;
        inbuffers[0].cbBuffer = s->received;
        inbuffers[1].BufferType = SECBUFFER_EMPTY;

       
        outbuffers[0].BufferType = SECBUFFER_TOKEN;

        flags = ISC_REQ_USE_SUPPLIED_CREDS | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
        sec = InitializeSecurityContextA(
            &s->handle,
            context,
            "cix.co.uk",
            flags,
            0,
            0,
            context ? &indesc : NULL,
            0,
            context ? NULL : &s->context,
            &outdesc,
            &flags,
            NULL);

        // after first call to InitializeSecurityContext context is available and should be reused for next calls
        context = &s->context;

        if (inbuffers[1].BufferType == SECBUFFER_EXTRA)
        {
            MoveMemory(s->incoming, s->incoming + (s->received - inbuffers[1].cbBuffer), inbuffers[1].cbBuffer);
            s->received = inbuffers[1].cbBuffer;
        }
        else
        {
            s->received = 0;
        }

        if (sec == SEC_E_OK)
        {
            // tls handshake completed
            break;
        }
        else if (sec == SEC_I_INCOMPLETE_CREDENTIALS)
        {
            // server asked for client certificate, not supported here
            result = -1;
            break;
        }
        else if (sec == SEC_I_CONTINUE_NEEDED)
        {
            // need to send data to server
            char* buffer = outbuffers[0].pvBuffer;
            int size = outbuffers[0].cbBuffer;

            while (size != 0)
            {
                int d = send(sock, buffer, size, 0);
                if (d <= 0)
                {
                    break;
                }
                size -= d;
                buffer += d;
            }
            FreeContextBuffer(outbuffers[0].pvBuffer);
            if (size != 0)
            {
                // failed to fully send data to server
                result = -1;
                break;
            }
        }
        else if (sec != SEC_E_INCOMPLETE_MESSAGE)
        {
            // SEC_E_CERT_EXPIRED - certificate expired or revoked
            // SEC_E_WRONG_PRINCIPAL - bad hostname
            // SEC_E_UNTRUSTED_ROOT - cannot vertify CA chain
            // SEC_E_ILLEGAL_MESSAGE / SEC_E_ALGORITHM_MISMATCH - cannot negotiate crypto algorithms
            result = -1;
            break;
        }

        // read more data from server when possible
        if (s->received == sizeof(s->incoming))
        {
            // server is sending too much data instead of proper handshake?
            result = -1;
            break;
        }

        r = recv(sock, s->incoming + s->received, sizeof(s->incoming) - s->received, 0);
        if (r == 0)
        {
            // server disconnected socket
            return 0;
        }
        else if (r < 0)
        {
            // socket error
            result = -1;
            break;
        }
        s->received += r;
    }


	return 0;


}

// returns 0 on success or negative value on error
static int tls_write(SOCKET sock, struct TlsState* s, const void* buffer, unsigned int size)
{
    while (size != 0)
    {
		SecBuffer buffers[3];
		SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };
		SECURITY_STATUS sec;
		int total;
		int sent;

        int use = min(size, s->sizes.cbMaximumMessage);

        char wbuffer[TLS_MAX_PACKET_SIZE];
        // assert(s->sizes.cbHeader + s->sizes.cbMaximumMessage + s->sizes.cbTrailer <= sizeof(wbuffer));

        buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
        buffers[0].pvBuffer = wbuffer;
        buffers[0].cbBuffer = s->sizes.cbHeader;
        buffers[1].BufferType = SECBUFFER_DATA;
        buffers[1].pvBuffer = wbuffer + s->sizes.cbHeader;
        buffers[1].cbBuffer = use;
        buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buffers[2].pvBuffer = wbuffer + s->sizes.cbHeader + use;
        buffers[2].cbBuffer = s->sizes.cbTrailer;

        CopyMemory(buffers[1].pvBuffer, buffer, use);



        sec = EncryptMessage(&s->context, 0, &desc, 0);
        if (sec != SEC_E_OK)
        {
            // this should not happen, but just in case check it
            return -1;
        }

        total = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
        sent = 0;
        while (sent != total)
        {
            int d = send(sock, wbuffer + sent, total - sent, 0);
            if (d <= 0)
            {
                // error sending data to socket, or server disconnected
                return -1;
            }
            sent += d;
        }

        buffer = (char*)buffer + use;
        size -= use;
    }

    return 0;
}

// blocking read, waits & reads up to size bytes, returns amount of bytes received on success (<= size)
// returns 0 on disconnect or negative value on error
static int tls_read(SOCKET sock, struct TlsState* s, void* buffer, int size)
{
    int result = 0;
	int r;

    while (size != 0)
    {
        if (s->decrypted)
        {
            // if there is decrypted data available, then use it as much as possible
            int use = min(size, s->available);
            CopyMemory(buffer, s->decrypted, use);
            buffer = (char*)buffer + use;
            size -= use;
            result += use;

            if (use == s->available)
            {
                // all decrypted data is used, remove ciphertext from incoming buffer so next time it starts from beginning
                MoveMemory(s->incoming, s->incoming + s->used, s->received - s->used);
                s->received -= s->used;
                s->used = 0;
                s->available = 0;
                s->decrypted = NULL;
            }
            else
            {
                s->available -= use;
                s->decrypted += use;
            }
        }
        else
        {
            // if any ciphertext data available then try to decrypt it
            if (s->received != 0)
            {
                SecBuffer buffers[4];
				SecBufferDesc desc = { SECBUFFER_VERSION, ARRAYSIZE(buffers), buffers };
				SECURITY_STATUS sec;
                // assert(s->sizes.cBuffers == ARRAYSIZE(buffers));

                buffers[0].BufferType = SECBUFFER_DATA;
                buffers[0].pvBuffer = s->incoming;
                buffers[0].cbBuffer = s->received;
                buffers[1].BufferType = SECBUFFER_EMPTY;
                buffers[2].BufferType = SECBUFFER_EMPTY;
                buffers[3].BufferType = SECBUFFER_EMPTY;

                


				sec = DecryptMessage(&s->context, &desc, 0, NULL);
                if (sec == SEC_E_OK)
                {
                    //assert(buffers[0].BufferType == SECBUFFER_STREAM_HEADER);
                    //assert(buffers[1].BufferType == SECBUFFER_DATA);
                    //assert(buffers[2].BufferType == SECBUFFER_STREAM_TRAILER);

                    s->decrypted = buffers[1].pvBuffer;
                    s->available = buffers[1].cbBuffer;
                    s->used = s->received - (buffers[3].BufferType == SECBUFFER_EXTRA ? buffers[3].cbBuffer : 0);

                    // data is now decrypted, go back to beginning of loop to copy memory to output buffer
                    continue;
                }
                else if (sec == SEC_I_CONTEXT_EXPIRED)
                {
                    // server closed TLS connection (but socket is still open)
                    s->received = 0;
                    return result;
                }
                else if (sec == SEC_I_RENEGOTIATE)
                {
                    // server wants to renegotiate TLS connection, not implemented here
                    return -1;
                }
                else if (sec != SEC_E_INCOMPLETE_MESSAGE)
                {
                    // some other schannel or TLS protocol error
                    return -1;
                }
                // otherwise sec == SEC_E_INCOMPLETE_MESSAGE which means need to read more data
            }
            // otherwise not enough data received to decrypt

            if (result != 0)
            {
                // some data is already copied to output buffer, so return that before blocking with recv
                break;
            }

            if (s->received == sizeof(s->incoming))
            {
                // server is sending too much garbage data instead of proper TLS packet
                return -1;
            }

            // wait for more ciphertext data from server
            r = recv(sock, s->incoming + s->received, sizeof(s->incoming) - s->received, 0);
            if (r == 0)
            {
                // server disconnected socket
                return 0;
            }
            else if (r < 0)
            {
                // error receiving data from socket
                result = -1;
                break;
            }
            s->received += r;
        }
    }

    return result;
}