#!/bin/bash
# Langstone V3H — GitHub Upgrade Script v2
# Repo: https://github.com/jacintomfr/Langstone-JR
# Progresso: /tmp/langstone_upgrade_progress
# Log detalhado: /home/pi/Langstone/upgrade.log

REPO_URL="https://github.com/jacintomfr/Langstone-JR"
INSTALL_DIR="/home/pi/Langstone"
CLONE_DIR="/tmp/Langstone-JR-upgrade"
BACKUP_DIR="/home/pi/Langstone/backup_$(date +%Y%m%d_%H%M%S)"
LOG="$INSTALL_DIR/upgrade.log"
PROGRESS="/tmp/langstone_upgrade_progress"
VER_FILE="$INSTALL_DIR/version.h"

# ── Output helpers ───────────────────────────────────────────────────────────
log()  { echo "[$(date '+%H:%M:%S')] $1" >> "$LOG"; }
msg()  { echo "MSG:$1" > "$PROGRESS"; log "$1"; }
ok()   { echo "OK:$1"  > "$PROGRESS"; log "OK: $1"; }
err()  { echo "ERR:$1" > "$PROGRESS"; log "ERROR: $1"; }

UPGRADE_FILES=(
    "LangstoneGUI_Hack.c" "LangstoneGUI_Pluto.c"
    "Lang_TRX_Hack.py"    "Lang_TRX_Pluto.py"
    "ControlTRX_Hack.py"  "ControlTRX_Pluto.py"
    "langstone_upgrade_git.sh"
    "run" "stop"
    # NOTE: "build" and "version.h" are intentionally excluded
    # build: has auto-increment logic — never overwrite with repo version
    # version.h: only used for comparison — build auto-updates it
)
BUILD_TARGETS=("GUI_Hack" "GUI_Pluto")

# ── Init log ─────────────────────────────────────────────────────────────────
mkdir -p "$INSTALL_DIR"
echo "" >> "$LOG"
echo "════════════════════════════════════════" >> "$LOG"
echo "Upgrade started: $(date)" >> "$LOG"
echo "User: $(whoami), PWD: $(pwd)" >> "$LOG"
echo "Script: $0" >> "$LOG"
log "Repo: $REPO_URL"
echo "STARTED" > "$PROGRESS"

# ── STEP 1: Rede ─────────────────────────────────────────────────────────────
msg "1/7 A verificar rede..."
sleep 0.3
log "Testing: ping -c 1 -W 5 github.com"
PING_OUT=$(ping -c 1 -W 5 github.com 2>&1)
PING_EXIT=$?
log "ping exit=$PING_EXIT output=$PING_OUT"
if [ $PING_EXIT -ne 0 ]; then
    # Try alternate test
    log "ping failed, trying curl..."
    CURL_OUT=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 5 https://github.com 2>&1)
    log "curl http_code=$CURL_OUT"
    if [ "$CURL_OUT" != "200" ] && [ "$CURL_OUT" != "301" ] && [ "$CURL_OUT" != "302" ]; then
        err "Sem rede ping=$PING_EXIT curl=$CURL_OUT"
        exit 1
    fi
fi
ok "1/7 Rede OK"
sleep 0.3

# ── STEP 2: Versão remota ─────────────────────────────────────────────────────
msg "2/7 A verificar versao remota..."
sleep 0.3
REMOTE_VER_URL="https://raw.githubusercontent.com/jacintomfr/Langstone-JR/main/version.h"
# Cache-bust URL — prevents GitHub CDN from returning stale version
REMOTE_VER_URL_NOCACHE="${REMOTE_VER_URL}?$(date +%s)"
REMOTE_VER_FILE="/tmp/langstone_remote_version.h"
rm -f "$REMOTE_VER_FILE"

log "Fetching: $REMOTE_VER_URL"
if command -v curl > /dev/null 2>&1; then
    FETCH_OUT=$(curl -s -L -o "$REMOTE_VER_FILE" -w "%{http_code}" \
        -H "Cache-Control: no-cache" -H "Pragma: no-cache" \
        --connect-timeout 10 "$REMOTE_VER_URL_NOCACHE" 2>&1)
    log "curl fetch: http_code=$FETCH_OUT url=$REMOTE_VER_URL_NOCACHE"
elif command -v wget > /dev/null 2>&1; then
    FETCH_OUT=$(wget -q -O "$REMOTE_VER_FILE" "$REMOTE_VER_URL" 2>&1; echo $?)
    log "wget fetch: $FETCH_OUT"
else
    err "curl e wget nao encontrados"
    exit 1
fi

if [ ! -s "$REMOTE_VER_FILE" ]; then
    err "version.h remoto vazio ou nao obtido http=$FETCH_OUT"
    log "Content of $REMOTE_VER_FILE: $(cat $REMOTE_VER_FILE 2>/dev/null || echo 'empty')"
    exit 1
fi

log "Remote version.h content: $(cat $REMOTE_VER_FILE)"

