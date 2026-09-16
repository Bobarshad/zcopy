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

# Helper function: Map bitrate to Wi-Fi 7 EHT (320 MHz, 4-stream / single-stream base) MCS & QAM
map_rate_to_mcs() {
    local raw_rate="$1"
    echo "$raw_rate" | awk '{
        # Normalize to numeric Mbps
        rate = $0
        gsub(/[^0-9.]/, "", rate)
        r = rate + 0

        if (r == 0) {
            print "N/A"
        } else if (r >= 10500) {
            print "MCS 13 (4096-QAM, 5/6 coding)"
        } else if (r >= 9500) {
            print "MCS 12 (4096-QAM, 3/4 coding)"
        } else if (r >= 8500) {
            print "MCS 11 (1024-QAM, 5/6 coding)"
        } else if (r >= 7700) {
            print "MCS 10 (1024-QAM, 3/4 coding)"
        } else if (r >= 6900) {
            print "MCS 9 (256-QAM, 5/6 coding)"
        } else if (r >= 6000) {
            print "MCS 8 (256-QAM, 3/4 coding)"
        } else if (r >= 5000) {
            print "MCS 7 (64-QAM, 5/6 coding)"
        } else if (r >= 4000) {
            print "MCS 6 (64-QAM, 3/4 coding)"
        } else if (r >= 3000) {
            print "MCS 5 (64-QAM, 2/3 coding)"
        } else if (r >= 2200) {
            print "MCS 4 (16-QAM, 3/4 coding)"
        } else if (r >= 1500) {
            print "MCS 3 (16-QAM, 1/2 coding)"
        } else if (r >= 900) {
            print "MCS 2 (QPSK, 3/4 coding)"
        } else if (r >= 450) {
            print "MCS 1 (QPSK, 1/2 coding)"
        } else {
            print "MCS 0 (BPSK, 1/2 coding)"
        }
    }'
}

# CSV Header
echo "timestamp,target_pwr_dbm,r1_tx_rate,r1_mcs,r1_snr,r1_signal_dbm,r2_rx_rate,r2_rx_mcs,r2_tx_rate,r2_signal_dbm,tput_mbps" > "$LOG_FILE"

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

# Sample midway through active traffic transmission
sleep $((DURATION / 2))

# 2. Collect PHY metrics from R1 AP using wlanconfig and iw
R1_WCFG=$(wlanconfig "$R1_WLAN" list sta detail 2>/dev/null | grep -i -A 15 "$R2_MAC" || true)
R1_STA=$(iw dev "$R1_WLAN" station get "$R2_MAC" 2>/dev/null || true)

# Extract TXRATE and SNR from wlanconfig, fallback to iw if needed
R1_RATE=$(echo "$R1_WCFG" | awk 'NR==1 {print $4}')
[ -z "$R1_RATE" ] && R1_RATE=$(echo "$R1_STA" | awk '/tx bitrate:/ {print $3" "$4}')

R1_SNR=$(echo "$R1_WCFG" | awk -F': ' '/SNR/ {print $2}' | awk '{print $1}')
R1_SIG=$(echo "$R1_STA" | awk '/signal:/ {print $2}')

# Map R1 bitrate to MCS and Modulation
R1_MCS=$(map_rate_to_mcs "$R1_RATE")

# 3. Collect PHY metrics from R2 STA
R2_LINK=$(ssh root@"$R2_IP" "iw dev '$R2_WLAN' link 2>/dev/null" || true)
R2_SIG=$(echo "$R2_LINK" | awk '/signal:/ {print $2}')
R2_RX_RATE=$(echo "$R2_LINK" | awk '/rx bitrate:/ {print $3" "$4}')
R2_TX_RATE=$(echo "$R2_LINK" | awk '/tx bitrate:/ {print $3" "$4}')

# Map R2 received bitrate to MCS and Modulation
R2_RX_MCS=$(map_rate_to_mcs "$R2_RX_RATE")

wait "$IPERF_PID" 2>/dev/null || true

# 4. Extract throughput (iperf 2 CSV column 9 is raw bits/sec)
TPUT_BPS=$(tail -n 1 /tmp/iperf_run.csv 2>/dev/null | awk -F',' '{print $9}')
TPUT_MBPS=$(awk -v bps="$TPUT_BPS" 'BEGIN {printf "%.2f", (bps ? bps/1000000 : 0)}')

TS=$(date +%s)
echo "$TS,\"$PWR\",\"$R1_RATE\",\"$R1_MCS\",${R1_SNR:-N/A},${R1_SIG:-N/A},\"$R2_RX_RATE\",\"$R2_RX_MCS\",\"$R2_TX_RATE\",${R2_SIG:-N/A},$TPUT_MBPS" >> "$LOG_FILE"

printf "  - R1 AP TX Rate: %s | %s | SNR: %s dB | Signal: %s dBm\n" "$R1_RATE" "$R1_MCS" "${R1_SNR:-N/A}" "${R1_SIG:-N/A}"
printf "  - R2 STA RX Rate: %s | %s | Signal: %s dBm\n" "$R2_RX_RATE" "$R2_RX_MCS" "${R2_SIG:-N/A}"
printf "  - R2 STA TX Rate: %s\n" "$R2_TX_RATE"
printf "  - Measured Throughput: %s Mbps\n" "$TPUT_MBPS"
echo "[*] Done. Result appended to: $LOG_FILE"
