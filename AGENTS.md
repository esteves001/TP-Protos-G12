# AGENTS.md — Proxy SOCKSv5 (TP Final, Protocolos de Comunicación, ITBA)

Este archivo es la guía de contexto para cualquier agente de IA o persona que trabaje en este repo.
Leelo entero antes de tocar código. Las reglas de la sección "Restricciones duras" no son negociables:
violarlas significa que el TP se desaprueba.

## 1. Qué es este proyecto

Servidor proxy **SOCKSv5** (RFC 1928) con autenticación usuario/contraseña (RFC 1929), escrito en **C11**,
con **un solo thread** que atiende todas las conexiones mediante **sockets no bloqueantes multiplexados**
(`pselect` vía el módulo `selector`). Incluye además:

- Un **protocolo propio de administración/monitoreo** (métricas y configuración en caliente) en un puerto aparte.
- Un **cliente** de ese protocolo (ejecutable separado).
- Logs de acceso por conexión.
- Makefile, tests unitarios (libcheck), informe y README.

Entrega: tarball del clon del repo git (con historia). Defensa presencial: cada integrante debe poder explicar
cualquier parte del código. Documento de análisis y plan completo del grupo:
https://claude.ai/artifact/QMrypgpZewyf7eTvTvWphU

## 2. Restricciones duras (condiciones excluyentes de aprobación)

1. **Nada bloquea en el thread principal.** Ni `accept`, ni `recv`, ni `send`, ni `connect`, ni `getaddrinfo`.
   Toda operación de I/O ocurre dentro de un handler del selector, sobre un fd previamente puesto en
   `O_NONBLOCK` con `selector_fd_set_nio`, y solo cuando el selector avisó que el fd está listo.
   - `connect()` se hace no bloqueante: se espera `EINPROGRESS`, se suscribe `OP_WRITE` y se verifica con
     `getsockopt(SO_ERROR)`.
   - `getaddrinfo()` **solo** dentro de un `pthread` lanzado para eso, que al terminar llama
     `selector_notify_block()`. Nunca en el thread principal.
   - Las funciones `sock_blocking_*` de `netutils.c` **solo** pueden usarse en el cliente de administración
     (`client/`), nunca en el servidor.
2. **Lecturas y escrituras parciales son la norma.** Todo parser es incremental (consume byte a byte y guarda
   estado entre llamadas). Toda escritura usa el `buffer` y recuerda cuánto falta enviar. Nunca asumir que un
   `recv` trae un mensaje completo ni que un `send` escribe todo.
3. **C11 estricto.** `-std=c11 -pedantic -Wall -Wextra`. El código debe compilar sin warnings.
   `_POSIX_C_SOURCE=200809L` se define en `CFLAGS`, no en los fuentes.
4. **Solo código permitido.** El esqueleto de la cátedra (Juan Codagnone) se usa con atribución. Cualquier otra
   librería o fragmento de terceros requiere licencia OSI y aprobación previa de la cátedra en el foro.
   **No agregar dependencias externas sin consultar al grupo.**
5. **Memoria acotada por conexión.** Dos buffers de tamaño fijo por conexión. Nunca acumular un flujo en
   memoria. El copy entre cliente y origin usa backpressure (ver sección 5).

## 3. Layout del repositorio

