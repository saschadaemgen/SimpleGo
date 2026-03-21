/**
 * SimpleGo - main.c
 * Application entry point, TLS handshake, and UI poll timer
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>          // Session 47: PRId64 for timing
#include "esp_sntp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"        // Session 47: Timing analysis
#include "nvs_flash.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"

#include "sodium.h"

// SimpleGo modules
#include "smp_types.h"
#include "smp_utils.h"
#include "smp_crypto.h"
#include "smp_network.h"
#include "smp_ratchet.h"
#include "smp_contacts.h"
#include "smp_parser.h"
#include "smp_peer.h"
#include "smp_ack.h"
#include "smp_e2e.h"
#include "reply_queue.h"   // Session 34 Phase 6: per-contact reply queues
#include "smp_agent.h"
#include "wifi_manager.h"
#include "smp_storage.h"
#include "smp_tasks.h"
#include "smp_history.h"   // Session 37: history_message_t, HISTORY_DIR_SENT
#include "sntrup761_test.h" // Session 46: Post-quantum KEM verification
#include "pq_crypto_task.h" // Session 46 Teil E: PQ crypto task init
#include "smp_ratchet.h"    // Session 46: smp_settings_get_pq_enabled()

#include "smp_x448.h"
#include "smp_queue.h"
#include "smp_handshake.h"  // Auftrag 50d: handshake_load_state
#include "simplex_crypto.h"
#include "smp_servers.h"    // Session 49: Multi-server management

// T-Deck Display Driver
#include "tdeck_display.h"
#include "tdeck_backlight.h"
#include "tdeck_lvgl.h"
#include "tdeck_touch.h"
#include "tdeck_keyboard.h"  // Auftrag 50d: T-Deck keyboard

// UI System
#include "ui_manager.h"
#include "ui_splash.h"
#include "ui/screens/ui_connect.h"
#include "ui_chat.h"
#include "ui_contacts.h"       // Auftrag 2a: live refresh on incoming message
#include "ui_main.h"           // Auftrag 2a: live refresh on incoming message
#include "ui_settings.h"
#include "ui_theme.h"
#include "simplego_fonts.h"

static const char *TAG = "SMP";

// NTP sync success callback (non-blocking, fires when time is available)
static void ntp_sync_callback(struct timeval *tv)
{
    (void)tv;
    time_t now = time(NULL);
    struct tm ti;
    gmtime_r(&now, &ti);
    ESP_LOGI(TAG, "NTP synced! %04d-%02d-%02d %02d:%02d:%02d UTC",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             ti.tm_hour, ti.tm_min, ti.tm_sec);
}

// Session 37c: RAM copies of Montserrat with German umlaut fallback
lv_font_t simplego_font_14;
lv_font_t simplego_font_12;
lv_font_t simplego_font_10;

// Auftrag 50b: Session restoration flag (set in app_main, read in smp_connect)
static bool session_restored = false;

// Keyboard -> SMP thread communication via FreeRTOS Queue
static QueueHandle_t kbd_msg_queue = NULL;   // char[256] messages from kbd task

// Chat send callback (LVGL thread -> kbd_msg_queue -> smp_app_run)

static void chat_send_cb(const char *text)
{
    if (kbd_msg_queue && text && text[0] != '\0') {
        char buf[256];
        strncpy(buf, text, 255);
        buf[255] = '\0';
        xQueueSend(kbd_msg_queue, buf, 0);
        ESP_LOGI("CHAT_CB", "Send queued: \"%s\"", buf);
    }
}

// Session 32: LVGL timer polls app_to_ui_queue (runs in LVGL thread context)
// Session 33 Phase 4A: Pending QR code (thread-safe handoff from smp_connect)
static char s_pending_qr_link[1500] = {0};
static volatile bool s_show_qr_pending = false;

// Session 39k: Auto-launch WiFi setup when no credentials exist
static volatile bool s_wifi_setup_pending = false;

// Session 37b: Progressive history rendering - 3 messages per timer tick
#define HISTORY_CHUNK_SIZE  3   // messages per 50ms tick - smooth UX
// Session 40c: MAX_VISIBLE_BUBBLES removed - replaced by BUBBLE_WINDOW_SIZE in ui_chat.c
static int  s_hist_render_idx   = 0;     // current position in batch
static int  s_hist_render_end   = 0;     // Session 40c: stop index (window end)
static bool s_hist_rendering    = false; // true while progressively rendering

static void ui_poll_timer_cb(lv_timer_t *t)
{
    (void)t;

    // Session 47 2d: Check connection timeouts every ~5s (100 ticks x 50ms)
    {
        static int s_timeout_counter = 0;
        if (++s_timeout_counter >= 100) {
            s_timeout_counter = 0;
            ui_contacts_check_connect_timeouts();
        }
    }

    // Session 37b: Progressive history rendering - 3 messages per tick
    // Session 40c: Renders only the window portion [window_start..window_end)
    if (s_hist_rendering && smp_history_batch && smp_history_batch_count > 0) {
        int end = s_hist_render_idx + HISTORY_CHUNK_SIZE;
        if (end > s_hist_render_end) end = s_hist_render_end;

        for (int h = s_hist_render_idx; h < end; h++) {
            history_message_t *m = &smp_history_batch[h];
            ui_chat_add_history_message(
                m->text,
                m->direction == HISTORY_DIR_SENT,
                smp_history_batch_slot,
                m->timestamp,
                m->delivery_status);
        }

        s_hist_render_idx = end;

        // All done?
        if (s_hist_render_idx >= s_hist_render_end) {
            // Scroll to bottom after final chunk
            ui_chat_scroll_to_bottom();

            // Session 40c: Clear setup guard so scroll-cb can fire
            ui_chat_window_render_done();

            // Session 40c: Log actual rendered count, not total batch size
            int rendered = s_hist_render_end - ui_chat_get_window_start();
            ESP_LOGI("UI", "History: render complete (%d bubbles, window [%d..%d))",
                     rendered, ui_chat_get_window_start(), s_hist_render_end);

            // Clear batch state (buffer stays allocated for reuse)
            // Cache in ui_chat.c retains data for scroll-up access
            smp_history_batch_count = 0;
            smp_history_batch_slot = -1;
            s_hist_rendering = false;
        }
    }

    // Session 33 Phase 4A: Show QR code from smp_connect (thread-safe)
    if (s_show_qr_pending) {
        s_show_qr_pending = false;
        ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE);
        ui_connect_set_invite_link(s_pending_qr_link);
        ESP_LOGI("UI", "QR code displayed from pending buffer");
    }

    // Session 39k: Auto-launch WiFi Settings on first boot (no credentials)
    if (s_wifi_setup_pending) {
        s_wifi_setup_pending = false;
        ui_manager_show_screen(UI_SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_NONE);
        ui_settings_show_wifi_tab();
        ESP_LOGI("UI", "Auto-launched WiFi setup (no credentials)");
    }

    extern QueueHandle_t app_to_ui_queue;
    if (!app_to_ui_queue) return;

    ui_event_t evt;
    while (xQueueReceive(app_to_ui_queue, &evt, 0) == pdTRUE) {
        switch (evt.type) {
            case UI_EVT_MESSAGE:
                // Auftrag 2a: No auto-navigate. SD history is written in
                // smp_agent.c before this event. PSRAM cache updates if
                // chat screen is open (guard in ui_chat_add_message).
                ui_chat_add_message(evt.text, evt.is_outgoing, evt.contact_idx);
                // Live-refresh unread counters on whichever screen is visible
                if (ui_manager_get_current() == UI_SCREEN_CONTACTS) {
                    ui_contacts_refresh();
                } else if (ui_manager_get_current() == UI_SCREEN_MAIN) {
                    ui_main_refresh();
                }
                break;
            case UI_EVT_NAVIGATE:
                ui_manager_show_screen((ui_screen_t)evt.screen, LV_SCR_LOAD_ANIM_NONE);
                break;
            case UI_EVT_SET_CONTACT:
                ui_chat_set_contact(evt.text);
                break;
            case UI_EVT_DELIVERY_STATUS:
                /* Only update bubble status if chat screen is active.
                 * Receipt events can arrive after navigating away from chat.
                 * Stale events are harmless - next chat open loads from history. */
                if (ui_manager_get_current() == UI_SCREEN_CHAT) {
                    ui_chat_update_status(evt.msg_seq, (int)evt.status);
                }
                break;
            case UI_EVT_SHOW_QR:
                // Session 33 Phase 4A: Show QR code on connect screen
                ui_manager_show_screen(UI_SCREEN_CONNECT, LV_SCR_LOAD_ANIM_NONE);
                ui_connect_set_invite_link(evt.text);
                break;
            case UI_EVT_UPDATE_STATUS:
                // Session 33 Phase 4A: Update status text
                ui_connect_set_status(evt.text);
                // Session 47 2d: Also update contact row status during handshake
                ui_contacts_update_connecting_status(evt.text);
                break;
            case UI_EVT_SWITCH_CONTACT:
                // 35e: Switch chat view to different contact
                ui_chat_switch_contact(evt.contact_idx, evt.text);
                break;

            case UI_EVT_HISTORY_BATCH:
                // Session 40c: Cache batch in PSRAM and render sliding window.
                // cache_history() clears all bubbles internally before setting
                // window state, and sets a guard to block scroll-cb during setup.
                if (smp_history_batch && smp_history_batch_count > 0
                    && smp_history_batch_slot == evt.contact_idx) {

                    // 40c: Cache + clear + set window (atomic, guard ON)
                    ui_chat_cache_history(smp_history_batch,
                                          smp_history_batch_count,
                                          evt.contact_idx);

                    // 40c: Progressive render starts at window_start, ends at window_end.
                    // IMPORTANT: Window indices are cache indices. Since HISTORY_MAX_LOAD (20)
                    // <= MSG_CACHE_SIZE (30), batch and cache indices are always identical.
                    // If HISTORY_MAX_LOAD ever exceeds MSG_CACHE_SIZE, add batch_offset here.
                    s_hist_render_idx = ui_chat_get_window_start();
                    s_hist_render_end = ui_chat_get_window_end();
                    s_hist_rendering = true;

                    ESP_LOGI("UI", "History: starting render, %d msgs cached, window [%d..%d)",
                             smp_history_batch_count,
                             s_hist_render_idx, s_hist_render_end);
                } else {
                    // Empty history (new contact) - just hide loading
                    ui_chat_hide_loading();
                }
                break;

            // Session 47 2d: Connection progress events
            case UI_EVT_CONNECT_SCANNED: {
                int64_t t2 = esp_timer_get_time();
                ESP_LOGI("TIMING", "[2] CONNECT_SCANNED received in UI timer at %" PRId64 " us", t2);

                ui_contacts_set_connect_status(evt.contact_idx, CONN_ST_SCANNED, "SCANNED");

                int64_t t3 = esp_timer_get_time();
                ESP_LOGI("TIMING", "[3] show_screen(CONTACTS) called at %" PRId64 " us", t3);
                ui_manager_show_screen(UI_SCREEN_CONTACTS, LV_SCR_LOAD_ANIM_NONE);
                // Remove QR screen from nav stack so "back" doesn't return to it
                ui_manager_remove_from_nav_stack(UI_SCREEN_CONNECT);

                int64_t t3b = esp_timer_get_time();
                ESP_LOGI("TIMING", "[3b] show_screen done at %" PRId64 " us (delta from [2]: %" PRId64 " ms)",
                         t3b, (t3b - t2) / 1000);
                break;
            }
            case UI_EVT_CONNECT_NAME:
                ESP_LOGI("UI", "CONNECT_NAME: contact [%d] name='%s'", evt.contact_idx, evt.text);
                ui_contacts_set_connect_status(evt.contact_idx, CONN_ST_NAME, "EXCHANGING KEYS");
                if (ui_manager_get_current() == UI_SCREEN_CONTACTS) {
                    ui_contacts_refresh();
                }
                break;
            case UI_EVT_CONNECT_DONE:
                ESP_LOGI("UI", "CONNECT_DONE: contact [%d] - handshake complete", evt.contact_idx);
                ui_contacts_clear_connect_status(evt.contact_idx);
                if (ui_manager_get_current() == UI_SCREEN_CONTACTS) {
                    ui_contacts_refresh();
                }
                break;
        }
    }
}

