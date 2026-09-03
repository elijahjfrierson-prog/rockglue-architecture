#!/usr/bin/env bash
# Builds RockGlue-Architecture-macOS.pkg — a guided installer that shows the
# EULA and then drops "RockGlue Architecture.vst3" into
# /Library/Audio/Plug-Ins/VST3/ and "RockGlue Architecture.component" into
# /Library/Audio/Plug-Ins/Components/ (so Logic Pro sees it as an Audio Unit).
#
# Runs on macOS, using pkgbuild + productbuild from the Xcode Command Line Tools.
# Requires build/RockGlue_artefacts/Release/{VST3,AU} to exist already.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"

VERSION="${VERSION:-0.2.0}"
BUNDLE_ID="com.rockglue.architecture.installer"
PKG_OUT="${PKG_OUT:-RockGlue-Architecture-macOS.pkg}"
NAME="RockGlue Architecture"

ART="$REPO/build/RockGlue_artefacts/Release"
VST3_SRC="$ART/VST3/$NAME.vst3"
AU_SRC="$ART/AU/$NAME.component"

if [[ ! -d "$VST3_SRC" ]]; then
  echo "ERROR: $VST3_SRC not found. Run 'cmake --build build --config Release' first." >&2
  exit 1
fi

STAGE="$(mktemp -d)"
VST3_ROOT="$STAGE/vst3_root/Library/Audio/Plug-Ins/VST3"
AU_ROOT="$STAGE/au_root/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_ROOT" "$AU_ROOT"

cp -R "$VST3_SRC" "$VST3_ROOT/"
[[ -d "$AU_SRC" ]] && cp -R "$AU_SRC" "$AU_ROOT/"

# Sign the bundles so Gatekeeper does not reject them outright. A real Developer
# ID is used when APPLE_DEVELOPER_ID is exported; otherwise ad-hoc.
SIGN_IDENTITY="${APPLE_DEVELOPER_ID:--}"
codesign --force --deep --sign "$SIGN_IDENTITY" "$VST3_ROOT/$NAME.vst3" || true
if [[ -d "$AU_ROOT/$NAME.component" ]]; then
  codesign --force --deep --sign "$SIGN_IDENTITY" "$AU_ROOT/$NAME.component" || true
fi

PKGDIR="$STAGE/pkgs"
SCRIPTS_VST3="$STAGE/vst3_scripts"
SCRIPTS_AU="$STAGE/au_scripts"
mkdir -p "$PKGDIR" "$SCRIPTS_VST3" "$SCRIPTS_AU"

