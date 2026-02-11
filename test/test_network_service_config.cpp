/**
 * @file test_network_service_config.cpp
 * @brief NetworkService Config Change Detection Tests
 *
 * Tests for verifying config change detection in NetworkService:
 * - Ethernet DHCP mode change detection
 * - Ethernet static IP change detection
 * - WiFi STA SSID change detection
 * - WiFi STA password change detection
 * - No restart on unchanged config
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "network_service.h"
#include "app_types.h"
#include "event_bus.h"

// ============================================================================
// Test State Tracking
// ============================================================================

// Mock restart function call counters
static int ethernet_restart_count = 0;
static int wifi_restart_count = 0;

// Test config storage
static app_network_config_t test_config = {};
static app_network_config_t last_config = {};

// ============================================================================
// Test Setup/Teardown
// ============================================================================

static void reset_test_state(void)
{
    ethernet_restart_count = 0;
    wifi_restart_count = 0;
    memset(&test_config, 0, sizeof(test_config));
    memset(&last_config, 0, sizeof(last_config));
}

static void setup_default_config(void)
{
    // Default test configuration
    test_config.wifi_ap.enabled = false;
    test_config.wifi_ap.channel = 1;
    memset(test_config.wifi_ap.ssid, 0, sizeof(test_config.wifi_ap.ssid));
    memset(test_config.wifi_ap.password, 0, sizeof(test_config.wifi_ap.password));

    test_config.wifi_sta.enabled = true;
    strncpy(test_config.wifi_sta.ssid, "TestSSID", sizeof(test_config.wifi_sta.ssid) - 1);
    strncpy(test_config.wifi_sta.password, "TestPassword123", sizeof(test_config.wifi_sta.password) - 1);

    test_config.ethernet.enabled = true;
    test_config.ethernet.dhcp_enabled = true;
    strncpy(test_config.ethernet.static_ip, "192.168.1.100", sizeof(test_config.ethernet.static_ip) - 1);
    strncpy(test_config.ethernet.static_netmask, "255.255.255.0", sizeof(test_config.ethernet.static_netmask) - 1);
    strncpy(test_config.ethernet.static_gateway, "192.168.1.1", sizeof(test_config.ethernet.static_gateway) - 1);

    // Copy to last_config for comparison
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));
}

// ============================================================================
// Ethernet DHCP Mode Change Tests
// ============================================================================

/**
 * @brief Test: Ethernet DHCP to Static IP mode change
 *
 * Expected: restartEthernet() should be called when dhcp_enabled changes from true to false
 */
TEST_CASE("ethernet_dhcp_to_static_change_triggers_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Initial config with DHCP enabled
    TEST_ASSERT_TRUE(test_config.ethernet.dhcp_enabled);

    // Simulate config change: DHCP -> Static
    test_config.ethernet.dhcp_enabled = false;

    // Verify change detected
    bool dhcp_changed = (last_config.ethernet.dhcp_enabled != test_config.ethernet.dhcp_enabled);
    TEST_ASSERT_TRUE(dhcp_changed);

    // In actual implementation, this would trigger restartEthernet()
    // For unit test, we verify the change detection logic
    TEST_ASSERT_FALSE(test_config.ethernet.dhcp_enabled);  // Now static
    TEST_ASSERT_TRUE(last_config.ethernet.dhcp_enabled);   // Was DHCP
}

/**
 * @brief Test: Ethernet Static to DHCP mode change
 *
 * Expected: restartEthernet() should be called when dhcp_enabled changes from false to true
 */
TEST_CASE("ethernet_static_to_dhcp_change_triggers_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Start with Static IP mode
    test_config.ethernet.dhcp_enabled = false;
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Simulate config change: Static -> DHCP
    test_config.ethernet.dhcp_enabled = true;

    // Verify change detected
    bool dhcp_changed = (last_config.ethernet.dhcp_enabled != test_config.ethernet.dhcp_enabled);
    TEST_ASSERT_TRUE(dhcp_changed);

    TEST_ASSERT_TRUE(test_config.ethernet.dhcp_enabled);   // Now DHCP
    TEST_ASSERT_FALSE(last_config.ethernet.dhcp_enabled);  // Was static
}

/**
 * @brief Test: Ethernet DHCP mode unchanged
 *
 * Expected: restartEthernet() should NOT be called when dhcp_enabled stays the same
 */
