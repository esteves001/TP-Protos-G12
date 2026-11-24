/**
 * hello_test.c - tests del parser del hello (RFC 1928 §3)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "hello.h"
#include "socks5.h"
#include "tests.h"

/** registro de los métodos que el parser reporta por callback */
struct seen {
    uint8_t  methods[256];
    unsigned n;
};

static void
on_method(struct hello_parser *p, const uint8_t method) {
    struct seen *s = p->data;
    s->methods[s->n++] = method;
}

static void
parser_setup(struct hello_parser *p, struct seen *s) {
    memset(s, 0, sizeof(*s));
    p->data                     = s;
    p->on_authentication_method = on_method;
    hello_parser_init(p);
}

START_TEST (test_hello_valid_whole) {
    // hello completo, de una: VER=5 NMETHODS=2 METHODS={00, 02}
    uint8_t data[] = { 0x05, 0x02, 0x00, 0x02 };
    uint8_t raw[16];
    buffer b;
    buffer_init(&b, N(raw), raw);
    memcpy(buffer_write_ptr(&b, &(size_t){0}), data, sizeof(data));
    buffer_write_adv(&b, sizeof(data));

    struct hello_parser p;
    struct seen s;
    parser_setup(&p, &s);

    bool errored = false;
    const enum hello_state st = hello_consume(&b, &p, &errored);
    ck_assert_uint_eq(hello_done, st);
    ck_assert(hello_is_done(st, &errored));
    ck_assert(!errored);
    ck_assert(!buffer_can_read(&b));          // consumió todo

    ck_assert_uint_eq(2, s.n);
    ck_assert_uint_eq(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED, s.methods[0]);
    ck_assert_uint_eq(SOCKS_HELLO_USERPASS,                  s.methods[1]);
}
END_TEST

START_TEST (test_hello_valid_byte_by_byte) {
    // el mismo hello pero alimentado de a un byte con hello_parser_feed
    uint8_t data[] = { 0x05, 0x02, 0x00, 0x02 };

    struct hello_parser p;
    struct seen s;
    parser_setup(&p, &s);

    bool errored = false;
    ck_assert_uint_eq(hello_nmethods, hello_parser_feed(&p, data[0]));
    ck_assert(!hello_is_done(p.state, &errored));
    ck_assert_uint_eq(hello_methods,  hello_parser_feed(&p, data[1]));
    ck_assert(!hello_is_done(p.state, &errored));
    ck_assert_uint_eq(hello_methods,  hello_parser_feed(&p, data[2]));
    ck_assert(!hello_is_done(p.state, &errored));
    ck_assert_uint_eq(1, s.n);                // ya avisó el primero
    ck_assert_uint_eq(hello_done,     hello_parser_feed(&p, data[3]));
    ck_assert(hello_is_done(p.state, &errored));
    ck_assert(!errored);

    ck_assert_uint_eq(2, s.n);
    ck_assert_uint_eq(0x00, s.methods[0]);
    ck_assert_uint_eq(0x02, s.methods[1]);
}
END_TEST

START_TEST (test_hello_valid_fragmented_consume) {
    // llega fragmentado por recv: primero 1 byte, después 2, después 1.
    uint8_t data[] = { 0x05, 0x03, 0x00, 0x01, 0x02 };
    const size_t chunks[] = { 1, 2, 2 };
    uint8_t raw[16];
    buffer b;
    buffer_init(&b, N(raw), raw);

    struct hello_parser p;
    struct seen s;
    parser_setup(&p, &s);

    bool errored = false;
    size_t off = 0;
    enum hello_state st = hello_version;
    for(unsigned i = 0; i < N(chunks); i++) {
        memcpy(buffer_write_ptr(&b, &(size_t){0}), data + off, chunks[i]);
        buffer_write_adv(&b, chunks[i]);
        off += chunks[i];
        st = hello_consume(&b, &p, &errored);
        if(i + 1 < N(chunks)) {
            ck_assert(!hello_is_done(st, &errored));
        }
    }
    ck_assert_uint_eq(hello_done, st);
    ck_assert(!errored);
    ck_assert_uint_eq(3, s.n);
    ck_assert_uint_eq(0x00, s.methods[0]);
    ck_assert_uint_eq(0x01, s.methods[1]);
    ck_assert_uint_eq(0x02, s.methods[2]);
}
END_TEST