```
.
├── AGENTS.md             este archivo
├── README.md             cómo compilar, ejecutar, opciones; ubicación de todo
├── Makefile
├── src/                  servidor socks5d
│   ├── selector.c/h      multiplexor (cátedra)
│   ├── stm.c/h           motor de máquina de estados (cátedra)
│   ├── buffer.c/h        buffer con punteros R/W (cátedra)
│   ├── parser.c/h        motor genérico de parsers byte a byte (cátedra)
│   ├── parser_utils.c/h  parsers reusables, strcmpi (cátedra)
│   ├── netutils.c/h      sockaddr_to_human, I/O bloqueante para el cliente (cátedra)
│   ├── args.c/h          línea de comandos con getopt (cátedra)
│   ├── main.c            arranque: args, sockets pasivos, loop del selector
│   ├── socks5nio.c/h     struct socks5, fd_handler, tabla de estados, accept
│   ├── hello.c/h         parser + marshall del hello (RFC 1928 §3)
│   ├── auth.c/h          parser + marshall user/pass (RFC 1929)
│   ├── request.c/h       parser + marshall del request/reply, errno → REP
│   ├── users.c/h         tabla de usuarios mutable en runtime
│   ├── metrics.c/h       contadores
│   ├── log.c/h           registro de acceso
│   └── admin.c/h         servidor del protocolo de administración
├── client/               cliente del protocolo de administración
├── test/                 *_test.c (libcheck) y scripts de stress
├── docs/
│   ├── protocolo-admin.txt   especificación del protocolo de admin (estilo RFC)
│   └── informe.pdf
└── bin/                  binarios y objetos generados (ignorado por git)
```

Si se mueve o renombra algo, actualizar este archivo.

## 4. Comandos

```sh
make                 # compila bin/socks5d y bin/client
make clean
make test            # compila y corre los tests de libcheck
make debug           # -O0 -g -fsanitize=address,undefined (si está definido)

# correr
ulimit -n 4096
./bin/socks5d -p 1080 -P 8080 -u juan:1234
./bin/client -P 8080 ...

# probar a mano
curl --socks5-hostname juan:1234@127.0.0.1:1080 http://example.com   # manda el FQDN al proxy
curl --socks5 127.0.0.1:1080 http://1.1.1.1                           # resuelve local, manda IP
nc 127.0.0.1 8080                                                     # protocolo de admin si es texto

# memoria
valgrind --leak-check=full --show-leak-kinds=all ./bin/socks5d
```

Entorno de referencia: Ubuntu 25.04 en VirtualBox. Dependencias: `build-essential git valgrind check pkg-config curl netcat-openbsd iperf3`.
El desarrollo se hace por SSH remoto sobre la VM; macOS no es target (faltan `MSG_NOSIGNAL`, etc.).

## 5. Arquitectura (lo que hay que tener en la cabeza para tocar cualquier cosa)

### Tres capas desacopladas

```
selector (pselect)  →  socksv5_read/write/block/close(key)  →  stm_handler_*  →  on_read_ready/... del estado actual
   no sabe de SOCKS         8 líneas de pegamento                 salta de estado      devuelve el próximo estado
```

El selector llama al `fd_handler` registrado para el fd. En `socks5nio.c` ese handler delega en la
`state_machine` de la conexión. Cada estado implementa `on_arrival`, `on_departure`, `on_read_ready`,
`on_write_ready`, `on_block_ready` según necesite, y **devuelve el id del próximo estado**. El motor `stm`
ejecuta `on_departure` del viejo y `on_arrival` del nuevo al saltar.

### Una conexión = un `struct socks5`

- Compartido por `client_fd` y `origin_fd`: ambos se registran en el selector con el mismo puntero en `data`.
- `references` cuenta cuántos fds lo referencian. `handle_close` → `socks5_destroy` libera solo con la última.
- Uniones `client {hello, request, copy}` y `orig {conn, copy}`: los estados son excluyentes en el tiempo, así
  que comparten memoria. Una alocación por conexión, con pool de reuso.
- `socksv5_done` desregistra y cierra los fds; `socks5_destroy` libera memoria. Son cosas distintas, no mezclar.

### Estados de una conexión SOCKS