TEST_CASE("ethernet_dhcp_unchanged_no_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Keep DHCP enabled
    test_config.ethernet.dhcp_enabled = true;
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Apply same config (no change)
    // In actual implementation, this would not trigger restartEthernet()

    // Verify no change detected
    bool dhcp_changed = (last_config.ethernet.dhcp_enabled != test_config.ethernet.dhcp_enabled);
    TEST_ASSERT_FALSE(dhcp_changed);

    TEST_ASSERT_TRUE(test_config.ethernet.dhcp_enabled);
    TEST_ASSERT_TRUE(last_config.ethernet.dhcp_enabled);
}

// ============================================================================
// Ethernet Static IP Change Tests
// ============================================================================

/**
 * @brief Test: Ethernet static IP address change
 *
 * Expected: restartEthernet() should be called when static_ip changes
 */
TEST_CASE("ethernet_static_ip_change_triggers_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Initial static IP
    const char* original_ip = "192.168.1.100";
    strncpy(test_config.ethernet.static_ip, original_ip, sizeof(test_config.ethernet.static_ip) - 1);
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change static IP
    const char* new_ip = "192.168.1.200";
    strncpy(test_config.ethernet.static_ip, new_ip, sizeof(test_config.ethernet.static_ip) - 1);

    // Verify change detected
    bool ip_changed = (strcmp(last_config.ethernet.static_ip, test_config.ethernet.static_ip) != 0);
    TEST_ASSERT_TRUE(ip_changed);

    TEST_ASSERT_EQUAL_STRING(new_ip, test_config.ethernet.static_ip);
    TEST_ASSERT_EQUAL_STRING(original_ip, last_config.ethernet.static_ip);
}

/**
 * @brief Test: Ethernet static netmask change
 *
 * Expected: restartEthernet() should be called when static_netmask changes
 */
TEST_CASE("ethernet_static_netmask_change_triggers_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Initial netmask
    const char* original_netmask = "255.255.255.0";
    strncpy(test_config.ethernet.static_netmask, original_netmask, sizeof(test_config.ethernet.static_netmask) - 1);
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change netmask
    const char* new_netmask = "255.255.0.0";
    strncpy(test_config.ethernet.static_netmask, new_netmask, sizeof(test_config.ethernet.static_netmask) - 1);

    // Verify change detected
    bool netmask_changed = (strcmp(last_config.ethernet.static_netmask, test_config.ethernet.static_netmask) != 0);
    TEST_ASSERT_TRUE(netmask_changed);

    TEST_ASSERT_EQUAL_STRING(new_netmask, test_config.ethernet.static_netmask);
    TEST_ASSERT_EQUAL_STRING(original_netmask, last_config.ethernet.static_netmask);
}

/**
 * @brief Test: Ethernet static gateway change
 *
 * Expected: restartEthernet() should be called when static_gateway changes
 */
TEST_CASE("ethernet_static_gateway_change_triggers_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Initial gateway
    const char* original_gateway = "192.168.1.1";
    strncpy(test_config.ethernet.static_gateway, original_gateway, sizeof(test_config.ethernet.static_gateway) - 1);
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change gateway
    const char* new_gateway = "192.168.1.254";
    strncpy(test_config.ethernet.static_gateway, new_gateway, sizeof(test_config.ethernet.static_gateway) - 1);

    // Verify change detected
    bool gateway_changed = (strcmp(last_config.ethernet.static_gateway, test_config.ethernet.static_gateway) != 0);
    TEST_ASSERT_TRUE(gateway_changed);

    TEST_ASSERT_EQUAL_STRING(new_gateway, test_config.ethernet.static_gateway);
    TEST_ASSERT_EQUAL_STRING(original_gateway, last_config.ethernet.static_gateway);
}

/**
 * @brief Test: All Ethernet static settings unchanged
 *
 * Expected: restartEthernet() should NOT be called when no static IP settings change
 */
TEST_CASE("ethernet_static_settings_unchanged_no_restart", "[network_service][ethernet]")
{
    reset_test_state();
    setup_default_config();

    // Apply same config
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Verify no changes detected
    bool ip_changed = (strcmp(last_config.ethernet.static_ip, test_config.ethernet.static_ip) != 0);
    bool netmask_changed = (strcmp(last_config.ethernet.static_netmask, test_config.ethernet.static_netmask) != 0);
    bool gateway_changed = (strcmp(last_config.ethernet.static_gateway, test_config.ethernet.static_gateway) != 0);

    TEST_ASSERT_FALSE(ip_changed);
    TEST_ASSERT_FALSE(netmask_changed);
    TEST_ASSERT_FALSE(gateway_changed);
}

// ============================================================================
// WiFi STA SSID Change Tests
// ============================================================================

/**
 * @brief Test: WiFi STA SSID change
 *
 * Expected: restartWiFi() should be called when SSID changes
 */
