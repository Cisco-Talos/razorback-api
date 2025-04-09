#include <razorback/log.h>
#include <razorback/lock.h>
#include <razorback/thread.h>
#include "init.h"


#include <openssl/ssl.h>
#include <openssl/err.h>




static bool Crypto_Initialize_OpenSSL(void)
{

    SSL_load_error_strings ();
    SSL_library_init();
    OpenSSL_add_all_digests();
    return true;
}

bool Crypto_Initialize(void)
{
	return Crypto_Initialize_OpenSSL();
}
