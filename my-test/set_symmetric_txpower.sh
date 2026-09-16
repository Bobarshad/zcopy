#!/bin/sh
set -e

# --- Configuration ---
R2_IP="192.168.10.192"
R1_RADIO="wifi2"
R2_RADIO="wifi2"
RECONNECT_WAIT=12

# Usage check
if [ -z "$1" ]; then
    echo "Usage: $0 <txpower_in_dBm>"
    echo "Example: $0 10"
    exit 1
fi

PWR="$1"

echo "=================================================="
echo " Applying Symmetric TX Power: ${PWR} dBm"
echo "=================================================="

# 1. Update and commit UCI on R1
echo "[*] Configuring R1 (${R1_RADIO}) -> ${PWR} dBm..."
uci set wireless."${R1_RADIO}".txpower="$PWR"
uci commit wireless

# 2. Update, commit, and reload UCI on R2 over SSH
echo "[*] Configuring and reloading R2 (${R2_RADIO}) over SSH..."
ssh root@"$R2_IP" \
    "uci set wireless.${R2_RADIO}.txpower='$PWR' && uci commit wireless && wifi reload"

# 3. Reload R1 local wireless interface
echo "[*] Reloading R1 wireless subsystem..."
wifi reload

# 4. Wait for link re-establishment
echo "[*] Waiting ${RECONNECT_WAIT}s for Wi-Fi 7 (EHT320) link to re-associate..."
sleep "$RECONNECT_WAIT"

# 5. Verification
echo "[*] Checking link status..."
R1_SIG=$(iw dev ath21 station get 00:05:9e:99:c7:ce 2>/dev/null | awk '/signal:/ {print $2}')
R2_SIG=$(ssh root@"$R2_IP" "iw dev ath2 link 2>/dev/null" | awk '/signal:/ {print $2}')

echo "--------------------------------------------------"
echo " Resulting Link Signal Levels:"
echo "   R1 AP  (Rx from R2): ${R1_SIG:-N/A} dBm"
echo "   R2 STA (Rx from R1): ${R2_SIG:-N/A} dBm"
echo "=================================================="
