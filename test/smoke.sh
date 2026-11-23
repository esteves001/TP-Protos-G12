#!/bin/sh
# smoke.sh - verificación de humo de la Etapa 0 (solo hello).
#
# Uso: test/smoke.sh [bin/socks5d] [puerto] [valgrind|""]
#
# 1. 05 01 00  -> responde 05 00 y cierra.
# 2. 05 01 02  -> responde 05 FF y cierra (user/pass aún no soportado).
# 3. SIGTERM   -> el servidor termina con código 0.
set -u
BIN=${1:-bin/socks5d}
PORT=${2:-1080}
WRAP=${3:-}
fail=0

$WRAP "$BIN" -p "$PORT" >/dev/null 2>smoke-server.err &
pid=$!
# esperar a que escuche
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if nc -z 127.0.0.1 "$PORT" 2>/dev/null; then break; fi
    sleep 0.25
done

check() {
    name=$1; send=$2; want=$3
    got=$(printf "$send" | nc -w 2 127.0.0.1 "$PORT" | od -An -tx1 | tr -d ' \n')
    if [ "$got" = "$want" ]; then
        echo "ok   $name: got $got"
    else
        echo "FAIL $name: want $want got '$got'"
        fail=1
    fi
}

check "hello noauth    (05 01 00)" '\005\001\000' "0500"
check "hello userpass  (05 01 02)" '\005\001\002' "05ff"
check "hello 2 methods (05 02 02 00)" '\005\002\002\000' "0500"
check "hello bad ver   (04 01 00)" '\004\001\000' ""

kill -TERM $pid
wait $pid
rc=$?
if [ $rc -eq 0 ]; then
    echo "ok   SIGTERM: exit code 0"
else
    echo "FAIL SIGTERM: exit code $rc"
    fail=1
fi
echo "--- stderr del servidor:"
cat smoke-server.err
rm -f smoke-server.err
exit $fail