// ============== CONFIG ==============
// Session 49: SMP_HOST/SMP_PORT removed. Server chosen via
// smp_servers_get_active() from NVS-persisted selection.
static smp_server_t *s_active_server = NULL;

// ============== Main SMP Connection ==============

// #define TEST_DUAL_TLS   /* Session 49: Test done. Result: ~1500B per conn, 6351B remaining with 3 conns */

#ifdef TEST_DUAL_TLS

static void test_dual_tls(void)
{
    static const char *T = "DUAL-TLS";
    int ret;

    ESP_LOGI(T, "");
    ESP_LOGI(T, "=== Dual TLS Connection Test ===");

    /* Baseline: one TLS connection already active from smp_connect() */
    uint32_t heap_baseline = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(T, "SRAM baseline (after smp_connect): %" PRIu32 " bytes free", heap_baseline);

    /* Connection #1: smp.simplego.dev */
    ESP_LOGI(T, "--- Connection #1: smp.simplego.dev ---");

    typedef struct {
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config conf;
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context ctr_drbg;
        int sock;
        uint8_t session_id[32];
    } test_tls_ctx_t;

    test_tls_ctx_t *ctx1 = heap_caps_calloc(1, sizeof(test_tls_ctx_t), MALLOC_CAP_INTERNAL);
    if (!ctx1) {
        ESP_LOGE(T, "Failed to allocate ctx1 in SRAM!");
        return;
    }

    mbedtls_ssl_init(&ctx1->ssl);
    mbedtls_ssl_config_init(&ctx1->conf);
    mbedtls_entropy_init(&ctx1->entropy);
    mbedtls_ctr_drbg_init(&ctx1->ctr_drbg);
    ctx1->sock = -1;

    ret = mbedtls_ctr_drbg_seed(&ctx1->ctr_drbg, mbedtls_entropy_func,
                                 &ctx1->entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(T, "ctx1 DRBG seed failed: -0x%04x", -ret);
        goto cleanup1;
    }

    ESP_LOGI(T, "Connecting to smp.simplego.dev:5223...");
    ctx1->sock = smp_tcp_connect("smp.simplego.dev", 5223);
    if (ctx1->sock < 0) {
        ESP_LOGE(T, "ctx1 TCP connect failed");
        goto cleanup1;
    }

    ret = mbedtls_ssl_config_defaults(&ctx1->conf, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) { ESP_LOGE(T, "ctx1 config failed"); goto cleanup1; }

    mbedtls_ssl_conf_min_tls_version(&ctx1->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&ctx1->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&ctx1->conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&ctx1->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&ctx1->conf, mbedtls_ctr_drbg_random, &ctx1->ctr_drbg);

    {
        static const char *alpn_test[] = {"smp/1", NULL};
        mbedtls_ssl_conf_alpn_protocols(&ctx1->conf, alpn_test);
    }

    ret = mbedtls_ssl_setup(&ctx1->ssl, &ctx1->conf);
    if (ret != 0) { ESP_LOGE(T, "ctx1 ssl_setup failed"); goto cleanup1; }

    mbedtls_ssl_set_hostname(&ctx1->ssl, "smp.simplego.dev");
    mbedtls_ssl_set_bio(&ctx1->ssl, &ctx1->sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&ctx1->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(T, "ctx1 TLS handshake failed: -0x%04x", -ret);
            goto cleanup1;
        }
    }
    ESP_LOGI(T, "ctx1 TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ctx1->ssl));

    /* Read SMP ServerHello on ctx1 */
    {
        uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
        if (block) {
            int content_len = smp_read_block(&ctx1->ssl, block, 15000);
            if (content_len >= 37) {
                uint8_t *hello = block + 2;
                if (hello[4] == 32) {
                    memcpy(ctx1->session_id, &hello[5], 32);
                    ESP_LOGI(T, "ctx1 SMP handshake [OK] (sessId=%02x%02x...)",
                             ctx1->session_id[0], ctx1->session_id[1]);
                }
            } else {
                ESP_LOGW(T, "ctx1 SMP: no ServerHello (len=%d)", content_len);
            }
            heap_caps_free(block);
        }
    }

    uint32_t heap_after_ctx1 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(T, "SRAM after ctx1: %" PRIu32 " bytes free (delta: -%" PRIu32 ")",
             heap_after_ctx1, heap_baseline - heap_after_ctx1);

    /* Connection #2: smp8.simplex.im */
    ESP_LOGI(T, "--- Connection #2: smp8.simplex.im ---");

    test_tls_ctx_t *ctx2 = heap_caps_calloc(1, sizeof(test_tls_ctx_t), MALLOC_CAP_INTERNAL);
    if (!ctx2) {
        ESP_LOGE(T, "Failed to allocate ctx2 in SRAM!");
        goto cleanup1;
    }

    mbedtls_ssl_init(&ctx2->ssl);
    mbedtls_ssl_config_init(&ctx2->conf);
    mbedtls_entropy_init(&ctx2->entropy);
    mbedtls_ctr_drbg_init(&ctx2->ctr_drbg);
    ctx2->sock = -1;

    ret = mbedtls_ctr_drbg_seed(&ctx2->ctr_drbg, mbedtls_entropy_func,
                                 &ctx2->entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(T, "ctx2 DRBG seed failed: -0x%04x", -ret);
        goto cleanup2;
    }

    ESP_LOGI(T, "Connecting to smp8.simplex.im:5223...");
    ctx2->sock = smp_tcp_connect("smp8.simplex.im", 5223);
    if (ctx2->sock < 0) {
        ESP_LOGE(T, "ctx2 TCP connect failed");
        goto cleanup2;
    }

    ret = mbedtls_ssl_config_defaults(&ctx2->conf, MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) { ESP_LOGE(T, "ctx2 config failed"); goto cleanup2; }

    mbedtls_ssl_conf_min_tls_version(&ctx2->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&ctx2->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&ctx2->conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&ctx2->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&ctx2->conf, mbedtls_ctr_drbg_random, &ctx2->ctr_drbg);

    {
        static const char *alpn_test2[] = {"smp/1", NULL};
        mbedtls_ssl_conf_alpn_protocols(&ctx2->conf, alpn_test2);
    }

    ret = mbedtls_ssl_setup(&ctx2->ssl, &ctx2->conf);
    if (ret != 0) { ESP_LOGE(T, "ctx2 ssl_setup failed"); goto cleanup2; }

    mbedtls_ssl_set_hostname(&ctx2->ssl, "smp8.simplex.im");
    mbedtls_ssl_set_bio(&ctx2->ssl, &ctx2->sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&ctx2->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(T, "ctx2 TLS handshake failed: -0x%04x", -ret);
            goto cleanup2;
        }
    }
    ESP_LOGI(T, "ctx2 TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ctx2->ssl));

    /* Read SMP ServerHello on ctx2 */
    {
        uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_SPIRAM);
        if (block) {
            int content_len = smp_read_block(&ctx2->ssl, block, 15000);
            if (content_len >= 37) {
                uint8_t *hello = block + 2;
                if (hello[4] == 32) {
                    memcpy(ctx2->session_id, &hello[5], 32);
                    ESP_LOGI(T, "ctx2 SMP handshake [OK] (sessId=%02x%02x...)",
                             ctx2->session_id[0], ctx2->session_id[1]);
                }
            } else {
                ESP_LOGW(T, "ctx2 SMP: no ServerHello (len=%d)", content_len);
            }
            heap_caps_free(block);
        }
    }

    uint32_t heap_after_ctx2 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(T, "SRAM after ctx2: %" PRIu32 " bytes free (delta: -%" PRIu32 ")",
             heap_after_ctx2, heap_after_ctx1 - heap_after_ctx2);

    /* Stability check: hold both connections for 30 seconds */
    ESP_LOGI(T, "Holding both connections for 30s...");
    for (int i = 0; i < 30; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (i % 10 == 9) {
            uint32_t heap_now = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            ESP_LOGI(T, "  %ds - SRAM: %" PRIu32 " bytes free", i + 1, heap_now);
        }
    }

    /* Summary */
    ESP_LOGI(T, "");
    ESP_LOGI(T, "=== Dual TLS Test Results ===");
    ESP_LOGI(T, "SRAM baseline (1 conn active): %" PRIu32, heap_baseline);
    ESP_LOGI(T, "SRAM after +ctx1:              %" PRIu32 " (-%" PRIu32 ")",
             heap_after_ctx1, heap_baseline - heap_after_ctx1);
    ESP_LOGI(T, "SRAM after +ctx2:              %" PRIu32 " (-%" PRIu32 ")",
             heap_after_ctx2, heap_after_ctx1 - heap_after_ctx2);
    ESP_LOGI(T, "Total SRAM for 2 extra conns:  %" PRIu32 " bytes",
             heap_baseline - heap_after_ctx2);
    ESP_LOGI(T, "SRAM remaining with 3 total:   %" PRIu32 " bytes", heap_after_ctx2);
    ESP_LOGI(T, "=== Test Complete ===");
    ESP_LOGI(T, "");

