#!/bin/bash
# Langstone V3H — GitHub Upgrade Script
# Repo: https://github.com/jacintomfr/Langstone-JR
# Progresso em /tmp/langstone_upgrade_progress (lido pelo GUI)
# Exit: 0=sucesso/actualizado, 1=falha (backup restaurado)

REPO_URL="https://github.com/jacintomfr/Langstone-JR"
INSTALL_DIR="/home/pi/Langstone"
CLONE_DIR="/tmp/Langstone-JR-upgrade"
BACKUP_DIR="/home/pi/Langstone/backup_$(date +%Y%m%d_%H%M%S)"
LOG="$INSTALL_DIR/upgrade.log"
PROGRESS="/tmp/langstone_upgrade_progress"
VER_FILE="$INSTALL_DIR/version.h"

msg() { echo "MSG:$1" > "$PROGRESS"; echo "[$(date '+%H:%M:%S')] $1" >> "$LOG"; }
err() { echo "ERR:$1" > "$PROGRESS"; echo "[$(date '+%H:%M:%S')] ERROR: $1" >> "$LOG"; }
ok()  { echo "OK:$1"  > "$PROGRESS"; echo "[$(date '+%H:%M:%S')] OK: $1" >> "$LOG"; }

UPGRADE_FILES=(
    "LangstoneGUI_Hack.c" "LangstoneGUI_Pluto.c"
    "Lang_TRX_Hack.py"    "Lang_TRX_Pluto.py"
    "ControlTRX_Hack.py"  "ControlTRX_Pluto.py"
    "langstone_upgrade_git.sh" "version.h"
    "build" "run" "stop"
)
BUILD_TARGETS=("GUI_Hack" "GUI_Pluto")

echo "" >> "$LOG"
echo "=== Upgrade $(date) ===" >> "$LOG"
echo "STARTED" > "$PROGRESS"

# ── STEP 1: Rede ─────────────────────────────────────────────────────────────
msg "1/7 A verificar rede..."
sleep 0.3
if ! ping -c 1 -W 5 github.com > /dev/null 2>&1; then
    err "Sem rede — nao chegou ao github.com"
    exit 1
fi
ok "1/7 Rede OK"
sleep 0.3

# ── STEP 2: Verificar versão remota (só descarrega version.h) ────────────────
msg "2/7 A verificar versao remota..."
sleep 0.3
REMOTE_VER_URL="https://raw.githubusercontent.com/jacintomfr/Langstone-JR/main/version.h"
REMOTE_VER_FILE="/tmp/langstone_remote_version.h"

if ! timeout 15 curl -s -o "$REMOTE_VER_FILE" "$REMOTE_VER_URL" 2>> "$LOG"; then
    # curl failed — try wget
    if ! timeout 15 wget -q -O "$REMOTE_VER_FILE" "$REMOTE_VER_URL" 2>> "$LOG"; then
        err "Nao foi possivel obter versao remota"
        exit 1
    fi
fi

# Extract versions
LOCAL_VER=""
REMOTE_VER=""
[ -f "$VER_FILE" ] && LOCAL_VER=$(grep 'LANGSTONE_VERSION' "$VER_FILE" | grep -o '"V[^"]*"' | tr -d '"')
[ -f "$REMOTE_VER_FILE" ] && REMOTE_VER=$(grep 'LANGSTONE_VERSION' "$REMOTE_VER_FILE" | grep -o '"V[^"]*"' | tr -d '"')

echo "Local: $LOCAL_VER  Remote: $REMOTE_VER" >> "$LOG"

if [ -z "$REMOTE_VER" ]; then
    err "Nao foi possivel ler versao remota"
    rm -f "$REMOTE_VER_FILE"
    exit 1
fi

if [ "$LOCAL_VER" = "$REMOTE_VER" ]; then
    echo "UP_TO_DATE:$LOCAL_VER" > "$PROGRESS"
    echo "[$(date '+%H:%M:%S')] Already up to date: $LOCAL_VER" >> "$LOG"
    rm -f "$REMOTE_VER_FILE"
    exit 0
