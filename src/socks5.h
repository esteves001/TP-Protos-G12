#ifndef SOCKS5_H_2vY4rQ7mNcK9pL3xT8wE1aZ6bD0fG5h
#define SOCKS5_H_2vY4rQ7mNcK9pL3xT8wE1aZ6bD0fG5h

/**
 * socks5.h - constantes del protocolo SOCKS v5 (RFC 1928) y de la
 *            autenticación usuario/contraseña (RFC 1929).
 *
 * Solo constantes: acá no va lógica.
 */

/** versión del protocolo (RFC 1928 §3) */
enum socks5_version {
    SOCKS5_VERSION = 0x05,
};

/** métodos de autenticación negociados en el hello (RFC 1928 §3) */
enum socks5_auth_method {
    SOCKS_HELLO_NOAUTHENTICATION_REQUIRED = 0x00,
    SOCKS_HELLO_GSSAPI                    = 0x01,
    SOCKS_HELLO_USERPASS                  = 0x02,
    SOCKS_HELLO_NO_ACCEPTABLE_METHODS     = 0xFF,
};

/** comandos del request (RFC 1928 §4) */
enum socks5_cmd {
    SOCKS5_CMD_CONNECT       = 0x01,
    SOCKS5_CMD_BIND          = 0x02,
    SOCKS5_CMD_UDP_ASSOCIATE = 0x03,
};

/** tipos de dirección (RFC 1928 §4, §5) */
enum socks5_atyp {
    SOCKS5_ATYP_IPV4       = 0x01,
    SOCKS5_ATYP_DOMAINNAME = 0x03,
    SOCKS5_ATYP_IPV6       = 0x04,
};

/** byte reservado del request/reply (RFC 1928 §4, §6) */
enum socks5_rsv {
    SOCKS5_RSV = 0x00,
};

/** códigos de respuesta REP (RFC 1928 §6) */
enum socks5_rep {
    SOCKS5_REP_SUCCEEDED               = 0x00,
    SOCKS5_REP_GENERAL_FAILURE         = 0x01,
    SOCKS5_REP_NOT_ALLOWED_BY_RULESET  = 0x02,
    SOCKS5_REP_NETWORK_UNREACHABLE     = 0x03,
    SOCKS5_REP_HOST_UNREACHABLE        = 0x04,
    SOCKS5_REP_CONNECTION_REFUSED      = 0x05,
    SOCKS5_REP_TTL_EXPIRED             = 0x06,
    SOCKS5_REP_COMMAND_NOT_SUPPORTED   = 0x07,
    SOCKS5_REP_ATYP_NOT_SUPPORTED      = 0x08,
};

/** subnegociación usuario/contraseña (RFC 1929) */
enum socks5_userpass {
    SOCKS5_USERPASS_VERSION = 0x01,
    SOCKS5_USERPASS_SUCCESS = 0x00,
    SOCKS5_USERPASS_FAILURE = 0x01,
};

#endif
