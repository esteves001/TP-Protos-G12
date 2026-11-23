# Cambios sobre el esqueleto de la cátedra

Registro de toda modificación a los archivos provistos por la cátedra
(Juan F. Codagnone, ITBA). Va al informe, sección "problemas encontrados".

Formato: archivo, qué se cambió, por qué, commit.

## selector.c

- `selector_fd_set_nio`: `fcntl(fd, F_GETFD, 0)` → `fcntl(fd, F_GETFL, 0)`.
  `F_GETFD` devuelve los flags del *descriptor* (`FD_CLOEXEC`), no los del
  *open file description*. El `F_SETFL` posterior recibía flags equivocados:
  funcionaba por casualidad (los flags de fd son 0 normalmente) pero podía
  pisar `O_APPEND` u otros flags del archivo. Etapa 0.
- Nota: el `fcntl(i, F_GETFD, 0)` en el manejo de `EBADF` dentro de
  `selector_select` es correcto (solo detecta si el fd es válido) y queda.

## stm.h

- Eliminado el global suelto `struct selector_key *key;`. Era una variable
  global definida en un header (una definición por cada .c que lo incluye)
  sin uso alguno. Como efecto colateral esa línea era lo único que declaraba
  `struct selector_key` antes de los prototipos; se reemplazó por la
  declaración forward `struct selector_key;` (sin ella gcc avisa "declared
  inside parameter list" y los punteros a función no tipan). Etapa 0.

## args.c

- `version()`: texto actualizado a "2026/2 -- Grupo 12". Etapa 0.

## socks5nio.c

El archivo original no compilaba: tenía huecos (`…`) y referenciaba headers
inexistentes (`hello.h`, `request.h`, `socks5nio.h`, `socks5.h`).
Se completó con lo mínimo para la Etapa 0 (ver commit "socks5nio: headers
faltantes..."):

- `#include "request.h"` reemplazado por un `TODO(etapa-1)`: el parser del
  request no existe todavía.
- `hello_process` original devolvía `ERROR` directamente cuando no había
  método aceptable, con lo cual el `05 FF` que ya se había marshalleado nunca
  se enviaba. Ahora siempre pasa a `HELLO_WRITE`, y `hello_write` pasa a
  `ERROR` recién después de vaciar el buffer de salida (RFC 1928 §3: el
  servidor responde `FF` y el cliente cierra).
- `hello_read_init`: se inicializa `d->method = NO_ACCEPTABLE_METHODS`. El
  original dejaba el campo sin inicializar y el callback solo lo escribe si
  llega un método aceptable, así que con `05 01 02` se leía basura.
- `socks5_destroy`: antes de encolar en el pool libera `origin_resolution`
  (si no, el `freeaddrinfo` quedaba pendiente hasta el reuso).
- `socksv5_pool_destroy`: usa `socks5_destroy_` en vez de `free` por el mismo
  motivo, y resetea `pool`/`pool_size`.
- `socks5_new`: al sacar del pool decrementa `pool_size`; el pool del
  esqueleto original solo incrementaba.

## main.c

- Parseo manual de `argc/argv` reemplazado por `parse_args` (args.c).
  Firma `main(const int, char **)` porque `parse_args` recibe `char **`
  (getopt no toma `const char **`).
- `listen(fd, 20)` → `listen(fd, 128)`: 20 es poco para el stress test.
- `signal(SIGPIPE, SIG_IGN)` agregado antes del loop.
- Socket pasivo toma dirección y puerto de `-l`/`-p` (antes solo el puerto
  posicional y `INADDR_ANY`).

## Warnings corregidos

Ver sección siguiente, se completa en el punto 5 de la Etapa 0.
