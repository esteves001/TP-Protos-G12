#ifndef SOCKS5NIO_H_hN2xK8rT4mQ9vL1pW6cJ3bF7dA5gZ0e
#define SOCKS5NIO_H_hN2xK8rT4mQ9vL1pW6cJ3bF7dA5gZ0e

#include "selector.h"

/**
 * socks5nio.c - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 *
 * Cada conexión aceptada se modela con un `struct socks5' (privado al .c)
 * y una máquina de estados (stm.c) cuyos eventos son los del selector.
 */

/**
 * Handler de lectura del socket pasivo: acepta la conexión entrante,
 * la pone en modo no bloqueante, instancia su estado y la registra
 * en el selector.
 */
void
socksv5_passive_accept(struct selector_key *key);

/**
 * Libera el pool de `struct socks5' reusables. Llamar al terminar el
 * programa (después de destruir el selector).
 */
void
socksv5_pool_destroy(void);

#endif