cleanup2:
    if (ctx2) {
        mbedtls_ssl_close_notify(&ctx2->ssl);
        mbedtls_ssl_free(&ctx2->ssl);
        mbedtls_ssl_config_free(&ctx2->conf);
        mbedtls_ctr_drbg_free(&ctx2->ctr_drbg);
        mbedtls_entropy_free(&ctx2->entropy);
        if (ctx2->sock >= 0) close(ctx2->sock);
        heap_caps_free(ctx2);
    }

cleanup1:
    if (ctx1) {
        mbedtls_ssl_close_notify(&ctx1->ssl);
        mbedtls_ssl_free(&ctx1->ssl);
        mbedtls_ssl_config_free(&ctx1->conf);
        mbedtls_ctr_drbg_free(&ctx1->ctr_drbg);
        mbedtls_entropy_free(&ctx1->entropy);
        if (ctx1->sock >= 0) close(ctx1->sock);
        heap_caps_free(ctx1);
    }

    uint32_t heap_final = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(T, "SRAM after cleanup: %" PRIu32 " bytes free (leaked: %" PRIu32 ")",
             heap_final, heap_baseline - heap_final);
}

#endif /* TEST_DUAL_TLS */

static void smp_connect(void) {
    int ret;
    int sock = -1;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    uint8_t session_id[32];
    uint8_t ca_hash[32];

    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "Failed to allocate buffer!");
        return;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  SimpleGo v0.1.17-alpha Connection!    |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "");

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) goto cleanup;

    // ========== Step 1: TCP + TLS ==========
    ui_splash_set_status("Connecting to server...");
    ui_splash_set_progress(75);
    ESP_LOGI(TAG, "[1/5] Connecting to %s:%d...", s_active_server->host, s_active_server->port);
    sock = smp_tcp_connect(s_active_server->host, s_active_server->port);
    if (sock < 0) goto cleanup;

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto cleanup;

    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&conf, alpn_list);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) goto cleanup;

    mbedtls_ssl_set_hostname(&ssl, s_active_server->host);
    mbedtls_ssl_set_bio(&ssl, &sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "      TLS failed: -0x%04X", -ret);
            goto cleanup;
        }
    }
    ESP_LOGI(TAG, "      TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ssl));

    // ========== Step 2: ServerHello ==========
    ESP_LOGI(TAG, "[2/5] Waiting for ServerHello...");
    int content_len = smp_read_block(&ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      No ServerHello");
        goto cleanup;
    }

    uint8_t *hello = block + 2;
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    uint8_t sessIdLen = hello[4];

    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "      Unexpected sessionId length: %d", sessIdLen);
        goto cleanup;
    }
    memcpy(session_id, &hello[5], 32);

    ESP_LOGI(TAG, "      Server version range: %d-%d (we choose: 7)", minVer, maxVer);
    ESP_LOGI(TAG, "      SessionId: %02x%02x%02x%02x...",
             session_id[0], session_id[1], session_id[2], session_id[3]);

    // ========== Step 3: ClientHello ==========
    ESP_LOGI(TAG, "[3/5] Sending ClientHello...");

    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);

    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
    }

    // SEC-07: Verify TLS certificate fingerprint against selected server
    if (memcmp(ca_hash, s_active_server->key_hash, 32) != 0) {
        ESP_LOGE(TAG, "SEC-07: FINGERPRINT MISMATCH for %s!", s_active_server->host);
        ESP_LOGE(TAG, "   Expected: %02x%02x%02x%02x...",
                 s_active_server->key_hash[0], s_active_server->key_hash[1],
                 s_active_server->key_hash[2], s_active_server->key_hash[3]);
        ESP_LOGE(TAG, "   Got:      %02x%02x%02x%02x...",
                 ca_hash[0], ca_hash[1], ca_hash[2], ca_hash[3]);
        goto cleanup;
    }
    ESP_LOGI(TAG, "      SEC-07: Fingerprint verified for %s", s_active_server->host);

    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x07;
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;

    ret = smp_write_handshake_block(&ssl, block, client_hello, pos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      ClientHello sent!");

    // ========== Step 4: Load or Create Contacts ==========
    ui_splash_set_status("Loading contacts...");
    ui_splash_set_progress(85);
    ESP_LOGI(TAG, "[4/5] Loading contacts...");

    if (!session_restored) {
        // Fresh start for testing - comment out in production!
        ESP_LOGW(TAG, "      Clearing old contacts for fresh test...");
        clear_all_contacts();
    } else {
        ESP_LOGI(TAG, "      Session restored - keeping persisted contacts");
    }

    load_contacts_from_nvs();

    if (contacts_db.num_contacts == 0) {
        if (session_restored) {
            ESP_LOGE(TAG, "      Session restored but no contacts! Falling back to fresh start...");
            session_restored = false;
        }
        ESP_LOGI(TAG, "      No contacts - user can add via Contacts screen");
    } else {
        ESP_LOGI(TAG, "      %d contact(s) loaded from NVS", contacts_db.num_contacts);
    }

    list_contacts();

    // ========== Step 5: Subscribe All Contacts ==========
    ui_splash_set_status("Subscribing to queues...");
    ui_splash_set_progress(90);
    ESP_LOGI(TAG, "[5/5] Subscribing to queues...");
    subscribe_all_contacts(&ssl, block, session_id);

    // Invite links printed to console for debug (not auto-displayed on screen)
    if (contacts_db.num_contacts > 0) {
        print_invitation_links(ca_hash, s_active_server->host, s_active_server->port);
    }

    // ========== Phase 2: Start FreeRTOS Tasks (all PSRAM) ==========
    ESP_LOGI(TAG, "Free heap before tasks: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free INTERNAL heap: %lu bytes",
             (unsigned long)esp_get_free_internal_heap_size());

    if (smp_tasks_init() != 0) {
        ESP_LOGE(TAG, "Task infrastructure init failed!");
        goto cleanup;
    }
    if (smp_tasks_start(&ssl, session_id, sock) != 0) {
        ESP_LOGE(TAG, "Task start failed!");
        goto cleanup;
    }

    // ========== App Logic (runs on main task for NVS access) ==========
    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "|   App logic on main task (64KB)      |");
    ESP_LOGI(TAG, "+--------------------------------------+");

    /* Session 48: Splash done - subscribe finished in smp_connect(),
     * tasks started, ready for messages. */
    {
#if defined(CONFIG_NVS_ENCRYPTION)
        bool vault = true;
#else
        bool vault = false;
#endif
        uint8_t pq = smp_settings_get_pq_enabled();
        int layers = pq ? 5 : 4;
        if (vault && pq) {
            char msg[64];
            snprintf(msg, sizeof(msg), "eFuse sealed. %d layers. Quantum-ready.", layers);
            ui_splash_set_status(msg);
        } else if (vault) {
            char msg[64];
            snprintf(msg, sizeof(msg), "eFuse sealed. %d layers active.", layers);
            ui_splash_set_status(msg);
        } else if (pq) {
            char msg[64];
            snprintf(msg, sizeof(msg), "%d layers active. Quantum-ready.", layers);
            ui_splash_set_status(msg);
        } else {
            ui_splash_set_status("All systems go.");
        }
        ui_splash_set_progress(100);
    }

    // This blocks forever (ring buffer read loop)