| Estado | Interés | Qué hace | Siguiente |
|---|---|---|---|
| HELLO_READ | OP_READ client | parser hello | HELLO_WRITE |
| HELLO_WRITE | OP_WRITE client | manda `05 METHOD` | AUTH_READ si METHOD=02, REQUEST_READ si 00, ERROR si FF |
| AUTH_READ | OP_READ client | parser RFC 1929 (VER=**01**) | AUTH_WRITE |
| AUTH_WRITE | OP_WRITE client | manda `01 STATUS` | REQUEST_READ si ok, ERROR si no |
| REQUEST_READ | OP_READ client | parser request | RESOLV si ATYP=03, CONNECTING si 01/04 |
| RESOLV | OP_NOOP (thread) | `getaddrinfo` en pthread, vuelve por `on_block_ready` | CONNECTING |
| CONNECTING | OP_WRITE origin | `connect` no bloqueante, `SO_ERROR`; si falla, próxima `addrinfo` | REQUEST_WRITE |
| REQUEST_WRITE | OP_WRITE client | manda `05 REP ...` (REP=00 solo si conectó) | COPY si ok, ERROR si no |
| COPY | READ/WRITE ambos | túnel bidireccional con backpressure | DONE |
| DONE / ERROR | — | `socksv5_done` | — |

Cualquier estado pasa a ERROR ante error de I/O o de parseo.

### Copy con backpressure

Dos buffers: `c2o` (client→origin) y `o2c` (origin→client). Después de cada I/O se recalculan los intereses:

```
interés(client_fd) = (buffer_can_write(c2o) ? OP_READ : 0) | (buffer_can_read(o2c) ? OP_WRITE : 0)
interés(origin_fd) = (buffer_can_write(o2c) ? OP_READ : 0) | (buffer_can_read(c2o) ? OP_WRITE : 0)
```

Si un buffer está lleno, se deja de leer del lado que lo llena; TCP frena al emisor solo.
`recv` == 0 en un lado ⇒ cuando su buffer se vació, `shutdown(SHUT_WR)` del otro lado (half-close) y
seguir copiando en la otra dirección. La conexión termina cuando ambas direcciones cerraron.

### DNS en thread

`REQUEST_READ` con ATYP=03 hace `pthread_create` con una copia del nombre y puerto. El thread hace
`getaddrinfo(name, port, {ai_family=AF_UNSPEC, ai_socktype=SOCK_STREAM})`, guarda el resultado en
`s->origin_resolution`, llama `selector_notify_block(selector, client_fd)` y termina (detached).
El selector interrumpe su `pselect` por señal y llama `handle_block` → `on_block_ready` de RESOLV.
El thread no toca nada más del `struct socks5`.

### Mapeo errno → REP (request.c)

| Situación | REP |
|---|---|
| conectó | 00 |
| error interno | 01 |
| `ENETUNREACH` | 03 |
| `EHOSTUNREACH`, FQDN no resuelve | 04 |
| `ECONNREFUSED` | 05 |
| `ETIMEDOUT` | 06 |
| CMD ≠ CONNECT | 07 |
| ATYP desconocido | 08 |

### Estado compartido sin locks

Métricas, tabla de usuarios y configuración se leen y escriben **solo** desde el thread del selector
(incluyendo los handlers del protocolo de admin). No hace falta mutex. El único thread aparte (DNS) solo escribe
`origin_resolution` de su propia conexión antes de notificar.

## 6. Convenciones de código

- Estilo del esqueleto: 4 espacios, llaves K&R, `snake_case`, funciones `static` salvo las exportadas en `.h`,
  `goto finally` para limpieza en funciones con varios recursos, `#define N(x) (sizeof(x)/sizeof((x)[0]))`.
- Un módulo = `foo.c` + `foo.h` con include guard. El `.h` documenta la interfaz con comentarios estilo doxygen
  como el esqueleto.
- Parsers: struct con `state` + campos parciales, función `*_parser_init`, `*_parser_feed(p, byte)` que devuelve
  el nuevo estado, `*_consume(buffer, parser, &error)` que alimenta desde un buffer, `*_is_done(state, &error)`,
  `*_marshall(buffer, ...)` para la respuesta. Copiar el patrón de `hello`.
- Cada parser nuevo lleva su `test/foo_test.c` con libcheck que lo alimenta **de a un byte** y también con
  mensajes inválidos.
- `recv`/`send` siempre con `MSG_NOSIGNAL` en `send` (evita SIGPIPE) o `signal(SIGPIPE, SIG_IGN)` en `main`.
- Errores de `selector_*` se verifican siempre (`SELECTOR_SUCCESS`). Un fd nunca se cierra sin desregistrarlo
  antes (`selector_unregister_fd`), o `pselect` falla con `EBADF`.
