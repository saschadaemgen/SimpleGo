/**
 * SimpleGo - smp_contacts.h
 * Contact management and NVS persistence
 */

#ifndef SMP_CONTACTS_H
#define SMP_CONTACTS_H

#include <stdbool.h>
#include "smp_types.h"
#include "mbedtls/ssl.h"

// ============== Global Declarations ==============

extern const uint8_t ED25519_SPKI_HEADER[12];
extern const uint8_t X25519_SPKI_HEADER[12];
extern contacts_db_t contacts_db;

// NVS operations
bool load_contacts_from_nvs(void);
bool save_contacts_to_nvs(void);
void clear_all_contacts(void);

/** Set true before calling add_contact from PSRAM-stack tasks.
 *  NVS write will be skipped; caller must save from Internal SRAM task. */
extern volatile bool contacts_nvs_deferred;

// Session 33: PSRAM allocation (call before any contact operations)
bool contacts_init_psram(void);

// Session 33: Save single contact (faster for runtime updates)
bool save_contact_single(int idx);

// Contact lookup
int find_contact_by_recipient_id(const uint8_t *recipient_id, uint8_t len);

// Contact display
void list_contacts(void);
void print_invitation_links(const uint8_t *ca_hash, const char *host, int port);

// Contact operations (require server connection)
int add_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                const uint8_t *session_id, const char *name);
bool remove_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                    const uint8_t *session_id, int index);

// Get invite link for UI
bool get_invite_link(const uint8_t *ca_hash, const char *host, int port, char *out_link, size_t out_len);

// Session 34: Get invite link for a specific contact slot
bool get_invite_link_for_slot(int slot, const uint8_t *ca_hash,
                               const char *host, int port,
                               char *out_link, size_t out_len);

// Subscribe to all contacts
void subscribe_all_contacts(mbedtls_ssl_context *ssl, uint8_t *block,
                            const uint8_t *session_id);

#endif // SMP_CONTACTS_H