# Pre-install cleanup. Dropping a new bundle on top of an old one leaves hosts
# resolving a half-stale plug-in from their own database, which is a classic
# source of "it crashes on load after updating".
cat > "$SCRIPTS_VST3/preinstall" <<PREINSTALL_VST3
#!/bin/sh
rm -rf "/Library/Audio/Plug-Ins/VST3/$NAME.vst3" 2>/dev/null || true
for home in /Users/*; do
  [ -d "\$home" ] || continue
  rm -rf "\$home/Library/Audio/Plug-Ins/VST3/$NAME.vst3" 2>/dev/null || true
done
exit 0
PREINSTALL_VST3
chmod +x "$SCRIPTS_VST3/preinstall"

# The AU also needs the validation cache flushed, otherwise Logic Pro serves a
# cached "approved" result keyed to the previous binary instead of re-validating.
cat > "$SCRIPTS_AU/preinstall" <<PREINSTALL_AU
#!/bin/sh
rm -rf "/Library/Audio/Plug-Ins/Components/$NAME.component" 2>/dev/null || true
for home in /Users/*; do
  [ -d "\$home" ] || continue
  rm -rf "\$home/Library/Audio/Plug-Ins/Components/$NAME.component" 2>/dev/null || true
  rm -rf "\$home/Library/Caches/AudioUnitCache" 2>/dev/null || true
done
rm -rf "/Library/Caches/AudioUnitCache" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
PREINSTALL_AU
chmod +x "$SCRIPTS_AU/preinstall"

pkgbuild \
  --identifier "${BUNDLE_ID}.vst3" \
  --version "$VERSION" \
  --root "$STAGE/vst3_root" \
  --scripts "$SCRIPTS_VST3" \
  --install-location "/" \
  "$PKGDIR/vst3.pkg"

if [[ -d "$AU_ROOT/$NAME.component" ]]; then
  pkgbuild \
    --identifier "${BUNDLE_ID}.au" \
    --version "$VERSION" \
    --root "$STAGE/au_root" \
    --scripts "$SCRIPTS_AU" \
    --install-location "/" \
    "$PKGDIR/au.pkg"
fi

# Distribution XML — defines the installer wizard screens.
DIST="$STAGE/distribution.xml"
cat > "$DIST" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$NAME $VERSION</title>
    <welcome    file="welcome.html"    mime-type="text/html"/>
    <license    file="license.txt"     mime-type="text/plain"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options    customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="vst3"/>
$( [[ -f "$PKGDIR/au.pkg" ]] && echo '        <line choice="au"/>' )
    </choices-outline>
    <choice id="vst3" title="VST3 Plug-in" visible="true" start_selected="true">
        <pkg-ref id="${BUNDLE_ID}.vst3"/>
    </choice>
$( [[ -f "$PKGDIR/au.pkg" ]] && cat <<CH
    <choice id="au" title="Audio Unit (for Logic Pro)" visible="true" start_selected="true">
        <pkg-ref id="${BUNDLE_ID}.au"/>
    </choice>
CH
)
    <pkg-ref id="${BUNDLE_ID}.vst3" version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
$( [[ -f "$PKGDIR/au.pkg" ]] && echo "    <pkg-ref id=\"${BUNDLE_ID}.au\" version=\"$VERSION\" onConclusion=\"none\">au.pkg</pkg-ref>" )
</installer-gui-script>
XML

RES="$STAGE/resources"
mkdir -p "$RES"
cp "$REPO/installer/LICENSE.txt" "$RES/license.txt"

cat > "$RES/welcome.html" <<'HTML'
<html><body style="font-family: -apple-system, system-ui; padding: 20px;">
<h2 style="margin-top:0;">Welcome to RockGlue Architecture</h2>
<p>A zero-latency master-bus glue processor for rock mixes (FET drum smash, mono-locked low end, M/S pocket EQ, VCA glue).
This installer puts the plug-ins where every supported DAW looks for them:</p>
<ul>
  <li><b>VST3</b> &rarr; /Library/Audio/Plug-Ins/VST3/<br/>
      <small>Ableton, Reaper, Cubase, Studio One, Bitwig, FL Studio&hellip;</small></li>
  <li><b>Audio Unit</b> &rarr; /Library/Audio/Plug-Ins/Components/<br/>
      <small>Logic Pro, GarageBand, MainStage</small></li>
</ul>
<p>Quit your DAW before continuing, then rescan plug-ins after the install.</p>
</body></html>
HTML

cat > "$RES/conclusion.html" <<'HTML'
<html><body style="font-family: -apple-system, system-ui; padding: 20px;">
<h2 style="margin-top:0;">Installation complete.</h2>
<p>Open your DAW and rescan plug-ins:</p>
<ul>
  <li><b>Logic Pro</b> &rarr; detects the new Audio Unit on next launch;
      it appears under Audio Units &rarr; HumHouse.</li>
  <li><b>Ableton</b> &rarr; Preferences &rarr; Plug-Ins &rarr; Rescan.</li>
  <li><b>Reaper</b> &rarr; Preferences &rarr; VST &rarr; Re-scan.</li>
</ul>
<p>Insert it on your master bus, set the Glue Threshold for 2-4 dB of
gain reduction, and dial in Drive, Grit and Carve Pocket to taste.</p>
</body></html>
HTML

UNSIGNED_PKG="$STAGE/RockGlue-Architecture-unsigned.pkg"
productbuild \
  --distribution "$DIST" \
  --resources "$RES" \
  --package-path "$PKGDIR" \
  --version "$VERSION" \
  "$UNSIGNED_PKG"

# Sign the flat pkg when an installer identity is available; otherwise ship it
# unsigned (the user gets the usual right-click-to-open Gatekeeper prompt).
if [[ -n "${APPLE_INSTALLER_ID:-}" ]]; then
  productsign --sign "$APPLE_INSTALLER_ID" "$UNSIGNED_PKG" "$PKG_OUT"
else
  cp "$UNSIGNED_PKG" "$PKG_OUT"
fi

echo "Built: $PKG_OUT"
ls -la "$PKG_OUT"