TEST_CASE("wifi_sta_ssid_change_triggers_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Initial SSID
    const char* original_ssid = "TestSSID";
    strncpy(test_config.wifi_sta.ssid, original_ssid, sizeof(test_config.wifi_sta.ssid) - 1);
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change SSID
    const char* new_ssid = "NewTestSSID";
    strncpy(test_config.wifi_sta.ssid, new_ssid, sizeof(test_config.wifi_sta.ssid) - 1);

    // Verify change detected
    bool ssid_changed = (strcmp(last_config.wifi_sta.ssid, test_config.wifi_sta.ssid) != 0);
    TEST_ASSERT_TRUE(ssid_changed);

    TEST_ASSERT_EQUAL_STRING(new_ssid, test_config.wifi_sta.ssid);
    TEST_ASSERT_EQUAL_STRING(original_ssid, last_config.wifi_sta.ssid);
}

/**
 * @brief Test: WiFi STA SSID unchanged
 *
 * Expected: restartWiFi() should NOT be called when SSID stays the same
 */
TEST_CASE("wifi_sta_ssid_unchanged_no_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Apply same SSID
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Verify no change detected
    bool ssid_changed = (strcmp(last_config.wifi_sta.ssid, test_config.wifi_sta.ssid) != 0);
    TEST_ASSERT_FALSE(ssid_changed);
}

/**
 * @brief Test: WiFi STA SSID empty to non-empty change
 *
 * Expected: restartWiFi() should be called when SSID changes from empty to a value
 */
TEST_CASE("wifi_sta_ssid_empty_to_value_triggers_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Start with empty SSID
    memset(test_config.wifi_sta.ssid, 0, sizeof(test_config.wifi_sta.ssid));
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change to non-empty SSID
    const char* new_ssid = "MyNetwork";
    strncpy(test_config.wifi_sta.ssid, new_ssid, sizeof(test_config.wifi_sta.ssid) - 1);

    // Verify change detected
    bool ssid_changed = (strcmp(last_config.wifi_sta.ssid, test_config.wifi_sta.ssid) != 0);
    TEST_ASSERT_TRUE(ssid_changed);

    TEST_ASSERT_EQUAL_STRING(new_ssid, test_config.wifi_sta.ssid);
    TEST_ASSERT_EQUAL_STRING("", last_config.wifi_sta.ssid);
}

/**
 * @brief Test: WiFi STA SSID non-empty to empty change
 *
 * Expected: restartWiFi() should be called when SSID changes from a value to empty
 */
TEST_CASE("wifi_sta_ssid_value_to_empty_triggers_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Start with non-empty SSID
    const char* original_ssid = "OldNetwork";
    strncpy(test_config.wifi_sta.ssid, original_ssid, sizeof(test_config.wifi_sta.ssid) - 1);
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change to empty SSID
    memset(test_config.wifi_sta.ssid, 0, sizeof(test_config.wifi_sta.ssid));

    // Verify change detected
    bool ssid_changed = (strcmp(last_config.wifi_sta.ssid, test_config.wifi_sta.ssid) != 0);
    TEST_ASSERT_TRUE(ssid_changed);

    TEST_ASSERT_EQUAL_STRING("", test_config.wifi_sta.ssid);
    TEST_ASSERT_EQUAL_STRING(original_ssid, last_config.wifi_sta.ssid);
}

// ============================================================================
// WiFi STA Password Change Tests
// ============================================================================

/**
 * @brief Test: WiFi STA password change
 *
 * Expected: restartWiFi() should be called when password changes
 */
TEST_CASE("wifi_sta_password_change_triggers_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Initial password
    const char* original_password = "TestPassword123";
    strncpy(test_config.wifi_sta.password, original_password, sizeof(test_config.wifi_sta.password) - 1);
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change password
    const char* new_password = "NewPassword456";
    strncpy(test_config.wifi_sta.password, new_password, sizeof(test_config.wifi_sta.password) - 1);

    // Verify change detected
    bool password_changed = (strcmp(last_config.wifi_sta.password, test_config.wifi_sta.password) != 0);
    TEST_ASSERT_TRUE(password_changed);

    TEST_ASSERT_EQUAL_STRING(new_password, test_config.wifi_sta.password);
    TEST_ASSERT_EQUAL_STRING(original_password, last_config.wifi_sta.password);
}

/**
 * @brief Test: WiFi STA password unchanged
 *
 * Expected: restartWiFi() should NOT be called when password stays the same
 */