#ifdef TEST_DUAL_TLS
    test_dual_tls();
#endif
    smp_app_run(kbd_msg_queue);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "|       Session ended                  |");
    ESP_LOGI(TAG, "+--------------------------------------+");

cleanup:
    free(block);
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    if (sock >= 0) close(sock);
}

// ============== App Main ==============

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SimpleGo v0.1.17-alpha starting...");

    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium init failed!");
        return;
    }
    ESP_LOGI(TAG, "libsodium initialized");

    if (!x448_init()) {
        ESP_LOGE(TAG, "X448 init failed!");
        return;
    }
    ESP_LOGI(TAG, "X448 initialized (wolfSSL)");

    /* ========== SEC-02: NVS Initialization (Open or Vault Mode) ==========
     *
     * When CONFIG_NVS_ENCRYPTION + HMAC scheme is enabled (Vault build),
     * nvs_flash_init() automatically handles eFuse provisioning:
     *   1. Checks if HMAC key exists at BLOCK_KEY1
     *   2. If absent: generates random key, burns to eFuse, sets read-protect
     *   3. Derives XTS encryption keys via HMAC peripheral
     *   4. Initializes NVS with transparent encryption
     *
     * When CONFIG_NVS_ENCRYPTION is disabled (Open build), nvs_flash_init()
     * performs standard unencrypted initialization. No eFuse is touched.
     *
     * The CONFIG_SIMPLEGO_AUTO_PROVISION_EFUSE flag is a SimpleGo-level
     * guard that mirrors the NVS encryption setting for logging purposes. */

