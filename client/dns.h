/* dns.c
 * By Ron Bowes
 * Created December, 2009
 *
 * (See LICENSE.md)
 *
 * This module implements a reasonably functional DNS library that can build or
 * parse DNS packets in a platform-agnostic way. It implements a number of
 * record types (A, NS, CNAME, MX, TXT, and AAAA), and can add and parse
 * questions or answers.
 *
 * On Windows, due to IPv6 parsing being unavailable on older systems, I
 * disable AAAA records entirely.
 *
 * In the future I may opt to add more record types and stronger parsing, but
 * for now this was enough.
 */

#ifndef __DNS_H__
#define __DNS_H__

#include "types.h"

/* Define a list of dns types. The initial '_' in all the names here is because Windows
 * defines some of these, and differently than how we want them on occasion. */
typedef enum
{
  /* RFC 1034/1035 */
  _DNS_TYPE_A          = 0x0001,
  _DNS_TYPE_NS         = 0x0002,
  _DNS_TYPE_MD         = 0x0003,
  _DNS_TYPE_MF         = 0x0004,
  _DNS_TYPE_CNAME      = 0x0005,
  _DNS_TYPE_SOA        = 0x0006,
  _DNS_TYPE_MB         = 0x0007,
  _DNS_TYPE_MG         = 0x0008,
  _DNS_TYPE_MR         = 0x0009,
  _DNS_TYPE_NULL       = 0x000a,
  _DNS_TYPE_WKS        = 0x000b,
  _DNS_TYPE_PTR        = 0x000c,
  _DNS_TYPE_HINFO      = 0x000d,
  _DNS_TYPE_MINFO      = 0x000e,
  _DNS_TYPE_MX         = 0x000f,
  _DNS_TYPE_TEXT       = 0x0010,

  /*  RFC 1183 */
  _DNS_TYPE_RP         = 0x0011,
  _DNS_TYPE_AFSDB      = 0x0012,
  _DNS_TYPE_X25        = 0x0013,
  _DNS_TYPE_ISDN       = 0x0014,
  _DNS_TYPE_RT         = 0x0015,

  /*  RFC 1348 */
  _DNS_TYPE_NSAP       = 0x0016,
  _DNS_TYPE_NSAPPTR    = 0x0017,

  /*  RFC 2065 (DNS security) */
  _DNS_TYPE_SIG        = 0x0018,
  _DNS_TYPE_KEY        = 0x0019,

  /*  RFC 1664 (X.400 mail) */
  _DNS_TYPE_PX         = 0x001a,

  /*  RFC 1712 (Geographic position) */
  _DNS_TYPE_GPOS       = 0x001b,

  /*  RFC 1886 (IPv6 Address) */
  _DNS_TYPE_AAAA       = 0x001c,

  /*  RFC 1876 (Geographic location) */
  _DNS_TYPE_LOC        = 0x001d,

  /*  RFC 2065 (Secure negative response) */
  _DNS_TYPE_NXT        = 0x001e,

  /*  Patton (Endpoint Identifier) */
  _DNS_TYPE_EID        = 0x001f,

  /*  Patton (Nimrod Locator) */
/*  _DNS_TYPE_NIMLOC     = 0x0020,*/
  /*  RFC 2052 (Service location) */
/*  _DNS_TYPE_SRV        = 0x0021,*/

  /* NetBIOS. */
  _DNS_TYPE_NB         = 0x0020,

  /* Adapter status. */
  _DNS_TYPE_NBSTAT     = 0x0021,

  /*  ATM Address */
  _DNS_TYPE_ATMA       = 0x0022,

  /*  RFC 2168 (Naming Authority Pointer) */
  _DNS_TYPE_NAPTR      = 0x0023,

  /*  RFC 2230 (Key Exchanger) */
  _DNS_TYPE_KX         = 0x0024,

  /*  RFC 2538 (CERT) */
  _DNS_TYPE_CERT       = 0x0025,

  /*  A6 Draft (A6) */
  _DNS_TYPE_A6         = 0x0026,

  /*  DNAME Draft (DNAME) */
  _DNS_TYPE_DNAME      = 0x0027,

  /*  Eastlake (Kitchen Sink) */
  _DNS_TYPE_SINK       = 0x0028,

  /*  RFC 2671 (EDNS OPT) */
  _DNS_TYPE_OPT        = 0x0029,

  /*  IANA Reserved */

  _DNS_TYPE_UINFO      = 0x0064,
  _DNS_TYPE_UID        = 0x0065,
  _DNS_TYPE_GID        = 0x0066,
  _DNS_TYPE_UNSPEC     = 0x0067,

  /*  Query only types (1035, 1995) */
  _DNS_TYPE_ADDRS      = 0x00f8,
  _DNS_TYPE_TKEY       = 0x00f9,
  _DNS_TYPE_TSIG       = 0x00fa,
  _DNS_TYPE_IXFR       = 0x00fb,
  _DNS_TYPE_AXFR       = 0x00fc,
  _DNS_TYPE_MAILB      = 0x00fd,
  _DNS_TYPE_MAILA      = 0x00fe,
  _DNS_TYPE_ALL        = 0x00ff,
  _DNS_TYPE_ANY        = 0x00ff,
} dns_type_t;