LOCAL_VER=""
REMOTE_VER=""
[ -f "$VER_FILE" ] && LOCAL_VER=$(grep 'LANGSTONE_VERSION' "$VER_FILE" | grep -o '"V[^"]*"' | tr -d '"')
REMOTE_VER=$(grep 'LANGSTONE_VERSION' "$REMOTE_VER_FILE" | grep -o '"V[^"]*"' | tr -d '"')
log "Local version: '$LOCAL_VER'  Remote version: '$REMOTE_VER'"

if [ -z "$REMOTE_VER" ]; then
    err "Nao foi possivel ler versao remota"
    exit 1
fi

if [ "$LOCAL_VER" = "$REMOTE_VER" ]; then
    log "Already up to date: $LOCAL_VER"
    rm -f "$REMOTE_VER_FILE"
    echo "UP_TO_DATE:$LOCAL_VER" > "$PROGRESS"
    sleep 2
    exit 0
fi

# Only upgrade if remote is NEWER — compare build numbers (Vxx-yyy)
LOCAL_BUILD=$(echo "$LOCAL_VER" | grep -o '[0-9]*$' | sed 's/^0*//' )
REMOTE_BUILD=$(echo "$REMOTE_VER" | grep -o '[0-9]*$' | sed 's/^0*//')
LOCAL_MAJOR=$(echo "$LOCAL_VER" | grep -o 'V[0-9]*' | grep -o '[0-9]*')
REMOTE_MAJOR=$(echo "$REMOTE_VER" | grep -o 'V[0-9]*' | grep -o '[0-9]*')
# Remove leading zeros to ensure numeric comparison
LOCAL_BUILD=$((10#${LOCAL_BUILD:-0}))
REMOTE_BUILD=$((10#${REMOTE_BUILD:-0}))
LOCAL_MAJOR=$((10#${LOCAL_MAJOR:-0}))
REMOTE_MAJOR=$((10#${REMOTE_MAJOR:-0}))
log "Local: major=$LOCAL_MAJOR build=$LOCAL_BUILD  Remote: major=$REMOTE_MAJOR build=$REMOTE_BUILD"

# Check if remote is newer
IS_NEWER=0
if [ "$REMOTE_MAJOR" -gt "$LOCAL_MAJOR" ] 2>/dev/null; then IS_NEWER=1
elif [ "$REMOTE_MAJOR" -eq "$LOCAL_MAJOR" ] && [ "$REMOTE_BUILD" -gt "$LOCAL_BUILD" ] 2>/dev/null; then IS_NEWER=1
fi

if [ $IS_NEWER -eq 0 ]; then
    log "Remote version not newer — no upgrade needed"
    rm -f "$REMOTE_VER_FILE"
    echo "UP_TO_DATE:$LOCAL_VER" > "$PROGRESS"
    sleep 2
    exit 0
fi

ok "2/7 $LOCAL_VER -> $REMOTE_VER"
sleep 0.5

# ── STEP 3: Clone ─────────────────────────────────────────────────────────────
msg "3/7 A obter de GitHub..."
rm -rf "$CLONE_DIR"
log "git clone --depth=1 $REPO_URL $CLONE_DIR"
GIT_OUT=$(timeout 120 git clone --depth=1 "$REPO_URL" "$CLONE_DIR" 2>&1)
GIT_EXIT=$?
log "git exit=$GIT_EXIT"
log "git output: $GIT_OUT"
if [ $GIT_EXIT -ne 0 ]; then
    err "git clone falhou exit=$GIT_EXIT: $GIT_OUT"
    rm -rf "$CLONE_DIR"
    exit 1
fi
log "Cloned files: $(ls $CLONE_DIR 2>/dev/null)"
ok "3/7 Repo OK $REMOTE_VER"
sleep 0.3

# ── STEP 4: Backup ────────────────────────────────────────────────────────────
msg "4/7 A criar backup..."
mkdir -p "$BACKUP_DIR"
log "Backup dir: $BACKUP_DIR"
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$INSTALL_DIR/$f" ]; then
        cp "$INSTALL_DIR/$f" "$BACKUP_DIR/" 2>> "$LOG"
        log "  backed up: $f"
    fi
done
for t in "${BUILD_TARGETS[@]}"; do
    [ -f "$INSTALL_DIR/$t" ] && cp "$INSTALL_DIR/$t" "$BACKUP_DIR/" 2>> "$LOG"
done
ok "4/7 Backup OK"
sleep 0.3

# ── STEP 5: Instalar ──────────────────────────────────────────────────────────
msg "5/7 A instalar ficheiros..."
FAIL=0
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$CLONE_DIR/$f" ]; then
        if ! cp "$CLONE_DIR/$f" "$INSTALL_DIR/$f" 2>> "$LOG"; then
            err "Falha ao copiar $f"
            log "Restoring backup..."
            for rf in "${UPGRADE_FILES[@]}"; do
                [ -f "$BACKUP_DIR/$rf" ] && cp "$BACKUP_DIR/$rf" "$INSTALL_DIR/$rf"
            done
            rm -rf "$CLONE_DIR"
            exit 1
        fi
        log "  installed: $f"
    else
        log "  skip (not in repo): $f"
    fi
done
chmod +x "$INSTALL_DIR/build" "$INSTALL_DIR/run" "$INSTALL_DIR/stop" \
         "$INSTALL_DIR/langstone_upgrade_git.sh" 2>/dev/null
ok "5/7 Ficheiros instalados"
sleep 0.3

# ── STEP 6: Build ─────────────────────────────────────────────────────────────
msg "6/7 A compilar..."
cd "$INSTALL_DIR"

# Critical fix: write version.h with REMOTE_VER-1 so that ./build increments
# to exactly REMOTE_VER. Without this, build consumes the remote version number
# and the next upgrade check sees LOCAL==REMOTE and does nothing.
REMOTE_MAJOR_N=$(echo "$REMOTE_VER" | sed 's/V0*//;s/-.*//')
REMOTE_BUILD_N=$(echo "$REMOTE_VER" | grep -o '[0-9]*$' | sed 's/^0*//')
[ -z "$REMOTE_BUILD_N" ] && REMOTE_BUILD_N=0
PRE_BUILD=$((REMOTE_BUILD_N - 1))
[ $PRE_BUILD -lt 0 ] && PRE_BUILD=0
PRE_VER=$(printf "V%02d-%03d" $REMOTE_MAJOR_N $PRE_BUILD)
cat > "$VER_FILE" << VEOF
#ifndef LANGSTONE_VERSION_H
#define LANGSTONE_VERSION_H
#define LANGSTONE_VER_MAJOR  $REMOTE_MAJOR_N
#define LANGSTONE_VER_BUILD  $PRE_BUILD
#define LANGSTONE_VERSION    "$PRE_VER"
#endif
VEOF
log "version.h set to $PRE_VER before build — build will produce $REMOTE_VER"

log "Running: bash ./build"
BUILD_OUT=$(bash ./build 2>&1)
BUILD_EXIT=$?
log "build exit=$BUILD_EXIT"
log "build output:"
echo "$BUILD_OUT" >> "$LOG"
VER_AFTER_BUILD=$(grep 'LANGSTONE_VERSION' "$VER_FILE" 2>/dev/null | grep -o '"V[^"]*"' | tr -d '"')
log "version.h after build: $VER_AFTER_BUILD (expected $REMOTE_VER)"
if [ "$VER_AFTER_BUILD" != "$REMOTE_VER" ]; then
    log "WARNING: version mismatch after build — forcing correct version"
fi
if [ $BUILD_EXIT -ne 0 ]; then
    err "Build falhou - a restaurar backup"
    log "Build errors: $BUILD_OUT"
    for f in "${UPGRADE_FILES[@]}"; do
        [ -f "$BACKUP_DIR/$f" ] && cp "$BACKUP_DIR/$f" "$INSTALL_DIR/$f"
    done
    for t in "${BUILD_TARGETS[@]}"; do
        [ -f "$BACKUP_DIR/$t" ] && cp "$BACKUP_DIR/$t" "$INSTALL_DIR/$t"
    done
    rm -rf "$CLONE_DIR"
    exit 1
fi
ok "6/7 Build OK"
sleep 0.3

# ── STEP 7: Cleanup ───────────────────────────────────────────────────────────
msg "7/7 A limpar..."
rm -rf "$CLONE_DIR"
rm -f "$REMOTE_VER_FILE"
ls -dt "$INSTALL_DIR"/backup_* 2>/dev/null | tail -n +4 | xargs rm -rf 2>/dev/null
ok "7/7 Concluido"
sleep 0.2

# Rewrite version.h completely — more reliable than sed with version strings
REMOTE_MAJOR_N=$(echo "$REMOTE_VER" | sed 's/V0*//;s/-.*//')
REMOTE_BUILD_N=$(echo "$REMOTE_VER" | grep -o '[0-9]*$' | sed 's/^0*//')
[ -z "$REMOTE_BUILD_N" ] && REMOTE_BUILD_N=0
cat > "$VER_FILE" << VEOF
#ifndef LANGSTONE_VERSION_H
#define LANGSTONE_VERSION_H
#define LANGSTONE_VER_MAJOR  $REMOTE_MAJOR_N
#define LANGSTONE_VER_BUILD  $REMOTE_BUILD_N
#define LANGSTONE_VERSION    "$REMOTE_VER"
#endif
VEOF
# Verify write succeeded
WRITTEN=$(grep 'LANGSTONE_VERSION' "$VER_FILE" 2>/dev/null | grep -o '"V[^"]*"' | tr -d '"')
log "version.h rewritten: expected=$REMOTE_VER written=$WRITTEN"
if [ "$WRITTEN" != "$REMOTE_VER" ]; then
    err "version.h write failed: got '$WRITTEN' expected '$REMOTE_VER'"
    exit 1
fi

echo "SUCCESS:$REMOTE_VER" > "$PROGRESS"
log "Upgrade completo: $LOCAL_VER -> $REMOTE_VER"
log "Log saved to $LOG"
exit 0