#if defined(CONFIG_SIMPLEGO_AUTO_PROVISION_EFUSE) && defined(CONFIG_NVS_ENCRYPTION)
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SEC-02: Security Mode = VAULT (HMAC NVS Encryption)");
    ESP_LOGI(TAG, "SEC-02: eFuse BLOCK_KEY1 will be auto-provisioned on first boot");
    ESP_LOGI(TAG, "");
#else
    ESP_LOGI(TAG, "SEC-02: Security Mode = OPEN (NVS unencrypted)");
#endif

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS format change detected, erasing for re-init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init FAILED: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "SEC-02: Cannot continue without NVS. Halting.");
        while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

#if defined(CONFIG_SIMPLEGO_AUTO_PROVISION_EFUSE) && defined(CONFIG_NVS_ENCRYPTION)
    /* Log eFuse status after init (key should now be provisioned) */
    ESP_LOGI(TAG, "SEC-02: NVS encrypted init OK. Verify with:");
    ESP_LOGI(TAG, "  espefuse.py -c esp32s3 -p COM6 summary");
#else
    ESP_LOGI(TAG, "NVS initialized (unencrypted mode)");
#endif

    // Storage Phase 1: NVS only (before display, no SPI conflict)
    smp_storage_init();
    smp_storage_print_info();
    storage_load_tz_offset();  /* Session 48: Before first clock display */
    // Bug #30: Boot test removed (saved ~50ms)

    // Session 33: Allocate multi-contact arrays in PSRAM
    if (!ratchet_multi_init()) {
        ESP_LOGE(TAG, "Ratchet multi-init failed!");
        return;
    }
    if (!handshake_multi_init()) {
        ESP_LOGE(TAG, "Handshake multi-init failed!");
        return;
    }
    // Session 33: Allocate contacts array in PSRAM
    if (!contacts_init_psram()) {
        ESP_LOGE(TAG, "Contacts PSRAM init failed!");
        return;
    }

    // Session 34 Phase 6: Per-contact reply queues in PSRAM
    if (!reply_queues_init()) {
        ESP_LOGE(TAG, "Reply queues PSRAM init failed!");
        // Non-fatal: legacy our_queue still works
    }

    // Display + LVGL Init
    ESP_LOGI(TAG, "Initializing Display...");
    ret = tdeck_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Initializing LVGL...");
        ret = tdeck_lvgl_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL init failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Initializing Touch...");
            ret = tdeck_touch_init();
            if (ret == ESP_OK) {
                tdeck_touch_register_lvgl();
                ESP_LOGI(TAG, "Touch input enabled!");
            } else {
                ESP_LOGW(TAG, "Touch init failed - continuing without touch");
            }

            // Session 32: Keyboard via LVGL (replaces raw polling task)
            ESP_LOGI(TAG, "Initializing Keyboard...");
            ret = tdeck_keyboard_init();
            if (ret == ESP_OK) {
                kbd_msg_queue = xQueueCreate(4, 256);

                lv_indev_t *kb_indev = tdeck_keyboard_register_lvgl();
                if (kb_indev) {
                    ui_chat_set_keyboard_indev(kb_indev);
                    ESP_LOGI(TAG, "Keyboard -> LVGL -> Chat linked! [KB]");
                }
            } else {
                ESP_LOGW(TAG, "Keyboard init failed - continuing without keyboard");
            }

            ESP_LOGI(TAG, "Initializing UI...");

            // Session 37c: Init RAM font copies with umlaut fallback
            memcpy(&simplego_font_14, &lv_font_montserrat_14, sizeof(lv_font_t));
            memcpy(&simplego_font_12, &lv_font_montserrat_12, sizeof(lv_font_t));
            memcpy(&simplego_font_10, &lv_font_montserrat_10, sizeof(lv_font_t));
            simplego_font_14.fallback = &simplego_umlauts_14;
            simplego_font_12.fallback = &simplego_umlauts_12;
            simplego_font_10.fallback = &simplego_umlauts_10;

            ui_manager_init();

            tdeck_lvgl_start();

            // Session 32: Wire up chat bridge
            ui_chat_set_send_callback(chat_send_cb);
            lv_timer_create(ui_poll_timer_cb, 50, NULL);  // 50ms UI poll
            ESP_LOGI(TAG, "Chat UI bridge active (50ms poll)");

            vTaskDelay(pdMS_TO_TICKS(50));
            tdeck_backlight_set(8);            /* Session 38j: 50% = 8/16 */
        }
    }

    // Storage Phase 2: SD card (after display owns SPI bus)
    ui_splash_set_status("Mounting secure storage...");
    ui_splash_set_progress(10);
    tdeck_lvgl_lock(1000);   /* Session 48: SPI2 bus shared with display */
    smp_storage_init_sd();
    tdeck_lvgl_unlock();

    ui_splash_set_status("Connecting to WiFi...");
    ui_splash_set_progress(20);
    wifi_manager_init();

    /* Session 39k: Auto-launch WiFi setup if no credentials stored.
     * Sets flag that ui_poll_timer_cb reads on LVGL task to navigate
     * to Settings > WiFi tab. Main task keeps waiting below --
     * LVGL runs on its own task so UI stays responsive. */
    if (wifi_manager_needs_setup()) {
        ESP_LOGI(TAG, "No WiFi credentials -- launching WiFi setup UI");
        s_wifi_setup_pending = true;
    }

    ESP_LOGI(TAG, "Waiting for WiFi...");
    int wifi_wait_ticks = 0;
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (++wifi_wait_ticks % 100 == 0) {
            ESP_LOGI(TAG, "WiFi: still waiting... (%ds)", wifi_wait_ticks / 10);
        }
    }
    ui_splash_set_status("WiFi connected");
    ui_splash_set_progress(40);
    vTaskDelay(pdMS_TO_TICKS(500));

    // ========== Auftrag 35g: NTP Time Sync (non-blocking) ==========
    // Bug #30: Init NTP but don't block boot waiting for sync.
    // Sync completes in background, timestamps use uptime until then.
    ESP_LOGI(TAG, "Starting NTP sync (non-blocking)...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(ntp_sync_callback);
    esp_sntp_init();

    // Bug #30: sntrup761 standalone test removed (saved ~2200ms)
    // PQ KEM verified in Session 46, crypto task validates on every use.

    // Session 46 Teil E: Start PQ crypto task (80 KB PSRAM, for keygen/encap/decap)
    ui_splash_set_status("Loading quantum encryption...");
    ui_splash_set_progress(50);
    if (pq_crypto_task_init() != ESP_OK) {
        ESP_LOGE(TAG, "PQ crypto task init failed! PQ operations will block.");
    } else {
        pq_crypto_precompute_keypair();  // Pre-generate first keypair in background
    }

    // Session 46: Read PQ setting from NVS (creates default ON if missing)
    uint8_t pq_on = smp_settings_get_pq_enabled();
    ESP_LOGI(TAG, "SEC-06: Post-Quantum Encryption = %s", pq_on ? "ON" : "OFF");

    // Bug #30: PQ header + HKDF KAT tests removed (saved ~170ms)
    // Verified in Session 46, crypto correctness proven by successful PQ messages.

    // Session 49: Initialize multi-server list (NVS or presets on first boot)
    ui_splash_set_status("Loading servers...");
    ui_splash_set_progress(55);
    smp_servers_init();
    s_active_server = smp_servers_get_active();
    if (!s_active_server) {
        ESP_LOGE(TAG, "No storage server available! Cannot continue.");
        while (1) vTaskDelay(pdMS_TO_TICKS(10000));
    }
    ESP_LOGI(TAG, "Active server: %s:%d", s_active_server->host, s_active_server->port);

    // ========== Auftrag 50b: Session Restoration or Fresh Start ==========
    ui_splash_set_status("Restoring encrypted session...");
    ui_splash_set_progress(60);
    if (smp_storage_exists("rat_00") && smp_storage_exists("queue_our")) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "+----------------------------------------+");
        ESP_LOGI(TAG, "|  [LOAD] RESTORING PREVIOUS SESSION                               |");
        ESP_LOGI(TAG, "+----------------------------------------+");
        ESP_LOGI(TAG, "");

        bool ratchet_ok = ratchet_load_state(0);
        bool queue_ok = queue_load_credentials();
        bool peer_ok = peer_load_state(0);
        bool hand_ok = handshake_load_state();

        if (ratchet_ok && queue_ok) {
            session_restored = true;
            ESP_LOGI(TAG, "[OK] Session restored! Skipping queue creation. (peer=%s, hand=%s)",
                     peer_ok ? "[OK]" : "[WARN]",
                     hand_ok ? "[OK]" : "[WARN]");

            // Session 49: Override active server with queue server.
            // Queues live on our_queue.server_host - we MUST connect there,
            // regardless of what the user selected as "active server".
            // The user-selected server only applies to fresh starts (no contacts).
            if (our_queue.server_host[0] != '\0') {
                smp_server_t *queue_srv = smp_servers_find_by_host(our_queue.server_host);
                if (queue_srv) {
                    s_active_server = queue_srv;
                    ESP_LOGI(TAG, "Session has queues on %s - connecting there (not user-selected server)",
                             our_queue.server_host);
                } else {
                    ESP_LOGW(TAG, "Queue server %s not in server list - using as-is",
                             our_queue.server_host);
                }
            }

            // Session 34 Phase 6: Load per-contact reply queues
            int rq_loaded = reply_queues_load_all();
            ESP_LOGI(TAG, "Reply queues: %d loaded from NVS", rq_loaded);

            // Session 46 Teil F: Load PQ KEM state from separate NVS keys
            if (pq_nvs_load(0)) {
                ESP_LOGI(TAG, "SEC-06: PQ state restored from NVS");
            }
        } else {
            ESP_LOGW(TAG, "[WARN] Partial restore failed (ratchet=%d, queue=%d) - fresh start",
                     ratchet_ok, queue_ok);
            session_restored = false;
        }
    } else {
        ESP_LOGI(TAG, "No previous session found - fresh start");
        session_restored = false;
    }

    if (!session_restored) {
        // Create Reply Queue (fresh start only)
        ESP_LOGI(TAG, "Creating reply queue on %s:%d...",
                 s_active_server->host, s_active_server->port);

        if (!queue_create(s_active_server->host, s_active_server->port)) {
            ESP_LOGE(TAG, "Failed to create reply queue!");
            ESP_LOGW(TAG, "  Continuing without reply queue...");
        } else {
            ESP_LOGI(TAG, "Reply queue created! sndId: %02x%02x%02x%02x... (%d bytes)",
                     our_queue.snd_id[0], our_queue.snd_id[1],
                     our_queue.snd_id[2], our_queue.snd_id[3],
                     our_queue.snd_id_len);
        }

        queue_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    smp_connect();

    ESP_LOGI(TAG, "Done!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
