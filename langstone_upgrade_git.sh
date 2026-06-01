#!/bin/bash
# ══════════════════════════════════════════════════════════════════════════════
#  Langstone V3H — GitHub Auto-Upgrade Script
#  Repo: http://www.github.com/jacintomfr/Langstone-JR
#  Usage: ./langstone_upgrade_git.sh [--gui]  (--gui = output for GUI display)
#  Returns: 0=success, 1=failure (backup restored)
# ══════════════════════════════════════════════════════════════════════════════

REPO_URL="http://www.github.com/jacintomfr/Langstone-JR"
INSTALL_DIR="/home/pi/Langstone"
CLONE_DIR="/tmp/Langstone-JR-upgrade"
BACKUP_DIR="/home/pi/Langstone/backup_$(date +%Y%m%d_%H%M%S)"
LOG="$INSTALL_DIR/upgrade.log"
GUI_MODE=0
[ "$1" = "--gui" ] && GUI_MODE=1

# ── Output helpers ────────────────────────────────────────────────────────────
msg() {
    echo "[$(date '+%H:%M:%S')] $1" | tee -a "$LOG"
    [ $GUI_MODE -eq 1 ] && echo "MSG:$1"
}
err() {
    echo "[$(date '+%H:%M:%S')] ERROR: $1" | tee -a "$LOG"
    [ $GUI_MODE -eq 1 ] && echo "ERR:$1"
}
ok() {
    echo "[$(date '+%H:%M:%S')] OK: $1" | tee -a "$LOG"
    [ $GUI_MODE -eq 1 ] && echo "OK:$1"
}

# ── Files to upgrade ──────────────────────────────────────────────────────────
UPGRADE_FILES=(
    "LangstoneGUI_Hack.c"
    "LangstoneGUI_Pluto.c"
    "Lang_TRX_Hack.py"
    "Lang_TRX_Pluto.py"
    "ControlTRX_Hack.py"
    "ControlTRX_Pluto.py"
    "build"
    "run"
    "stop"
)

# ── Build targets ─────────────────────────────────────────────────────────────
BUILD_TARGETS=(
    "GUI_Hack"
    "GUI_Pluto"
)

echo "" >> "$LOG"
echo "════════════════════════════════════════" >> "$LOG"
msg "Langstone GitHub upgrade started"
msg "Repo: $REPO_URL"

# ── STEP 1: Check network ─────────────────────────────────────────────────────
msg "Step 1/6: Checking network..."
if ! ping -c 1 -W 5 github.com > /dev/null 2>&1; then
    err "No network — cannot reach github.com"
    err "Check WiFi/Ethernet and try again"
    exit 1
fi
ok "Network OK"

# ── STEP 2: Clone repo ───────────────────────────────────────────────────────
msg "Step 2/6: Fetching from GitHub..."
rm -rf "$CLONE_DIR"
if ! timeout 120 git clone --depth=1 "$REPO_URL" "$CLONE_DIR" >> "$LOG" 2>&1; then
    err "git clone failed — check repo URL and network"
    rm -rf "$CLONE_DIR"
    exit 1
fi

# Verify key files exist in clone
for f in "${UPGRADE_FILES[@]}"; do
    if [ ! -f "$CLONE_DIR/$f" ]; then
        msg "Note: $f not found in repo (skipping)"
    fi
done
ok "Repo cloned OK"

# ── STEP 3: Check for changes ─────────────────────────────────────────────────
msg "Step 3/6: Checking for updates..."
CHANGED=0
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$CLONE_DIR/$f" ]; then
        if [ ! -f "$INSTALL_DIR/$f" ] || ! diff -q "$CLONE_DIR/$f" "$INSTALL_DIR/$f" > /dev/null 2>&1; then
            msg "  Updated: $f"
            CHANGED=$((CHANGED+1))
        fi
    fi
done

if [ $CHANGED -eq 0 ]; then
    ok "Already up to date — no changes"
    rm -rf "$CLONE_DIR"
    exit 0
fi
msg "$CHANGED file(s) will be updated"

# ── STEP 4: Backup current installation ───────────────────────────────────────
msg "Step 4/6: Creating backup..."
mkdir -p "$BACKUP_DIR"
BACKUP_OK=1
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$INSTALL_DIR/$f" ]; then
        if ! cp "$INSTALL_DIR/$f" "$BACKUP_DIR/$f"; then
            err "Backup failed for $f"
            BACKUP_OK=0
        fi
    fi
done
# Also backup binaries
for t in "${BUILD_TARGETS[@]}"; do
    [ -f "$INSTALL_DIR/$t" ] && cp "$INSTALL_DIR/$t" "$BACKUP_DIR/$t"
done

if [ $BACKUP_OK -eq 0 ]; then
    err "Backup incomplete — aborting for safety"
    rm -rf "$CLONE_DIR"
    exit 1
fi
ok "Backup saved to $BACKUP_DIR"

# ── STEP 5: Install new files ─────────────────────────────────────────────────
msg "Step 5/6: Installing new files..."
INSTALL_FAIL=0
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$CLONE_DIR/$f" ]; then
        if ! cp "$CLONE_DIR/$f" "$INSTALL_DIR/$f"; then
            err "Failed to copy $f"
            INSTALL_FAIL=1
        else
            msg "  Installed: $f"
        fi
    fi
done
chmod +x "$INSTALL_DIR/build" "$INSTALL_DIR/run" "$INSTALL_DIR/stop" 2>/dev/null

if [ $INSTALL_FAIL -eq 1 ]; then
    err "Install failed — restoring backup..."
    for f in "${UPGRADE_FILES[@]}"; do
        [ -f "$BACKUP_DIR/$f" ] && cp "$BACKUP_DIR/$f" "$INSTALL_DIR/$f"
    done
    err "Backup restored — system unchanged"
    rm -rf "$CLONE_DIR"
    exit 1
fi
ok "Files installed"

# ── STEP 6: Build ─────────────────────────────────────────────────────────────
msg "Step 6/6: Building..."
cd "$INSTALL_DIR"
BUILD_OUTPUT=$(bash ./build 2>&1)
BUILD_EXIT=$?
echo "$BUILD_OUTPUT" >> "$LOG"

if [ $BUILD_EXIT -ne 0 ]; then
    err "Build FAILED:"
    err "$BUILD_OUTPUT"
    msg "Restoring backup..."
    for f in "${UPGRADE_FILES[@]}"; do
        [ -f "$BACKUP_DIR/$f" ] && cp "$BACKUP_DIR/$f" "$INSTALL_DIR/$f"
    done
    for t in "${BUILD_TARGETS[@]}"; do
        [ -f "$BACKUP_DIR/$t" ] && cp "$BACKUP_DIR/$t" "$INSTALL_DIR/$t"
    done
    err "Backup restored — system unchanged"
    rm -rf "$CLONE_DIR"
    exit 1
fi
ok "Build successful"

# ── Cleanup ───────────────────────────────────────────────────────────────────
rm -rf "$CLONE_DIR"

# Keep only last 3 backups
ls -dt "$INSTALL_DIR"/backup_* 2>/dev/null | tail -n +4 | xargs rm -rf 2>/dev/null

ok "Upgrade complete — restart Langstone to use new version"
msg "Log saved to $LOG"
exit 0