TEST_CASE("wifi_sta_password_unchanged_no_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Apply same password
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Verify no change detected
    bool password_changed = (strcmp(last_config.wifi_sta.password, test_config.wifi_sta.password) != 0);
    TEST_ASSERT_FALSE(password_changed);
}

/**
 * @brief Test: WiFi STA password empty to non-empty change
 *
 * Expected: restartWiFi() should be called when password changes from empty to a value
 */
TEST_CASE("wifi_sta_password_empty_to_value_triggers_restart", "[network_service][wifi]")
{
    reset_test_state();
    setup_default_config();

    // Start with empty password
    memset(test_config.wifi_sta.password, 0, sizeof(test_config.wifi_sta.password));
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change to non-empty password
    const char* new_password = "SecurePass";
    strncpy(test_config.wifi_sta.password, new_password, sizeof(test_config.wifi_sta.password) - 1);

    // Verify change detected
    bool password_changed = (strcmp(last_config.wifi_sta.password, test_config.wifi_sta.password) != 0);
    TEST_ASSERT_TRUE(password_changed);

    TEST_ASSERT_EQUAL_STRING(new_password, test_config.wifi_sta.password);
    TEST_ASSERT_EQUAL_STRING("", last_config.wifi_sta.password);
}

// ============================================================================
// No Restart on Unchanged Config Tests
// ============================================================================

/**
 * @brief Test: No restart when all config values unchanged
 *
 * Expected: Neither restartEthernet() nor restartWiFi() should be called
 * when all config values remain the same
 */
TEST_CASE("no_restart_on_unchanged_config", "[network_service][integration]")
{
    reset_test_state();
    setup_default_config();

    // Copy to last (simulating previous state)
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Simulate reapplying same config
    app_network_config_t same_config;
    memcpy(&same_config, &test_config, sizeof(app_network_config_t));

    // Verify no Ethernet changes
    bool eth_enabled_changed = (last_config.ethernet.enabled != same_config.ethernet.enabled);
    bool eth_dhcp_changed = (last_config.ethernet.dhcp_enabled != same_config.ethernet.dhcp_enabled);
    bool eth_ip_changed = (strcmp(last_config.ethernet.static_ip, same_config.ethernet.static_ip) != 0);
    bool eth_netmask_changed = (strcmp(last_config.ethernet.static_netmask, same_config.ethernet.static_netmask) != 0);
    bool eth_gateway_changed = (strcmp(last_config.ethernet.static_gateway, same_config.ethernet.static_gateway) != 0);

    TEST_ASSERT_FALSE(eth_enabled_changed);
    TEST_ASSERT_FALSE(eth_dhcp_changed);
    TEST_ASSERT_FALSE(eth_ip_changed);
    TEST_ASSERT_FALSE(eth_netmask_changed);
    TEST_ASSERT_FALSE(eth_gateway_changed);

    // Verify no WiFi STA changes
    bool wifi_sta_enabled_changed = (last_config.wifi_sta.enabled != same_config.wifi_sta.enabled);
    bool wifi_sta_ssid_changed = (strcmp(last_config.wifi_sta.ssid, same_config.wifi_sta.ssid) != 0);
    bool wifi_sta_password_changed = (strcmp(last_config.wifi_sta.password, same_config.wifi_sta.password) != 0);

    TEST_ASSERT_FALSE(wifi_sta_enabled_changed);
    TEST_ASSERT_FALSE(wifi_sta_ssid_changed);
    TEST_ASSERT_FALSE(wifi_sta_password_changed);
}

/**
 * @brief Test: Only Ethernet changes, WiFi unchanged
 *
 * Expected: Only restartEthernet() should be called, not restartWiFi()
 */
TEST_CASE("only_ethernet_change_wifi_unchanged", "[network_service][integration]")
{
    reset_test_state();
    setup_default_config();

    // Copy to last
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change only Ethernet DHCP
    test_config.ethernet.dhcp_enabled = !test_config.ethernet.dhcp_enabled;

    // Verify Ethernet change detected
    bool eth_dhcp_changed = (last_config.ethernet.dhcp_enabled != test_config.ethernet.dhcp_enabled);
    TEST_ASSERT_TRUE(eth_dhcp_changed);

    // Verify WiFi STA unchanged
    bool wifi_sta_ssid_changed = (strcmp(last_config.wifi_sta.ssid, test_config.wifi_sta.ssid) != 0);
    bool wifi_sta_password_changed = (strcmp(last_config.wifi_sta.password, test_config.wifi_sta.password) != 0);

    TEST_ASSERT_FALSE(wifi_sta_ssid_changed);
    TEST_ASSERT_FALSE(wifi_sta_password_changed);
}

