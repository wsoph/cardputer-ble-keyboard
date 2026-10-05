# Releasing

1. Update `include/version.h` and changelog. Keep tag and artifact versions identical.
2. Run native checks, pinned firmware build, and device acceptance in `VALIDATION.md`.
3. Run `Package-Keyboard.ps1`. Optional `-Python`, `-BuildDirectory`, `-PackageRoot`, `-FrameworkDirectory` select external paths. Never overwrite previous packages.
4. Export only reviewed public files; exclude local history, VM/SSH material, credentials, machine paths, caches and logs.
5. Publish a matching GitHub tag/release with both images, checksums, source and debug artifacts. Explicitly state untested standalone boot and mark such releases prerelease.

## M5Burner

Official guide: https://docs.m5stack.com/en/uiflow/m5burner/publish

Log in with a **M5Stack community account**. Open **USER CUSTOM → Publish**. Use the files in `release/` for name, version, description, source URL and cover. Choose **Cardputer-ADV** if offered. Use the **standalone** image at `0x0000` when raw bin is accepted. The Launcher app file has no bootloader/partition table.

The guide recommends the Export function. Never export a used device's flash for public upload: it can contain bonds or credentials. If the uploader requires an exported container, use a dedicated clean test device with the clean standalone image, then inspect its export. Do not guess the container format.

Upload, then click **Publish** to make it public. Record and verify the listing/share code. GitHub login does not sign in to M5Stack; the account owner performs community login.

The standalone image replaces Launcher, apps and settings. Users retaining Launcher should use the GitHub app image. Packages come from clean build outputs, never device NVS.

## Rollback

Retain previous releases. Launcher users can select a retained app. Standalone users need their own backup or trusted Launcher image to restore another layout; releases contain no private device backups.
