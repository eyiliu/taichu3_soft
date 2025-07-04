#!/usr/bin/env bash
 
## kill all the remaining EUDAQ2 programs.
killall -q euCliProducer
killall -q euCliCollector
killall -q StdEventMonitor
killall -q euRun
 
BIN_PATH=/home/HEPuser/WorkSpace/DAQSoftware/taichu3_soft/INSTALL/bin 

echo "BIN_PATH is set to ${BIN_PATH}"

xterm -T "RunControl" -e "${BIN_PATH}/euRun" &
sleep 2

xterm -T "Taichu" -e "${BIN_PATH}/euCliProducer -n TaichuProducer -t taichu" &

#${BIN_PATH}/euCliProducer -n TaichuProducer -t taichu &


xterm -T "TID Sync DataCollector" -e "${BIN_PATH}/euCliCollector -n TriggerIDSyncDataCollector -t dc" &
# xterm -T "StdEvent Monitor" -e "${BIN_PATH}/StdEventMonitor -t StdEventMonitor" &
# xterm -T "AIDA TLU" -e "${BIN_PATH}/euCliProducer -n AidaTluProducer -t aida_tlu" &
# xterm -T "taichu telescope" -e "${BIN_PATH}/euCliProducer -n TaichuProducer -t taichu" &

${BIN_PATH}/StdEventMonitor -t StdEventMonitor &


