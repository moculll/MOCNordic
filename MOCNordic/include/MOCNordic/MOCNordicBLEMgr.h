#pragma once
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <bluetooth/hci_vs_sdc.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <vector>
#include <array>
#include <string>
#include <set>
#include <unordered_map>
#include <MOCNordic/MOCZephyrType.h>
namespace MOCNordic {

class MOCNordicBLEMgr {

public:
    MOCNordicBLEMgr() = delete;
    MOCNordicBLEMgr(MOCNordicBLEMgr &) = delete;
    MOCNordicBLEMgr(MOCNordicBLEMgr &&) = delete;

    

    static int BLEStackInit();
    static int setScanTarget(bt_addr_le_t *target);
    static int setScanTarget(uint8_t *target);
    static int setScanTarget(uint8_t index, uint8_t *target);

    static int removeScanTarget(bt_addr_le_t *target);
    static int removeScanTarget(uint8_t *target);
    static int removeScanTarget(uint8_t index);

    static int disconnectTarget(uint8_t index);

    static int setScanTarget(const std::string_view &generalName);

    

    static void printSequenceInfo();

    struct BLECallback {
        struct bt_conn_auth_cb auth_cb;
        struct bt_conn_auth_info_cb auth_info_cb;

        struct {
            struct cb_data cb_data;
            struct bt_scan_cb scan_cb;
        } scan_cb;
        

        struct bt_conn_cb conn_cb;
        struct bt_gatt_dm_cb dm_cb;
    };

    static void registerGetReportMapCallbackToIndex(unsigned int index, void (*callback)(uint8_t *data, uint32_t length))
    {
        if(index > PeripheralSequence.size() - 1)
            return;
        PeripheralSequence[index].getReportMapCallback = callback;
    }

    static void registerNotifyToIndex(unsigned int index, void (*callback)(uint8_t *data, uint32_t length))
    {
        if(index > PeripheralSequence.size() - 1)
            return;
        PeripheralSequence[index].getNotifyCallback = callback;
    }



    static void registerCharHandleReportIdMap(uint8_t index, uint16_t refHandle, uint8_t reportId)
    {
        if(index > PeripheralSequence.size() - 1)
            return;
        auto charHandle = PeripheralSequence[index].refHandleCharHandleMap.find(refHandle);
        if(charHandle != PeripheralSequence[index].refHandleCharHandleMap.end()) {
            PeripheralSequence[index].charHandleReportIdMap[charHandle->second] = reportId;
        }
    }

    static void registerRefHandleCharHandleMap(uint16_t index, uint16_t refHandle, uint16_t charHandle)
    {
        if(index > PeripheralSequence.size() - 1)
            return;
        PeripheralSequence[index].refHandleCharHandleMap[refHandle] = charHandle;
    }
private:
    struct NameAndAddr {
        char name_str[32];
        char addr[6];
        constexpr static uint32_t nameSize()
        {
            return sizeof(name_str);
        }

        constexpr static uint32_t addrSize()
        {
            return sizeof(addr);
        }
    };
    /* inline static MOCZephyr::ZMsgqControl<NameAndAddr, 16> scanRecvMsgqCtl; */
    
    inline static MOCZephyr::ZWorkControl<32> subscribeWorkCtl;
    inline static int currentWorkCnt = 0;
    struct PeripheralUnit {
        struct SubscribeParam {
            struct bt_gatt_subscribe_params subscribeParams;
            struct bt_gatt_discover_params discoverParams;
            
        };
        int64_t linkTimeMs;
        struct bt_gatt_read_params reportMapReadParams;
        uint8_t occupied;
        bt_addr_le_t targetMac;
        struct bt_conn *conn;
        std::array<SubscribeParam, 10> subscribeParams;
        uint8_t subscribed;
        uint8_t curSubIndex;

        std::array<uint8_t, 768> reportMap;


        std::unordered_map<uint16_t, uint8_t> charHandleReportIdMap;
        std::unordered_map<uint16_t, uint16_t> refHandleCharHandleMap;
        uint32_t reportMapLength;
        void (*getReportMapCallback)(uint8_t *data, uint32_t length);
        void (*getNotifyCallback)(uint8_t *data, uint32_t length);
        std::array<uint8_t, 768> &getReportMap()
        {
            return reportMap;
        }

        void resetReportMap()
        {
            reportMap.fill(0x00);
            reportMapLength = 0;
        }

        void reportMapInsert(const uint8_t *data, size_t length)
        {
            memcpy(reportMap.data() + reportMapLength, data, length);
            reportMapLength += length;
            /* reportMap.insert(reportMap.end(), data, static_cast<uint8_t *>(data + length)); */
        }

        PeripheralUnit() : getReportMapCallback(nullptr), getNotifyCallback(nullptr)
        {
            resetReportMap();
            reset();
        }

        void reset()
        {
            
            conn = nullptr;
            curSubIndex = 0;
            subscribed = 0;
            occupied = 0;
            resetReportMap();
            memset(&targetMac, 0, sizeof(targetMac));
            

        }

        bool empty()
        {
            return occupied;
        }
        
    };
    
    inline static std::array<PeripheralUnit, CONFIG_BT_MAX_PAIRED> PeripheralSequence;
    static PeripheralUnit *getPeripheralUnitByConn(struct bt_conn *conn)
    {
        uint8_t index = bt_conn_index(conn);
        /* for(auto &it: PeripheralSequence) {
            if(conn == it.conn)
                return &it;
        } */
        return &PeripheralSequence[index];
    }

    

    inline static std::set<std::string> scanTargetNameSequence;

    inline static BLECallback callbacks;


    static void connected(struct bt_conn *conn, uint8_t err);
    static void disconnected(struct bt_conn *conn, uint8_t reason);
    static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err);
    
    static uint8_t read_report_map_cb(struct bt_conn *conn, uint8_t err,
                                 struct bt_gatt_read_params *params,
                                 const void *data, uint16_t length);

    static void dm_discover_completed(struct bt_gatt_dm *dm, void *context);
    static void ServiceNotFound(struct bt_conn *conn, void *context);

    inline static unsigned int subscribe_count = 0;


    static uint8_t notifySubscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length);


    static void BLEStackCallbacksInit();

    static int BLEStackScanInit();
    static int BLEStackConnInit();
    static int BLEStackAuthInit();
};

} /* MOCNordic */
