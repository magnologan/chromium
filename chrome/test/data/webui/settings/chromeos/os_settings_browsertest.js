// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for Chrome OS settings page. */

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_AllJsTests DISABLED_AllJsTests');
GEN('#else');
GEN('#define MAYBE_AllJsTests AllJsTests');
GEN('#endif');

// Generic text fixture for CrOS Polymer Settings elements to be overridden by
// individual element tests.
const OSSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(
        [BROWSER_SETTINGS_PATH + 'ensure_lazy_loaded.js']);
  }

  /** @override */
  setUp() {
    super.setUp();
    settings.ensureLazyLoaded('chromeos');
  }
};

// Tests for the About section.
// eslint-disable-next-line no-var
var OSSettingsAboutPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_about_page/os_about_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_about_page_browser_proxy.js',
      'os_about_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAboutPageTest', 'AboutPage', () => {
  settings_about_page.registerTests();
  mocha.run();
});

GEN('#if defined(GOOGLE_CHROME_BUILD)');
TEST_F('OSSettingsAboutPageTest', 'AboutPage_OfficialBuild', () => {
  settings_about_page.registerOfficialBuildTests();
  mocha.run();
});
GEN('#endif');

// Test fixture for the chrome://os-settings/accounts page
// eslint-disable-next-line no-var
var OSSettingsAddUsersTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'accounts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'add_users_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAddUsersTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the advanced page browser tests.
// eslint-disable-next-line no-var
var OSSettingsAdvancedPageBrowserTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_advanced_page_browsertest.js',
    ]);
  }
};

// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
TEST_F('OSSettingsAdvancedPageBrowserTest', 'MAYBE_AllJsTests', () => {
  // Run all registered tests.
  mocha.run();
});

// Tests for the App section.
// eslint-disable-next-line no-var
var OSSettingsAndroidAppsPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'android_apps_page/android_apps_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'chromeos/test_android_apps_browser_proxy.js',
      'android_apps_page_test.js',
    ]);
  }
};

// Disabled due to flakiness on linux-chromeos-rel
TEST_F('OSSettingsAndroidAppsPageTest', 'DISABLED_AllJsTests', () => {
  mocha.run();
});

// Tests for the Device page.
// eslint-disable-next-line no-var
var OSSettingsBluetoothPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'bluetooth_page/bluetooth_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      'fake_bluetooth.js',
      'fake_bluetooth_private.js',
      'bluetooth_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsBluetoothPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the Crostini page.
// eslint-disable-next-line no-var
var OSSettingsCrostiniPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'crostini_page/crostini_page.html';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kCrostini']};
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_crostini_browser_proxy.js',
      'crostini_page_test.js',
    ]);
  }
};

// TODO(crbug.com/962114): Disabled due to flakes on linux-chromeos-rel.
TEST_F('OSSettingsCrostiniPageTest', 'DISABLED_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Date and Time page.
// eslint-disable-next-line no-var
var OSSettingsDateTimePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'date_time_page/date_time_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'date_time_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsDateTimePageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the Device page.
// eslint-disable-next-line no-var
var OSSettingsDevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'device_page/device_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'fake_system_display.js',
      'device_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsDevicePageTest', 'DevicePageTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.DevicePage)).run();
});

TEST_F('OSSettingsDevicePageTest', 'DisplayTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Display)).run();
});

TEST_F('OSSettingsDevicePageTest', 'KeyboardTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Keyboard)).run();
});

TEST_F('OSSettingsDevicePageTest', 'PointersTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Pointers)).run();
});

TEST_F('OSSettingsDevicePageTest', 'PowerTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Power)).run();
});

TEST_F('OSSettingsDevicePageTest', 'StylusTest', () => {
  mocha.grep(assert(device_page_tests.TestNames.Stylus)).run();
});

// Tests for the Fingerprint page.
// eslint-disable-next-line no-var
var OSSettingsFingerprintListTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/fingerprint_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'fingerprint_browsertest_chromeos.js',
    ]);
  }
};

TEST_F('OSSettingsFingerprintListTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for Google Assistant Page.
// eslint-disable-next-line no-var
var OSSettingsGoogleAssistantPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'google_assistant_page/google_assistant_page.html';
  }

  /** @override */
  get commandLineSwitches() {
    return [{
      switchName: 'enable-voice-interaction',
    }];
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'google_assistant_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsGoogleAssistantPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for settings-internet-detail-page.
// eslint-disable-next-line no-var
var OSSettingsInternetDetailPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'internet_page/internet_detail_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/util.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_networking_private.js',
      BROWSER_SETTINGS_PATH + '../chromeos/cr_onc_strings.js',
      'internet_detail_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetDetailPageTest', 'InternetDetailPage', () => {
  mocha.run();
});

// Test fixture for settings-internet-page.
// eslint-disable-next-line no-var
var OSSettingsInternetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'internet_page/internet_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_network_config_mojom.js',
      BROWSER_SETTINGS_PATH + '../chromeos/fake_networking_private.js',
      BROWSER_SETTINGS_PATH + '../chromeos/cr_onc_strings.js',
      'internet_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsInternetPageTest', 'InternetPage', () => {
  mocha.run();
});

// Test fixture for the main settings page.
// eslint-disable-next-line no-var
var OSSettingsMainTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_settings_main/os_settings_main.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'os_settings_main_test.js',
    ]);
  }
};