START_TEST (test_hello_bad_version) {
    uint8_t data[] = { 0x04, 0x01, 0x00 };
    uint8_t raw[16];
    buffer b;
    buffer_init(&b, N(raw), raw);
    memcpy(buffer_write_ptr(&b, &(size_t){0}), data, sizeof(data));
    buffer_write_adv(&b, sizeof(data));

    struct hello_parser p;
    struct seen s;
    parser_setup(&p, &s);

    bool errored = false;
    const enum hello_state st = hello_consume(&b, &p, &errored);
    ck_assert_uint_eq(hello_error_unsupported_version, st);
    ck_assert(hello_is_done(st, &errored));
    ck_assert(errored);
    ck_assert_uint_eq(0, s.n);                // ningún callback
    // no siguió consumiendo después del error
    ck_assert(buffer_can_read(&b));
}
END_TEST

START_TEST (test_hello_zero_methods) {
    // NMETHODS = 0: mensaje completo, sin callbacks. Quien lo procese
    // termina respondiendo NO_ACCEPTABLE_METHODS.
    uint8_t data[] = { 0x05, 0x00 };

    struct hello_parser p;
    struct seen s;
    parser_setup(&p, &s);

    bool errored = false;
    ck_assert_uint_eq(hello_nmethods, hello_parser_feed(&p, data[0]));
    ck_assert_uint_eq(hello_done,     hello_parser_feed(&p, data[1]));
    ck_assert(hello_is_done(p.state, &errored));
    ck_assert(!errored);
    ck_assert_uint_eq(0, s.n);
}
END_TEST

START_TEST (test_hello_callback_once_per_method) {
    // 255 métodos (el máximo): el callback se llama exactamente 255 veces,
    // en orden, incluso con duplicados.
    struct hello_parser p;
    struct seen s;
    parser_setup(&p, &s);

    ck_assert_uint_eq(hello_nmethods, hello_parser_feed(&p, 0x05));
    ck_assert_uint_eq(hello_methods,  hello_parser_feed(&p, 0xFF));
    for(unsigned i = 0; i < 255; i++) {
        const enum hello_state st = hello_parser_feed(&p, (uint8_t) (i % 3));
        ck_assert_uint_eq(i + 1, s.n);
        ck_assert_uint_eq((uint8_t) (i % 3), s.methods[i]);
        if(i < 254) {
            ck_assert_uint_eq(hello_methods, st);
        } else {
            ck_assert_uint_eq(hello_done, st);
        }
    }
    ck_assert_uint_eq(255, s.n);
}
END_TEST

START_TEST (test_hello_marshall) {
    uint8_t raw[2];
    buffer b;
    buffer_init(&b, N(raw), raw);

    ck_assert_int_eq(2, hello_marshall(&b, SOCKS_HELLO_NOAUTHENTICATION_REQUIRED));
    size_t n = 0;
    const uint8_t *ptr = buffer_read_ptr(&b, &n);
    ck_assert_uint_eq(2, n);
    ck_assert_uint_eq(0x05, ptr[0]);
    ck_assert_uint_eq(0x00, ptr[1]);

    // sin espacio -> -1 y no toca el buffer
    ck_assert_int_eq(-1, hello_marshall(&b, SOCKS_HELLO_NO_ACCEPTABLE_METHODS));
    buffer_read_ptr(&b, &n);
    ck_assert_uint_eq(2, n);
}
END_TEST

Suite *
suite(void) {
    Suite *s  = suite_create("hello");
    TCase *tc = tcase_create("hello");

    tcase_add_test(tc, test_hello_valid_whole);
    tcase_add_test(tc, test_hello_valid_byte_by_byte);
    tcase_add_test(tc, test_hello_valid_fragmented_consume);
    tcase_add_test(tc, test_hello_bad_version);
    tcase_add_test(tc, test_hello_zero_methods);
    tcase_add_test(tc, test_hello_callback_once_per_method);
    tcase_add_test(tc, test_hello_marshall);
    suite_add_tcase(s, tc);

    return s;
}

int
main(void) {
    int number_failed;
    SRunner *sr = srunner_create(suite());

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
