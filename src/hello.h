#ifndef HELLO_H_pQ4wN8xM2kR7tL1vC6bJ9zF3hD5sA0g
#define HELLO_H_pQ4wN8xM2kR7tL1vC6bJ9zF3hD5sA0g

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"

/**
 * hello.c - parser incremental del mensaje `hello' del cliente
 *           (RFC 1928 §3) y marshall de la respuesta del servidor.
 *
 *   +----+----------+----------+
 *   |VER | NMETHODS | METHODS  |
 *   +----+----------+----------+
 *   | 1  |    1     | 1 to 255 |
 *   +----+----------+----------+
 *
 * El parser se alimenta de a un byte y guarda estado entre llamadas, así
 * que tolera que el mensaje llegue fragmentado. Por cada METHOD recibido
 * invoca el callback `on_authentication_method'.
 *
 * Respuesta del servidor:
 *
 *   +----+--------+
 *   |VER | METHOD |
 *   +----+--------+
 *   | 1  |   1    |
 *   +----+--------+
 */

/** estados del parser */
enum hello_state {
    /** esperando el byte VER */
    hello_version,
    /** esperando el byte NMETHODS */
    hello_nmethods,
    /** leyendo los NMETHODS bytes de METHODS */
    hello_methods,

    // estados terminales
    /** mensaje completo y válido */
    hello_done,
    /** VER distinto de 0x05 */
    hello_error_unsupported_version,
};

struct hello_parser {
    /** dato de usuario, disponible desde el callback */
    void *data;
    /** invocado una vez por cada método de autenticación recibido */
    void (*on_authentication_method)(struct hello_parser *parser,
                                     const uint8_t method);

    /******** zona privada ********/
    enum hello_state state;
    /** métodos que faltan leer */
    uint8_t          remaining;
};

/** inicializa el parser. `data' y `on_authentication_method' ya deben estar seteados */
void
hello_parser_init(struct hello_parser *p);

/** entrega un byte al parser. retorna el nuevo estado */
enum hello_state
hello_parser_feed(struct hello_parser *p, const uint8_t b);

/**
 * por cada elemento del buffer llama a `hello_parser_feed' hasta que
 * el parsing se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error.
 */
enum hello_state
hello_consume(buffer *b, struct hello_parser *p, bool *errored);

/**
 * Permite distinguir a quien usa hello_parser_feed si debe seguir
 * enviando caracters o no.
 *
 * En caso de haber terminado permite tambien saber si se debe a un error.
 */
bool
hello_is_done(const enum hello_state state, bool *errored);

/**
 * serializa en buff la respuesta al hello: VER METHOD.
 *
 * Retorna la cantidad de bytes escritos (2), o -1 si no hay espacio
 * suficiente en el buffer.
 */
int
hello_marshall(buffer *b, const uint8_t method);

#endif