TEST_F('OSSettingsMainTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the side-nav menu.
// eslint-disable-next-line no-var
var OSSettingsMenuTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_settings_menu_test.js',
    ]);
  }
};

TEST_F('OSSettingsMenuTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage feature item.
// eslint-disable-next-line no-var
var OSSettingsMultideviceFeatureItemTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'multidevice_page/multidevice_feature_item.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'multidevice_feature_item_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureItemTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage feature toggle.
// eslint-disable-next-line no-var
var OSSettingsMultideviceFeatureToggleTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'multidevice_page/multidevice_feature_toggle.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'multidevice_feature_toggle_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceFeatureToggleTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings page.
// eslint-disable-next-line no-var
var OSSettingsMultidevicePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'multidevice_page/multidevice_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultidevicePageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice Smart Lock subpage.
// eslint-disable-next-line no-var
var OSSettingsMultideviceSmartLockSubpageTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'multidevice_page/multidevice_smartlock_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_smartlock_subpage_test.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceSmartLockSubpageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the multidevice settings subpage.
// eslint-disable-next-line no-var
var OSSettingsMultideviceSubpageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'multidevice_page/multidevice_subpage.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_multidevice_browser_proxy.js',
      'multidevice_subpage_tests.js',
    ]);
  }
};

TEST_F('OSSettingsMultideviceSubpageTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageAccountManagerTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/account_manager.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'people_page_account_manager_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageAccountManagerTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageChangePictureTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/change_picture.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'people_page_change_picture_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageChangePictureTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageKerberosAccountsTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/kerberos_accounts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'people_page_kerberos_accounts_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageKerberosAccountsTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageLockScreenTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/lock_screen.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'fake_quick_unlock_private.js',
      'fake_quick_unlock_uma.js',
      'quick_unlock_authenticate_browsertest_chromeos.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageLockScreenTest', 'AllJsTests', () => {
  settings_people_page_quick_unlock.registerLockScreenTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageQuickUnlockAuthenticateTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'people_page/lock_screen_password_prompt_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      'fake_quick_unlock_private.js', 'fake_quick_unlock_uma.js',
      'quick_unlock_authenticate_browsertest_chromeos.js'
    ]);
  }
};

TEST_F('OSSettingsPeoplePageQuickUnlockAuthenticateTest', 'AllJsTests', () => {
  settings_people_page_quick_unlock.registerAuthenticateTests();
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsPeoplePageSetupPinDialogTest =
    class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'people_page/setup_pin_dialog.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      'fake_quick_unlock_private.js', 'fake_quick_unlock_uma.js',
      'quick_unlock_authenticate_browsertest_chromeos.js'
    ]);
  }
};

TEST_F('OSSettingsPeoplePageSetupPinDialogTest', 'AllJsTests', () => {
  settings_people_page_quick_unlock.registerSetupPinDialogTests();
  mocha.run();
});

// Tests for the People section.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'sync_test_util.js',
      BROWSER_SETTINGS_PATH + 'test_profile_info_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      'os_people_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});

// Tests for the Personalization section.
// eslint-disable-next-line no-var
var OSSettingsPersonalizationPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'personalization_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPersonalizationPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Plugin VM page.
// eslint-disable-next-line no-var
var OSSettingsPluginVmPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'plugin_vm_page/plugin_vm_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/promise_resolver.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'plugin_vm_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPluginVmPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS printer entry.
// eslint-disable-next-line no-var
var OSSettingsPrinterEntryTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'printing_page/cups_printers_entry_list.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'cups_printer_entry_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrinterEntryTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS printer landing page.
// eslint-disable-next-line no-var
var OSSettingsPrinterLandingPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'printing_page/cups_printers.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_cups_printers_browser_proxy.js',
      'cups_printer_landing_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrinterLandingPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Tests for the CUPS page.
// eslint-disable-next-line no-var
var OSSettingsPrintingPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'printing_page/cups_printers.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/assert.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'test_cups_printers_browser_proxy.js',
      'cups_printer_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsPrintingPageTest', 'AllJsTests', () => {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsLanguagesPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_languages_page/os_languages_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../fake_chrome_event.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_language_settings_private.js',
      BROWSER_SETTINGS_PATH + 'test_languages_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'fake_settings_private.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'fake_input_method_private.js',
      'os_languages_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsLanguagesPageTest', 'LanguageMenu', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.LanguageMenu)).run();
});

TEST_F('OSSettingsLanguagesPageTest', 'InputMethods', function() {
  mocha.grep(assert(os_languages_page_tests.TestNames.InputMethods)).run();
});

// Tests for the Reset section.
// eslint-disable-next-line no-var
var OSSettingsResetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'reset_page/reset_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_reset_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_reset_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsResetPageTest', 'AllJsTests', () => {
  mocha.run();
});

// Test fixture for the Smb Shares page.
// eslint-disable-next-line no-var
var OSSettingsSmbPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_files_page/smb_shares_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'smb_shares_page_tests.js',
    ]);
  }
};

// Settings tests are flaky on debug. See https://crbug.com/968608.
TEST_F('OSSettingsSmbPageTest', 'MAYBE_AllJsTests', () => {
  mocha.run();
});