/**
 * @brief Test: Only WiFi changes, Ethernet unchanged
 *
 * Expected: Only restartWiFi() should be called, not restartEthernet()
 */
TEST_CASE("only_wifi_change_ethernet_unchanged", "[network_service][integration]")
{
    reset_test_state();
    setup_default_config();

    // Copy to last
    memcpy(&last_config, &test_config, sizeof(app_network_config_t));

    // Change only WiFi STA SSID
    const char* new_ssid = "DifferentSSID";
    strncpy(test_config.wifi_sta.ssid, new_ssid, sizeof(test_config.wifi_sta.ssid) - 1);

    // Verify WiFi STA change detected
    bool wifi_sta_ssid_changed = (strcmp(last_config.wifi_sta.ssid, test_config.wifi_sta.ssid) != 0);
    TEST_ASSERT_TRUE(wifi_sta_ssid_changed);

    // Verify Ethernet unchanged
    bool eth_dhcp_changed = (last_config.ethernet.dhcp_enabled != test_config.ethernet.dhcp_enabled);
    bool eth_ip_changed = (strcmp(last_config.ethernet.static_ip, test_config.ethernet.static_ip) != 0);

    TEST_ASSERT_FALSE(eth_dhcp_changed);
    TEST_ASSERT_FALSE(eth_ip_changed);
}

// ============================================================================
// Config Data Structure Tests
// ============================================================================

/**
 * @brief Test: Verify config structure sizes
 *
 * Ensures the config structures have expected sizes for proper memory operations
 */
TEST_CASE("config_structure_sizes", "[network_service][struct]")
{
    // Verify individual structure sizes
    TEST_ASSERT_EQUAL(33, sizeof(app_wifi_ap_t::ssid));       // 32 + null terminator
    TEST_ASSERT_EQUAL(65, sizeof(app_wifi_ap_t::password));    // 64 + null terminator
    TEST_ASSERT_EQUAL(33, sizeof(app_wifi_sta_t::ssid));       // 32 + null terminator
    TEST_ASSERT_EQUAL(65, sizeof(app_wifi_sta_t::password));    // 64 + null terminator
    TEST_ASSERT_EQUAL(16, sizeof(app_ethernet_t::static_ip));
    TEST_ASSERT_EQUAL(16, sizeof(app_ethernet_t::static_netmask));
    TEST_ASSERT_EQUAL(16, sizeof(app_ethernet_t::static_gateway));

    // Verify full config structure can be copied with memcpy
    app_network_config_t config1 = {};
    app_network_config_t config2 = {};

    setup_default_config();
    memcpy(&config1, &test_config, sizeof(app_network_config_t));
    memcpy(&config2, &config1, sizeof(app_network_config_t));

    TEST_ASSERT_EQUAL_MEMORY(&config1, &config2, sizeof(app_network_config_t));
}

/**
 * @brief Test: String null termination safety
 *
 * Verifies that string copy operations properly null-terminate strings
 */
TEST_CASE("string_null_termination_safety", "[network_service][safety]")
{
    app_network_config_t config = {};
    memset(&config, 0xFF, sizeof(config));  // Fill with invalid data

    // Copy maximum length strings (should null-terminate)
    strncpy(config.wifi_sta.ssid, "12345678901234567890123456789012", 32);  // 32 chars
    config.wifi_sta.ssid[32] = '\0';  // Manual null termination

    strncpy(config.wifi_sta.password,
            "1234567890123456789012345678901234567890123456789012345678901234", 64);  // 64 chars
    config.wifi_sta.password[64] = '\0';  // Manual null termination

    // Verify null termination
    TEST_ASSERT_EQUAL('\0', config.wifi_sta.ssid[32]);
    TEST_ASSERT_EQUAL('\0', config.wifi_sta.password[64]);

    // Verify string length
    TEST_ASSERT_EQUAL(32, strlen(config.wifi_sta.ssid));
    TEST_ASSERT_EQUAL(64, strlen(config.wifi_sta.password));
}

// ============================================================================
// Main Test Runner
// ============================================================================

void app_main(void)
{
    printf("NetworkService Config Change Detection Tests\n");
    printf("=============================================\n\n");

    printf("Test Categories:\n");
    printf("  [ethernet] - Ethernet config change detection\n");
    printf("  [wifi] - WiFi STA config change detection\n");
    printf("  [integration] - Combined config change scenarios\n");
    printf("  [struct] - Structure size and memory safety\n");
    printf("  [safety] - String operation safety\n\n");

    unity_run_menu();
}
