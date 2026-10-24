# XenevaOS Versioning and Build Identification Policy

**Status:** Proposed - Initial Draft  
**Purpose:** Establish a consistent versioning scheme for OS releases, kernel revisions, build artifacts, architectures, editions, and device-specific images.

## 1. Operating System Version

Use Semantic Versioning with prerelease identifiers.

**Format:** `MAJOR.MINOR.PATCH[-CHANNEL[.N]]`

Examples:

- `0.1.0-alpha.1`
- `0.1.0-beta.1`
- `0.1.0-rc.1`
- `1.0.0`

Version increment rules:

- **Major:** Breaking compatibility or architectural changes.
- **Minor:** New functionality that preserves the defined compatibility guarantees.
- **Patch:** Backward-compatible bug fixes.

During initial development, `0.y.z` may be used until the project declares its compatibility guarantees.

## 2. Hybrid Version and Build Metadata

Include the build date and sequence in the complete version identifier.

**Format:** `<OS_VERSION>+<YYYYMMDD>.<NNNN>`

Example:

`0.1.0-alpha.1+20261021.0001`

The release version communicates the software release; the build metadata identifies a particular build. Build metadata alone must not be used to determine version precedence.

## 3. Numeric Build ID

Maintain a separate build identifier for every published artifact.

**Format:** `YYYYMMDD.NNNN`

Example: `20261021.0001`

Rules:

- `YYYYMMDD` represents the build date, using UTC as the standard.
- `NNNN` is a four-digit, zero-padded sequence assigned centrally.
- Each published ISO, firmware image, or device-specific artifact receives a unique Build ID.
- Rebuilding an artifact requires a new Build ID.
- Published artifacts must never be silently replaced under an existing Build ID.
- Release checksums should be published alongside the artifacts.

The build identifier does not replace the OS version or Patch component.

## 4. Independent Kernel Versioning

Kernel versions must remain independent of the XenevaOS release version.

Record:

- Upstream/base kernel version.
- Xeneva-specific patch or revision identifier.
- Kernel build identifier, where applicable.

Follow the versioning conventions of the relevant upstream kernel. Do not force kernel revisions to match the OS version.

## 5. Architecture, Edition, and Device Naming

Every artifact must identify its target.

Supported naming categories include:

- Architecture: `arm64`, `esp32`, or another supported target.
- Edition: `universal`, `compact`, `firmware`, or another defined edition.
- Device: `generic` or an approved device codename.
- Release codename: optional human-friendly name for an OS release.

Release codenames and device codenames must be separate from the numeric version.

## 6. Artifact Filename Convention

**Format:**

`xenevaos-<version>-<architecture>-<edition>-<device>-<build-id>.<extension>`

Illustrative examples:

- `xenevaos-0.1.0-alpha.1-arm64-universal-generic-20261021.0001.iso`
- `xenevaos-0.1.0-alpha.1-arm64-compact-generic-20261021.0002.iso`
- `xenevaos-0.1.0-alpha.1-esp32-firmware-<board-codename>-20261021.0003.bin`
- `xenevaos-0.1.0-alpha.1-arm64-device-<device-codename>-20261021.0004.iso`

These filenames are examples of the proposed convention, not actual release artifacts.

## 7. Release Information

Each published release should disclose:

- OS version and release channel.
- Release codename, if applicable.
- Kernel version and Xeneva revision.
- Architecture, edition, and supported device.
- Build date and unique Build ID.
- Checksums and release notes.

## 8. Guiding Principle

The OS version communicates release-level changes. The kernel version tracks the kernel independently. The Build ID identifies the exact published artifact, while architecture, edition, and device labels identify its target.

**Status:** Initial proposal for team review and approval.
