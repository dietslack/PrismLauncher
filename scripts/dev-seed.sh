#!/usr/bin/env bash
# Creates a throwaway data dir with sample instances for UI development/screenshots.
# Usage: scripts/dev-seed.sh <data-dir> [ApplicationTheme] [IconTheme]
set -euo pipefail
DIR="${1:?usage: dev-seed.sh <data-dir> [theme] [icon-theme]}"
THEME="${2:-reprism_dark}"
ICONS="${3:-flat_white}"

rm -rf "$DIR"
mkdir -p "$DIR/instances"

cat > "$DIR/reprismlauncher.cfg" <<EOF
Language=en_US
ApplicationTheme=$THEME
IconTheme=$ICONS
AutomaticJavaDownload=true
UserAskedAboutAutomaticJavaDownload=true
IgnoreJavaWizard=true
SelectedInstance=survival
EOF

# A fake account so the login wizard page is skipped. It cannot actually log in.
cat > "$DIR/accounts.json" <<'EOF'
{"formatVersion": 3, "accounts": [{"type": "MSA", "active": true, "msa-client-id": "",
  "ygg": {"token": "0", "iat": 0, "extra": {"userName": "Steve", "clientToken": "0"}},
  "profile": {"id": "00000000000000000000000000000000", "name": "Steve", "skin": {"id": "", "url": "", "variant": ""}, "capes": []},
  "entitlement": {"ownsMinecraft": true, "canPlayMinecraft": true}}]}
EOF

now=$(($(date +%s) * 1000))
make_instance() {  # id name icon mcver [loaderUid loaderVer] lastLaunchAgoHours playedSeconds [packType packId]
    local id="$1" name="$2" icon="$3" mc="$4" luid="$5" lver="$6" ago="$7" played="$8" ptype="${9:-}" pid="${10:-}"
    mkdir -p "$DIR/instances/$id/minecraft"
    cat > "$DIR/instances/$id/instance.cfg" <<EOF
[General]
InstanceType=OneSix
name=$name
iconKey=$icon
lastLaunchTime=$((now - ago * 3600000))
totalTimePlayed=$played
EOF
    if [ -n "$ptype" ]; then
        printf 'ManagedPack=true\nManagedPackType=%s\nManagedPackID=%s\nManagedPackName=%s\n' "$ptype" "$pid" "$name" >> "$DIR/instances/$id/instance.cfg"
    fi
    local loader=""
    [ -n "$luid" ] && loader=", {\"uid\": \"$luid\", \"version\": \"$lver\"}"
    cat > "$DIR/instances/$id/mmc-pack.json" <<EOF
{"formatVersion": 1, "components": [{"uid": "net.minecraft", "version": "$mc", "important": true}$loader]}
EOF
}

make_instance survival   "Survival World"     grass      1.21.4 net.fabricmc.fabric-loader 0.16.9 2    360000
make_instance create     "Fabulously Optimized" gear     1.21.4 net.fabricmc.fabric-loader 0.16.9 72 1250000 modrinth 1KVo5zza
make_instance skyblock   "RLCraft"            netherstar 1.12.2 net.minecraftforge 14.23.5.2860 240 50000 flame 285109
make_instance vanilla    "Vanilla 1.8.9"      steve      1.8.9  "" ""                          2000 9000
make_instance quilt      "Quilt Test"         quiltmc    1.21.1 org.quiltmc.quilt-loader 0.27.1 500  600
make_instance creative   "Creative Builds"    diamond    1.21.4 "" ""                          30   80000

cat > "$DIR/instgroups.json" <<'EOF'
{"formatVersion": "1", "groups": {
  "Modpacks": {"hidden": false, "instances": ["create", "skyblock"]},
  "Testing": {"hidden": false, "instances": ["quilt"]}}}
EOF
echo "Seeded $DIR"