typedef enum
{
  _DNS_CLASS_IN = 1, /* The Internet */
  _DNS_CLASS_CS = 2, /* The CSNET class (Obsolete - used only for examples in some obsolete RFCs) */
  _DNS_CLASS_CH = 3, /* The CHAOS class */
  _DNS_CLASS_HS = 4, /* Hesiod [Dyer 87] */
} dns_class_t;

/* Here are how the opcodes, flags, and rcodes are laid out:
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                               1  1  1  1  1  1|
 * | 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5|
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
typedef enum
{
  _DNS_OPCODE_QUERY  = 0x0000,
  _DNS_OPCODE_IQUERY = 0x0800,
  _DNS_OPCODE_STATUS = 0x1000,
} dns_opcode_t;

typedef enum
{
  _DNS_FLAG_QR = 0x8000, /* Query response */
  _DNS_FLAG_AA = 0x0400, /* Authoritative answer */
  _DNS_FLAG_TC = 0x0200, /* Truncation */
  _DNS_FLAG_RD = 0x0100, /* Recursion desired */
  _DNS_FLAG_RA = 0x0080, /* Recursion available */
} dns_flag_t;

typedef enum
{
  _DNS_RCODE_SUCCESS         = 0x0000,
  _DNS_RCODE_FORMAT_ERROR    = 0x0001,
  _DNS_RCODE_SERVER_FAILURE  = 0x0002,
  _DNS_RCODE_NAME_ERROR      = 0x0003,
  _DNS_RCODE_NOT_IMPLEMENTED = 0x0004,
  _DNS_RCODE_REFUSED         = 0x0005,
} dns_rcode_t;


/* A DNS question. Questions are simple, they just have a name, type, and class. */
typedef struct
{
  char *name;
  dns_type_t  type;
  dns_class_t class;
} question_t;

/* An answer for an A packet. */
typedef struct
{
  char *address;
  uint8_t bytes[4];
} A_answer_t;

/* Only define AAAA on Linux. */
#ifndef WIN32
typedef struct
{
  char *address;
  uint8_t bytes[16];
} AAAA_answer_t;
#endif

/* Nameserver and CNAME (alias) records simply have a name. */
typedef struct
{
  char *name;
} NS_answer_t, CNAME_answer_t;

/* Mail server requests (MX) have a name and a preference number. */
typedef struct
{
  uint16_t preference;
  char *name;
} MX_answer_t;

/* A text record (TXT) has the text data and a length. Unlike MX, NS, and CNAME, text
 * records aren't encoded as a dns name. */
typedef struct
{
  uint8_t *text;
  uint8_t  length;
} TEXT_answer_t;

/* A NetBIOS answer (NB) is used by Windows on port 137. */
typedef struct
{
  uint16_t  flags;
  char     *address;
} NB_answer_t;

/* One element returned by a NBSTAT query (NBSTAT answers typically contain multiple names). */
typedef struct
{
  char    *name;
  uint8_t  name_type;
  uint16_t name_flags;
} NBSTAT_name_t;

/* A NetBIOS Status answer (NBSTAT or NBTSTAT) is used by Windows to get more information over 137. */
typedef struct
{
  uint8_t        name_count;
  NBSTAT_name_t *names;
  uint8_t        stats[64];
} NBSTAT_answer_t;

/* Let us refer to any kind of answer type together. */
typedef union
{
  A_answer_t      A;
  NS_answer_t     NS;
  CNAME_answer_t  CNAME;
  MX_answer_t     MX;
  TEXT_answer_t   TEXT;
#ifndef WIN32
  AAAA_answer_t   AAAA;
#endif
  NB_answer_t     NB;
  NBSTAT_answer_t NBSTAT;
} answer_types_t;

/* And finally, define a DNS answer. */
typedef struct
{
  char           *question;
  dns_type_t      type;
  dns_class_t     class;
  uint32_t        ttl;
  answer_types_t *answer;
} answer_t;

/* We don't implement authority records. */
typedef struct
{
  /* TODO */
  int dummy;
} authority_t;

/* An additional for an A packet. */
typedef struct
{
  char *address;
} A_additional_t;

/* Only define AAAA on Linux. */
#ifndef WIN32
typedef A_additional_t AAAA_additional_t;
#endif

/* Nameserver and CNAME (alias) records simply have a name. */
typedef struct
{
  char *name;
} NS_additional_t, CNAME_additional_t;

/* Mail server requests (MX) have a name and a preference number. */
typedef struct
{
  uint16_t preference;
  char *name;
} MX_additional_t;

/* A text record (TXT) has the text data and a length. Unlike MX, NS, and CNAME, text
 * records aren't encoded as a dns name. */
