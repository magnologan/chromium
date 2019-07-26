# QuiChrome

HTTP/3 implementation inside Chromium built on top of
[ngtcp2](https://github.com/ngtcp2/ngtcp2).

## Build Instructions

- Follow Chromium's instructions to get Chromium's code
  (<https://www.chromium.org/developers/how-tos/get-the-code>) until the
  'Setting up the build' section.

- Add this github fork as a second remote:

  ```sh
  git remote add fork https://github.com/DaanDeMeyer/chromium.git
  ```

- Fetch and checkout the iquic branch:

  ```sh
  git fetch fork iquic
  git checkout iquic
  ```

- Sync Chromium's third party dependencies:

  ```sh
  gclient sync
  ```

- Generate ninja build files:

  ```sh
  gn gen out/default
  ```

  To get faster builds, run `gn args out/default` and write the following args:

  ```txt
  is_debug = false
  is_component_build = true
  enable_nacl = false
  blink_symbol_level = 0
  symbol_level = 1
  ```

  These flags are explained in the 'Faster Builds' section in Chromium's
  get-the-code documentation.

- Build the chrome target:

  ```sh
  autoninja -C out/default chrome
  ```

- Run the resulting chrome executable, specifying the iquic version and forcing
  quic to be used on every website:

  ```sh
  out/default/chrome --quic-version=IQUIC_DRAFT_22 --origin-to-force-quic-on=* <url>
  ```
