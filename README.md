# Proxy SOCKSv5 — TP Final, Protocolos de Comunicación (ITBA, 2026/2)

Servidor proxy SOCKSv5 (RFC 1928) con autenticación usuario/contraseña (RFC 1929), en C11,
un solo thread, sockets no bloqueantes multiplexados con `pselect`. Incluye protocolo propio de
administración/monitoreo y su cliente.

Guía completa de arquitectura, restricciones y convenciones: [`AGENTS.md`](AGENTS.md).
Consigna de la cátedra: [`docs/consigna-20262-r0.txt`](docs/consigna-20262-r0.txt).

## Compilar

```sh
make            # bin/socks5d y bin/client
make test       # tests de libcheck (test/*_test.c)
make debug      # -O0 -g -fsanitize=address,undefined
make clean
```

Dependencias (Ubuntu): `build-essential git valgrind check pkg-config curl netcat-openbsd iperf3`.
Target: Linux. macOS no es target.

## Ejecutar

```sh
ulimit -n 4096
./bin/socks5d -p 1080 -P 8080 -u juan:1234
./bin/client -P 8080 ...
```

Probar a mano:

```sh
curl --socks5-hostname juan:1234@127.0.0.1:1080 http://example.com   # FQDN al proxy
curl --socks5 127.0.0.1:1080 http://1.1.1.1                           # IP directa
```

## Ubicación de todo

| Qué | Dónde |
|---|---|
| Servidor `socks5d` | `src/` |
| Cliente del protocolo de admin | `client/` |
| Tests libcheck | `test/` |
| Especificación protocolo de admin | `docs/protocolo-admin.txt` |
| Informe | `docs/informe.pdf` |
| Consigna de la cátedra | `docs/consigna-20262-r0.txt` |
| Patches originales del esqueleto (ya aplicados en la historia git) | `docs/patches/` |
| Binarios y objetos | `bin/` (ignorado por git) |

## Materiales de la cátedra

`selector`, `stm`, `buffer`, `parser`, `parser_utils`, `netutils`, `args`, `socks5nio` y `main` provienen
del esqueleto de Juan F. Codagnone (ITBA, 2017–2018), aplicado vía los patches en `docs/patches/`.
Cambios sobre ese código se documentan en el informe.
