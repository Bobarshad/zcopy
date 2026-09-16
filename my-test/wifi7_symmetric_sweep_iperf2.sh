#!/bin/sh
set -e

# --- Network & Hardware Configuration ---
R2_IP="192.168.10.192"          # R2 LAN IP
R1_WLAN="ath21"                 # R1 master 6 GHz AP interface
R2_WLAN="ath2"                  # R2 6 GHz client station interface
R2_MAC="00:05:9e:99:c7:ce"      # Station MAC of R2
LOG_FILE="/tmp/symmetric_txpower_sweep_$(date +%s).csv"
DURATION=10                     # Test duration per step (seconds)

cleanup() {
    echo -e "\n[*] Cleaning up background tasks..."
    ssh root@"$R2_IP" "killall -q iperf" 2>/dev/null || true
    killall -q iperf 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# CSV Header
echo "timestamp,target_pwr_dbm,r1_tx_rate,r1_signal_dbm,r2_rx_rate,r2_tx_rate,r2_signal_dbm,tput_mbps" > "$LOG_FILE"

echo "[*] Checking connectivity to R2 ($R2_IP)..."
ssh root@"$R2_IP" "true" || {
    echo "[!] Error: Cannot reach R2 passwordless via SSH."
    exit 1
}

echo "[*] Ensuring iperf server is running on R2..."
ssh root@"$R2_IP" "killall -q iperf 2>/dev/null || true; iperf -s -D"

# Read currently active target power from UCI on R1
PWR=$(uci -q get wireless.wifi2.txpower || echo "auto")

echo "=================================================="
echo " Running benchmark at active TX power: ${PWR} dBm"
echo "=================================================="

# 1. Run iperf 2 client in background (-f m: Mbps, -y C: CSV format)
iperf -c "$R2_IP" -t "$DURATION" -f m -y C > /tmp/iperf_run.csv 2>&1 &
IPERF_PID=$!

# Sample midway through transmission
sleep $((DURATION / 2))

# 2. Collect PHY metrics from R1 AP
R1_STA=$(iw dev "$R1_WLAN" station get "$R2_MAC" 2>/dev/null)
R1_RATE=$(echo "$R1_STA" | awk '/tx bitrate:/ {print $3" "$4}')
R1_SIG=$(echo "$R1_STA" | awk '/signal:/ {print $2}')

# 3. Collect PHY metrics from R2 STA (uses 'link' which works reliably on stations)
R2_LINK=$(ssh root@"$R2_IP" "iw dev '$R2_WLAN' link 2>/dev/null")
R2_SIG=$(echo "$R2_LINK" | awk '/signal:/ {print $2}')
R2_RX_RATE=$(echo "$R2_LINK" | awk '/rx bitrate:/ {print $3" "$4}')
R2_TX_RATE=$(echo "$R2_LINK" | awk '/tx bitrate:/ {print $3" "$4}')

wait "$IPERF_PID" 2>/dev/null || true

# 4. Extract throughput (iperf 2 CSV column 9 is raw bits/sec)
TPUT_BPS=$(tail -n 1 /tmp/iperf_run.csv 2>/dev/null | awk -F',' '{print $9}')
TPUT_MBPS=$(awk -v bps="$TPUT_BPS" 'BEGIN {printf "%.2f", (bps ? bps/1000000 : 0)}')

TS=$(date +%s)
echo "$TS,\"$PWR\",\"$R1_RATE\",${R1_SIG:-N/A},\"$R2_RX_RATE\",\"$R2_TX_RATE\",${R2_SIG:-N/A},$TPUT_MBPS" >> "$LOG_FILE"

printf "  - R1 AP  -> Signal: %s dBm | TX Rate: %s\n" "${R1_SIG:-N/A}" "$R1_RATE"
printf "  - R2 STA -> Signal: %s dBm | RX Rate: %s | TX Rate: %s\n" "${R2_SIG:-N/A}" "$R2_RX_RATE" "$R2_TX_RATE"
printf "  - Measured Throughput: %s Mbps\n" "$TPUT_MBPS"
echo "[*] Done. Result appended to: $LOG_FILE"
