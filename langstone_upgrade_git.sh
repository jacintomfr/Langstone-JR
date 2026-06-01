#!/bin/bash
# Langstone V3H — GitHub Upgrade Script
# Repo: http://www.github.com/jacintomfr/Langstone-JR
# Escreve progresso em /tmp/langstone_upgrade_progress (lido pelo GUI)
# Exit 0=sucesso, 1=falha (backup restaurado)

REPO_URL="http://www.github.com/jacintomfr/Langstone-JR"
INSTALL_DIR="/home/pi/Langstone"
CLONE_DIR="/tmp/Langstone-JR-upgrade"
BACKUP_DIR="/home/pi/Langstone/backup_$(date +%Y%m%d_%H%M%S)"
LOG="$INSTALL_DIR/upgrade.log"
PROGRESS="/tmp/langstone_upgrade_progress"

# Escreve msg no ficheiro de progresso E no log
msg() { echo "MSG:$1" > "$PROGRESS"; echo "[$(date '+%H:%M:%S')] $1" >> "$LOG"; }
err() { echo "ERR:$1" > "$PROGRESS"; echo "[$(date '+%H:%M:%S')] ERROR: $1" >> "$LOG"; }
ok()  { echo "OK:$1"  > "$PROGRESS"; echo "[$(date '+%H:%M:%S')] OK: $1" >> "$LOG"; }

UPGRADE_FILES=(
    "LangstoneGUI_Hack.c" "LangstoneGUI_Pluto.c"
    "Lang_TRX_Hack.py"    "Lang_TRX_Pluto.py"
    "ControlTRX_Hack.py"  "ControlTRX_Pluto.py"
    "langstone_upgrade_git.sh"
    "build" "run" "stop"
)
BUILD_TARGETS=("GUI_Hack" "GUI_Pluto")

echo "" >> "$LOG"
echo "=== Upgrade $(date) ===" >> "$LOG"
echo "STARTED" > "$PROGRESS"

# STEP 1: Rede
msg "1/6 A verificar rede..."
sleep 0.5
if ! ping -c 1 -W 5 github.com > /dev/null 2>&1; then
    err "Sem rede - nao chegou ao github.com"
    exit 1
fi
ok "1/6 Rede OK"
sleep 0.5

# STEP 2: Clone
msg "2/6 A obter de GitHub..."
rm -rf "$CLONE_DIR"
if ! timeout 120 git clone --depth=1 "$REPO_URL" "$CLONE_DIR" >> "$LOG" 2>&1; then
    err "git clone falhou - ver $LOG"
    rm -rf "$CLONE_DIR"
    exit 1
fi
ok "2/6 Repo OK"
sleep 0.3

# STEP 3: Verificar alteracoes
msg "3/6 A verificar alteracoes..."
CHANGED=0
for f in "${UPGRADE_FILES[@]}"; do
    [ -f "$CLONE_DIR/$f" ] || continue
    if [ ! -f "$INSTALL_DIR/$f" ] || ! diff -q "$CLONE_DIR/$f" "$INSTALL_DIR/$f" > /dev/null 2>&1; then
        CHANGED=$((CHANGED+1))
        echo "  Alterado: $f" >> "$LOG"
    fi
done
if [ $CHANGED -eq 0 ]; then
    echo "UP_TO_DATE" > "$PROGRESS"
    echo "[$(date '+%H:%M:%S')] Already up to date" >> "$LOG"
    rm -rf "$CLONE_DIR"
    exit 0
fi
ok "3/6 $CHANGED ficheiro(s) para actualizar"
sleep 0.3

# STEP 4: Backup
msg "4/6 A criar backup..."
mkdir -p "$BACKUP_DIR"
for f in "${UPGRADE_FILES[@]}"; do
    [ -f "$INSTALL_DIR/$f" ] && cp "$INSTALL_DIR/$f" "$BACKUP_DIR/" 2>> "$LOG"
done
for t in "${BUILD_TARGETS[@]}"; do
    [ -f "$INSTALL_DIR/$t" ] && cp "$INSTALL_DIR/$t" "$BACKUP_DIR/" 2>> "$LOG"
done
ok "4/6 Backup em backup_$(date +%Y%m%d_%H%M%S)"
sleep 0.3

# STEP 5: Instalar
msg "5/6 A instalar ficheiros..."
FAIL=0
for f in "${UPGRADE_FILES[@]}"; do
    if [ -f "$CLONE_DIR/$f" ]; then
        if ! cp "$CLONE_DIR/$f" "$INSTALL_DIR/$f"; then
            err "Falha ao copiar $f"
            FAIL=1; break
        fi
        echo "  Instalado: $f" >> "$LOG"
    fi
done
chmod +x "$INSTALL_DIR/build" "$INSTALL_DIR/run" "$INSTALL_DIR/stop" \
         "$INSTALL_DIR/langstone_upgrade_git.sh" 2>/dev/null
if [ $FAIL -eq 1 ]; then
    err "Instalacao falhou - a restaurar backup..."
    for f in "${UPGRADE_FILES[@]}"; do
        [ -f "$BACKUP_DIR/$f" ] && cp "$BACKUP_DIR/$f" "$INSTALL_DIR/$f"
    done
    rm -rf "$CLONE_DIR"; exit 1
fi
ok "5/6 Ficheiros instalados"
sleep 0.3

# STEP 6: Build
msg "6/6 A compilar..."
cd "$INSTALL_DIR"
if ! bash ./build >> "$LOG" 2>&1; then
    err "Build falhou - a restaurar backup..."
    for f in "${UPGRADE_FILES[@]}"; do
        [ -f "$BACKUP_DIR/$f" ] && cp "$BACKUP_DIR/$f" "$INSTALL_DIR/$f"
    done
    for t in "${BUILD_TARGETS[@]}"; do
        [ -f "$BACKUP_DIR/$t" ] && cp "$BACKUP_DIR/$t" "$INSTALL_DIR/$t"
    done
    rm -rf "$CLONE_DIR"; exit 1
fi
ok "6/6 Build OK"
sleep 0.3

# Limpa clones e backups antigos (guarda 3)
rm -rf "$CLONE_DIR"
ls -dt "$INSTALL_DIR"/backup_* 2>/dev/null | tail -n +4 | xargs rm -rf 2>/dev/null

echo "SUCCESS" > "$PROGRESS"
echo "[$(date '+%H:%M:%S')] Upgrade completo" >> "$LOG"
exit 0
