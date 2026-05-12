# Changelog

## [3.2.12] - 2026-05-12
### Added
- **Lock Screen Deferral**: The phone prompt is now deferred until the Windows Lock Screen is dismissed and the Sign-in screen is visible. This prevents early prompts when "Wait for key press" is disabled.
- **Default Provider Enforcement**: PC Bio Unlock now sets itself as the `LastLoggedOnProvider` in the registry upon successful authentication, ensuring Windows defaults back to it instead of PIN.

## [3.2.11] - 2026-05-07
### Added
- **Hybrid Bluetooth Scanning**: Combined Classic BT cache with BLE passive scanning for faster and more reliable device discovery.
- **Categorized Bluetooth UI**: Redesigned the pairing interface into "Saved Devices", "Search Results", and a collapsible "Unknown Devices" section.
- **MAC Address Display**: Added device MAC addresses to the Bluetooth list for easier identification.
- **CredUI & UAC Support**: Enabled the provider for `CPUS_CREDUI` scenarios, allowing it to work with UAC prompts and browser password autofill verifications.
- **Transparent Lock Screen Icon**: Replaced legacy bitmap logic with WIC-based PNG decoding to fix black background artifacts on the lock screen.
### Fixed
- **Bluetooth Contention**: Migrated from WinSock inquiry to BLE Advertisement Watcher to resolve Error 10108 radio contention issues.