typedef struct
{
  uint8_t *text;
  uint8_t  length;
} TEXT_additional_t;

/* A NetBIOS additional (NB) is used by Windows on port 137. */
typedef struct
{
  uint16_t  flags;
  char     *address;
} NB_additional_t;

/* A NetBIOS Status additional (NBSTAT or NBTSTAT) is used by Windows to get more information over 137. */
typedef struct
{
  uint8_t        name_count;
  NBSTAT_name_t *names;
  uint8_t        stats[64];
} NBSTAT_additional_t;

/* Let us refer to any kind of additional type together. */
typedef union
{
  A_additional_t      A;
  NS_additional_t     NS;
  CNAME_additional_t  CNAME;
  MX_additional_t     MX;
  TEXT_additional_t   TEXT;
#ifndef WIN32
  AAAA_additional_t   AAAA;
#endif
  NB_additional_t     NB;
  NBSTAT_additional_t NBSTAT;
} additional_types_t;

/* And finally, define a DNS additional. */
typedef struct
{
  char           *question;
  dns_type_t      type;
  dns_class_t     class;
  uint32_t        ttl;
  additional_types_t *additional;
} additional_t;

/* Define an entire DNS packet. */
typedef struct
{
  uint16_t     trn_id;
  dns_opcode_t opcode;
  dns_flag_t   flags;
  dns_rcode_t  rcode;

  uint16_t question_count;
  uint16_t answer_count;
  uint16_t authority_count;
  uint16_t additional_count;

  question_t   *questions;
  answer_t     *answers;
  authority_t  *authorities;
  additional_t *additionals;
} dns_t;

/* Allocate memory for a blank dns structure. Should be freed with dns_free(). */
dns_t   *dns_create(dns_opcode_t opcode, dns_flag_t flags, dns_rcode_t rcode);

/* Take a DNS packet as a stream of bytes, and create a dns_t structure from it.
 * Should also be cleaned up with dns_destroy(). */
dns_t   *dns_create_from_packet(uint8_t *packet, size_t length);

/* De-allocate memory and resources from a dns object. */
void     dns_destroy(dns_t *dns);

/* Add a question to the DNS packet. A DNS packet can have any number of questions, but
 * I normally limit it to one at a time. */
void     dns_add_question(dns_t *dns, char *name, dns_type_t type, dns_class_t class);

/* Add a NetBIOS question to the DNS packet. This is similar to a normal question but
 * with a couple extra fields and an encoded name. */
void     dns_add_netbios_question(dns_t *dns, char *name, uint8_t name_type, char *scope, dns_type_t type, dns_class_t class);

/* These functions add answers of the various types. */
void     dns_add_answer_A(dns_t *dns,     char *question, dns_class_t class, uint32_t ttl, char *address);
void     dns_add_answer_NS(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, char *name);
void     dns_add_answer_CNAME(dns_t *dns, char *question, dns_class_t class, uint32_t ttl, char *name);
void     dns_add_answer_MX(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, uint16_t preference, char *name);
void     dns_add_answer_TEXT(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, uint8_t *text, uint8_t length);
#ifndef WIN32
void     dns_add_answer_AAAA(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, char *address);
#endif
void     dns_add_answer_NB(dns_t *dns,  char *question, uint8_t question_type, char *scope, dns_class_t class, uint32_t ttl, uint16_t flags, char *address);

/* These functions add additionals of the various types. */
void     dns_add_additional_A(dns_t *dns,     char *question, dns_class_t class, uint32_t ttl, char *address);
void     dns_add_additional_NS(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, char *name);
void     dns_add_additional_CNAME(dns_t *dns, char *question, dns_class_t class, uint32_t ttl, char *name);
void     dns_add_additional_MX(dns_t *dns,    char *question, dns_class_t class, uint32_t ttl, uint16_t preference, char *name);
void     dns_add_additional_TEXT(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, uint8_t *text, uint8_t length);
#ifndef WIN32
void     dns_add_additional_AAAA(dns_t *dns,  char *question, dns_class_t class, uint32_t ttl, char *address);
#endif
void     dns_add_additional_NB(dns_t *dns,  char *question, uint8_t question_type, char *scope, dns_class_t class, uint32_t ttl, uint16_t flags, char *address);

/* Convert a DNS request into a packet that can be sent on port 53. Memory has to be freed. */
uint8_t *dns_to_packet(dns_t *dns, size_t *length);

/* Print the DNS request. Useful for debugging. */
void     dns_print(dns_t *dns);

/* Create a DNS error object. */
dns_t   *dns_create_error(uint16_t trn_id, question_t question);
/* Create a DNS error packet, ready to send. */
uint8_t *dns_create_error_string(uint16_t trn_id, question_t question, size_t *length);

/* Get the first system DNS server. Works on Windows and any system that uses /etc/resolv.conf. */
char *dns_get_system();

/* Runs dnstest and exits. Useful for --test parameters on any of the dns* programs. */
void dns_do_test(char *domain);

int      dns_is_error(dns_t *dns);

#endif