fi

ok "2/7 Nova versao: $LOCAL_VER -> $REMOTE_VER"
sleep 0.5

# ── STEP 3: Clone repo ───────────────────────────────────────────────────────
msg "3/7 A obter de GitHub ($REMOTE_VER)..."
rm -rf "$CLONE_DIR"
if ! timeout 120 git clone --depth=1 "$REPO_URL" "$CLONE_DIR" >> "$LOG" 2>&1; then
    err "git clone falhou — ver $LOG"
    rm -rf "$CLONE_DIR"
    exit 1
fi
ok "3/7 Repo OK"
sleep 0.3

# ── STEP 4: Backup ───────────────────────────────────────────────────────────
msg "4/7 A criar backup ($LOCAL_VER)..."
mkdir -p "$BACKUP_DIR"
for f in "${UPGRADE_FILES[@]}"; do
    [ -f "$INSTALL_DIR/$f" ] && cp "$INSTALL_DIR/$f" "$BACKUP_DIR/" 2>> "$LOG"
done
for t in "${BUILD_TARGETS[@]}"; do
    [ -f "$INSTALL_DIR/$t" ] && cp "$INSTALL_DIR/$t" "$BACKUP_DIR/" 2>> "$LOG"
done
ok "4/7 Backup guardado"
sleep 0.3

# ── STEP 5: Instalar ─────────────────────────────────────────────────────────
msg "5/7 A instalar ficheiros..."
FAIL=0
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$CLONE_DIR/$f" ]; then
        if ! cp "$CLONE_DIR/$f" "$INSTALL_DIR/$f" 2>> "$LOG"; then
            err "Falha ao copiar $f — a restaurar backup"
            for rf in "${UPGRADE_FILES[@]}"; do
                [ -f "$BACKUP_DIR/$rf" ] && cp "$BACKUP_DIR/$rf" "$INSTALL_DIR/$rf"
            done
            rm -rf "$CLONE_DIR"; exit 1
        fi
        echo "  Instalado: $f" >> "$LOG"
    fi
done
chmod +x "$INSTALL_DIR/build" "$INSTALL_DIR/run" "$INSTALL_DIR/stop" \
         "$INSTALL_DIR/langstone_upgrade_git.sh" 2>/dev/null
ok "5/7 Ficheiros instalados"
sleep 0.3

# ── STEP 6: Build ─────────────────────────────────────────────────────────────
msg "6/7 A compilar..."
cd "$INSTALL_DIR"

# Temporarily set build number to remote without incrementing
# (the repo version.h already has the correct version)
if ! bash ./build >> "$LOG" 2>&1; then
    err "Build falhou — a restaurar backup"
    for f in "${UPGRADE_FILES[@]}"; do
        [ -f "$BACKUP_DIR/$f" ] && cp "$BACKUP_DIR/$f" "$INSTALL_DIR/$f"
    done
    for t in "${BUILD_TARGETS[@]}"; do
        [ -f "$BACKUP_DIR/$t" ] && cp "$BACKUP_DIR/$t" "$INSTALL_DIR/$t"
    done
    rm -rf "$CLONE_DIR"; exit 1
fi
ok "6/7 Build OK"
sleep 0.3

# ── STEP 7: Cleanup ──────────────────────────────────────────────────────────
msg "7/7 A limpar..."
rm -rf "$CLONE_DIR"
rm -f "$REMOTE_VER_FILE"
# Keep only last 3 backups
ls -dt "$INSTALL_DIR"/backup_* 2>/dev/null | tail -n +4 | xargs rm -rf 2>/dev/null
ok "7/7 Concluido"
sleep 0.2

echo "SUCCESS:$REMOTE_VER" > "$PROGRESS"
echo "[$(date '+%H:%M:%S')] Upgrade completo: $LOCAL_VER -> $REMOTE_VER" >> "$LOG"
exit 0