- No hardcodear tamaños de buffer ni timeouts: salen de la configuración en runtime, leídos al crear cada conexión.
- Logs de acceso a `stdout`, una línea por conexión: `fecha ISO 8601, usuario, ip:puerto cliente, destino tal
  como lo pidió (FQDN o IP), puerto, REP`. Mensajes de diagnóstico a `stderr`.
- Commits chicos y frecuentes, en español o inglés pero consistente, con el nombre real del autor. La cátedra
  revisa la historia de git para evaluar participación.

## 7. Cosas conocidas del esqueleto de la cátedra

- `selector_fd_set_nio` usa `F_GETFD` donde debe usar `F_GETFL`. Corregido/por corregir; mencionar en el informe.
- `ITEMS_MAX_SIZE = FD_SETSIZE = 1024`: 500 clientes × 2 fds está al borde. Decisión pendiente: documentar el
  techo o reimplementar `selector.c` con `poll` manteniendo la interfaz de `selector.h`.
- `listen(fd, 20)` en `main.c` es poco para el stress test; usar ≥ 128.
- `main.c` escucha solo en `AF_INET`. Debe soportar `-l` con IPv6.
- `stm.h` tiene un `struct selector_key *key;` global suelto. Eliminar.
- `args.c` tiene `-N` (disectors) heredado de otro TP. Esperar el documento de parámetros de la cátedra.
- `hello_process` solo acepta método `0x00`; hay que extenderlo para `0x02` según haya usuarios configurados.

## 8. Lo que un agente NO debe hacer

- Reemplazar `selector`/`stm`/`buffer` por otra cosa, o agregar threads por conexión.
- Usar `read`/`write` bloqueantes, `sleep`, `usleep` o esperas activas en el servidor.
- Resolver nombres en el thread principal.
- Agregar librerías externas (libevent, libuv, uthash, cJSON, etc.) sin aprobación de la cátedra.
- Acumular datos de una conexión en memoria dinámica creciente (`realloc` en el copy).
- Tocar métricas o usuarios desde el thread de DNS.
- "Arreglar" warnings de macOS con `#ifdef __APPLE__`: el target es Linux.
- Reescribir archivos de la cátedra sin necesidad; si se cambia algo, documentar qué y por qué (va al informe).

## 9. Estado del proyecto y próximos pasos

Mantener esta sección al día. Marcar con `[x]` lo terminado.

- [ ] Etapa 0: patches aplicados, Makefile, compila sin warnings, tests de la cátedra pasan, bug de `fcntl` corregido, `args` integrado en `main`.
- [ ] Etapa 1: hello + request IPv4 + connecting + copy. Hito: `curl --socks5 127.0.0.1:1080 http://1.1.1.1`.
- [ ] Etapa 2: IPv6, FQDN con thread, iteración de addrinfo, errno → REP, escucha IPv6.
- [ ] Etapa 3: users + auth RFC 1929. Hito: `curl --socks5-hostname user:pass@...`.
- [ ] Etapa 4: log de acceso + métricas.
- [ ] Etapa 5: protocolo de admin (doc primero), servidor y cliente.
- [ ] Etapa 6: timeouts, decisión poll vs select, stress tests, valgrind limpio.
- [ ] Etapa 7: informe (11 secciones en orden), README, script de demo.

## 10. Referencias

- RFC 1928 — SOCKS Protocol Version 5
- RFC 1929 — Username/Password Authentication for SOCKS V5
- RFC 6724 — orden de direcciones que devuelve `getaddrinfo`
- POSIX.1-2008 Utility Conventions (argumentos de línea de comandos)
- "The new pselect() system call", LWN, 2006 — por qué el selector usa `pselect` y señales
- Esqueleto de la cátedra: Juan F. Codagnone, ITBA, 2017–2018 (selector, stm, buffer, parser, netutils, args, socks5nio)
