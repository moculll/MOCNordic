#include <MOCNordic/MOCNordicBLEMgr.h>
#include <MOCNordic/MOCNordicLogger.h>
#include <bluetooth/services/hogp.h>
#include <algorithm>
#include <MOCNordic/MOCNordicHIDevice.h>
#include <set>


LOG_MODULE_REGISTER(MOCNordic, CONFIG_LOG_DEFAULT_LEVEL);


namespace MOCNordic {


uint8_t MOCNordicBLEMgr::read_report_map_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params, const void *data, uint16_t length)
{
    DEBUG_PRINT("report map length: %d", length);
    if (err) {
        DEBUG_PRINT("Report Map read failed (err %d)", err);
        return BT_GATT_ITER_STOP;
    }
    uint8_t index = bt_conn_index(conn);
    
    if (!length) {
        
        auto &unit = PeripheralSequence[index];
        DEBUG_PRINT("get total Length: %d", unit.reportMapLength);
        if(unit.getReportMapCallback) {
            DEBUG_PRINT("registered callback, calling...");
            unit.getReportMapCallback(unit.getReportMap().data(), unit.reportMapLength);
        }

        
        DEBUG_PRINT("Report Map read complete");
  
        return BT_GATT_ITER_STOP;
    }

    std::array<uint8_t, 247> rm_buffer;
    memcpy(rm_buffer.data(), data, length);

    auto &unit = PeripheralSequence[index];
    unit.reportMapInsert(rm_buffer.data(), length);
    DEBUG_PRINT("current Inserted length: %d", unit.reportMapLength);


    return BT_GATT_ITER_CONTINUE;
}


/**
 * @note as https://github.com/zephyrproject-rtos/zephyr/issues/44579 says, subscription better be done after discovery!
 * 
 */
void MOCNordicBLEMgr::dm_discover_completed(struct bt_gatt_dm *dm, void *context)
{
    auto conn = bt_gatt_dm_conn_get(dm);
    uint8_t index = bt_conn_index(conn);

    DEBUG_PRINT("Service discovery completed, sourceIndex: %d", index);
    const struct bt_gatt_dm_attr *gatt_chrc;

    auto &unit = PeripheralSequence[index];
  
    for (gatt_chrc = bt_gatt_dm_char_next(dm, NULL); gatt_chrc; gatt_chrc = bt_gatt_dm_char_next(dm, gatt_chrc)) {
        
        const struct bt_gatt_chrc *chrc_val = bt_gatt_dm_attr_chrc_val(gatt_chrc);
        if(!chrc_val)
            break;
        if (bt_uuid_cmp(chrc_val->uuid, BT_UUID_HIDS_REPORT_MAP) == 0) {
            DEBUG_PRINT("found report map......");
            /* static struct bt_gatt_read_params read_params = {
                .func = read_report_map_cb,
                .handle_count = 1,
                .single = {
                    .handle = chrc_val->value_handle,
                    .offset = 0,
                },

            }; */
        
            unit.resetReportMap();
            unit.reportMapReadParams.func = read_report_map_cb;
            unit.reportMapReadParams.handle_count = 1;
            unit.reportMapReadParams.single.handle = chrc_val->value_handle;
            unit.reportMapReadParams.single.offset = 0;
        

            
            /* int err = bt_gatt_read(bt_gatt_dm_conn_get(dm), &read_params);
            if (err) {
                DEBUG_PRINT("Report Map read failed (err %d)", err);
            } */

        }

        if (!(chrc_val->properties & (BT_GATT_CHRC_NOTIFY/*  | BT_GATT_CHRC_INDICATE */))) {
            continue;
        }

        
        auto gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_HIDS_REPORT_REF);
        if (gatt_desc) {
            DEBUG_PRINT("get hid handle: %02x", gatt_desc->handle);
            bt_gatt_read_params *param = new bt_gatt_read_params;
            param->func = [](struct bt_conn *conn, uint8_t err, struct bt_gatt_read_params *params, const void *data, uint16_t length){
                if (err) {
                    delete params;
                    return (uint8_t)BT_GATT_ITER_STOP;
                }
                uint8_t index = bt_conn_index(conn);
                if(length) {
                    auto reportId = *(reinterpret_cast<const uint8_t *>(data));
                    registerCharHandleReportIdMap(index, params->single.handle, reportId);
                    DEBUG_PRINT("registered for reportId: %02x", reportId);
                }
                    

                return (uint8_t)BT_GATT_ITER_CONTINUE;
            };
            param->handle_count = 1;
            param->single.handle = gatt_desc->handle;

            param->single.offset = 0;
            registerRefHandleCharHandleMap(index, gatt_desc->handle, chrc_val->value_handle);
            bt_gatt_read(conn, param);
        }
       
        struct bt_gatt_subscribe_params *sub = &unit.subscribeParams[unit.curSubIndex].subscribeParams;
        memset(sub, 0, sizeof(bt_gatt_subscribe_params));
        memset(&unit.subscribeParams[unit.curSubIndex].discoverParams, 0, sizeof(bt_gatt_discover_params));
        /* auto subscription */
        sub->ccc_handle = 0x0000;
        sub->end_handle = 0xffff;

        sub->value_handle = chrc_val->value_handle;
        sub->value = BT_GATT_CCC_NOTIFY;
        sub->notify = notifySubscribe;
        
        sub->disc_params = &unit.subscribeParams[unit.curSubIndex++].discoverParams;

        atomic_set_bit(sub->flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

        if(bt_gatt_subscribe(unit.conn, sub)) {
            DEBUG_PRINT("subscribe handle: %d failed.", sub->value_handle);

        }
        else {
            DEBUG_PRINT("subscribed: %d", sub->value_handle);
        }
       
    }
    bt_gatt_dm_data_print(dm);
    bt_gatt_dm_data_release(dm);
    bt_gatt_dm_continue(dm, NULL);
}


uint8_t MOCNordicBLEMgr::notifySubscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{   
    DEBUG_PRINT("get notify from handle: %02x", params->value_handle);

    uint8_t index = bt_conn_index(conn);
    auto &unit = PeripheralSequence[index];
    if (length) {


        short response_len = length >= 247 ? (247 - 1) : length;
        char response[247];

        if(unit.subscribed && unit.getNotifyCallback) {
            /* hid report */
            auto hidChar = unit.charHandleReportIdMap.find(params->value_handle);
       
            if(hidChar != unit.charHandleReportIdMap.end()) {
                /* reportId */
                response[0] = hidChar->second;
                /* report is small, so just ignore the overflow situation... */
                memcpy(&response[1], data, response_len);
                ++length;
            }
            else {
                memcpy(response, data, response_len);
            }
            DEBUG_PRINT_HEX("notification", data, length);
            unit.getNotifyCallback((uint8_t *)response, length);
        }
            
        //MOCNordic::MOCNordicHIDevice::writeToDevice(0, reinterpret_cast<uint8_t *>(response), length);
    }
    return BT_GATT_ITER_CONTINUE;
}



void MOCNordicBLEMgr::ServiceNotFound(struct bt_conn *conn, void *context) {
    /* discovery done. */
    DEBUG_PRINT("Discovery service all done.");
    uint8_t index = bt_conn_index(conn);

    /* we update param until discovery done because timeout would effect discover speed */
    /* update param in the end, otherwise service discovery slow */
    /* some keyboard with lower interval cause key delay, so just use connect param */
    /* static struct bt_le_conn_param conn_param = {
        .interval_min = 6,
        .interval_max = 6,
        .latency = 0,
        .timeout = 50,
    };

    int zephyr_err = bt_conn_le_param_update(conn, &conn_param);

    if (zephyr_err) {
        DEBUG_PRINT("bt_conn_le_param_update failed (err %d)", zephyr_err);
    }
    
    DEBUG_PRINT("timeoutMs: %d", conn_param.timeout * 10); */
    
    auto &unit = PeripheralSequence[index];
    unit.subscribed = 1;

    int err = bt_gatt_read(unit.conn, &unit.reportMapReadParams);
    if (err) {
        DEBUG_PRINT("Report Map read failed (err %d)", err);
    }
    
    unit.linkTimeMs = k_uptime_get() - unit.linkTimeMs;
    double testTime = static_cast<double>(unit.linkTimeMs) / 1000.0;
    DEBUG_PRINT("[TEST] total time: %.2fs", testTime);
    


    
}


void MOCNordicBLEMgr::BLEStackCallbacksInit()
{
    callbacks.scan_cb.cb_data = {
        .filter_match = [](struct bt_scan_device_info *device_info, struct bt_scan_filter_match *filter_match, bool connectable) {
            DEBUG_PRINT_HEX("filterMatched", device_info->recv_info->addr->a.val, 6);
            int error = bt_scan_stop();
            if(error) {
                DEBUG_PRINT("bt scan stop failed");
            }
        },
        .filter_no_match = [] (struct bt_scan_device_info *device_info,bool connectable) {

        },
        .connecting_error = [](struct bt_scan_device_info *device_info) {
            DEBUG_PRINT_HEX("connectFailed", device_info->recv_info->addr->a.val, 6);
            
        },
        .connecting = [](struct bt_scan_device_info *device_info, struct bt_conn *conn) {
            DEBUG_PRINT_HEX("connecting", device_info->recv_info->addr->a.val, 6);
        },
    };
    callbacks.scan_cb.scan_cb.cb_addr = &callbacks.scan_cb.cb_data;

    callbacks.auth_cb.cancel = [] (struct bt_conn *conn) {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));
        DEBUG_PRINT("Pairing cancelled: %s", addr_str);
    };

    callbacks.auth_cb.passkey_display = [] (struct bt_conn *conn, unsigned int passkey) {
        char addr[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        DEBUG_PRINT("Passkey for %s: %06u", addr, passkey);
    };

    callbacks.auth_cb.passkey_confirm = [] (struct bt_conn *conn, unsigned int passkey) {
        char addr[BT_ADDR_LE_STR_LEN];

        /* auth_conn = bt_conn_ref(conn); */

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

        DEBUG_PRINT("Passkey for %s: %06u", addr, passkey);

    };

    callbacks.auth_cb.oob_data_request = [] (struct bt_conn *conn, struct bt_conn_oob_info *info) {
        

        DEBUG_PRINT("oob_data_request");

    };

    callbacks.auth_cb.pairing_confirm = [] (struct bt_conn *conn) {
        

        DEBUG_PRINT("pairing_confirm");

    };
    
    callbacks.auth_cb.passkey_entry = [] (struct bt_conn *conn) {
        

        DEBUG_PRINT("passkey_entry");

    };


    callbacks.auth_info_cb.pairing_complete = [] (struct bt_conn *conn, bool bonded) {
        DEBUG_PRINT("pairing_complete");
        
        

        char addr_str[BT_ADDR_LE_STR_LEN];
        auto macAddr = bt_conn_get_dst(conn);
        bt_addr_le_to_str(macAddr, addr_str, sizeof(addr_str));
        DEBUG_PRINT("Pairing completed: %s, bonded: %d", addr_str, bonded);
        bt_gatt_dm_start(conn, NULL, &callbacks.dm_cb, NULL);
        /* bt_scan_filter_remove_all(); */
        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
        
        
    };

    callbacks.auth_info_cb.pairing_failed = [] (struct bt_conn *conn, enum bt_security_err reason) {
        DEBUG_PRINT("pairing_failed");
        /* bt_scan_filter_remove_all(); */
        /* should start connecting next device */
        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);

        char addr_str[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

        DEBUG_PRINT("Pairing failed: %s, reason %d", addr_str, reason);
        
    };


    callbacks.dm_cb.completed = dm_discover_completed;
    callbacks.dm_cb.error_found = [] (struct bt_conn *conn, int err, void *context) {
        DEBUG_PRINT("Discovery failed (err %d), %s", err, bt_hci_err_to_str(err));
        
    };

    callbacks.dm_cb.service_not_found = ServiceNotFound;


    callbacks.conn_cb.connected = [](struct bt_conn *conn, uint8_t err) {
        if (err) {
            DEBUG_PRINT("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
            return;
        }
        char addr_str[BT_ADDR_LE_STR_LEN];
        uint8_t index = bt_conn_index(conn);
        PeripheralSequence[index].linkTimeMs = k_uptime_get();
        PeripheralSequence[index].conn = /* bt_conn_ref( */conn/* ) */;
        DEBUG_PRINT("current WorkCnt: %d", currentWorkCnt++);

        bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

        int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
        
        if (sec_err) {
            DEBUG_PRINT("Failed to set security level (err %d)", sec_err);
            bt_conn_disconnect(PeripheralSequence[index].conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            PeripheralSequence[index].reset();
            /* bt_scan_filter_remove_all(); */
            bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
            /* bt_gatt_dm_start(conn, NULL, &callbacks.dm_cb, NULL);
            struct bt_le_scan_param scan_param = BT_LE_SCAN_PARAM_INIT(BT_LE_SCAN_TYPE_ACTIVE, BT_LE_SCAN_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL, BT_GAP_SCAN_FAST_WINDOW);
            bt_le_scan_start(&scan_param, NULL); */
        }
        else {
            /* static struct bt_gatt_exchange_params mtu_exchange_params;
            mtu_exchange_params.func = [] (struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params) {
                DEBUG_PRINT("MTU exchange %u %s (%u)", bt_conn_index(conn), err == 0U ? "successful" : "failed", bt_gatt_get_mtu(conn));
            };

            bt_gatt_exchange_mtu(conn, &mtu_exchange_params); */

            
            

            
            
        }
        
        
    };

    callbacks.conn_cb.disconnected = [](struct bt_conn *conn, uint8_t reason) {
        DEBUG_PRINT("disconnected");
        char addr_str[BT_ADDR_LE_STR_LEN];
        auto macAddr = bt_conn_get_dst(conn);
        uint8_t index = bt_conn_index(conn);
        bt_addr_le_to_str(macAddr, addr_str, sizeof(addr_str));

        DEBUG_PRINT("Disconnected from addr %s (reason %u) %s", addr_str,
            reason, bt_hci_err_to_str(reason));
        
        disconnectTarget(index);
    };

    callbacks.conn_cb.security_changed = [](struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
        DEBUG_PRINT("security_changed");
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));
        

        if (!err) {
            DEBUG_PRINT("Security changed: %s level %u", addr_str, level);

            
        } else {
            DEBUG_PRINT("Security failed: %s level %u err %d", addr_str, level, err);
        }

    };

    

}

int MOCNordicBLEMgr::BLEStackScanInit()
{
    int err = 0;
    struct bt_le_conn_param conn_param = {
        .interval_min = 6, /* Minimum Connection Interval (interval_min * 1.25 ms) */
        .interval_max = 6, /* Maximum Connection Interval (interval_max * 1.25 ms) */
        .latency = 0,
        .timeout = 50, /* Supervision Timeout (timeout * 10 ms) */
    };
    struct bt_le_scan_param scan_param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.options = BT_LE_SCAN_OPT_NONE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL_MIN,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};
    struct bt_scan_init_param scan_init = {
        .scan_param = &scan_param,
        .connect_if_match = true,
		.conn_param = &conn_param
	};

	bt_scan_init(&scan_init);
    
    
    bt_scan_cb_register(&callbacks.scan_cb.scan_cb);

    

	err = bt_scan_filter_enable(BT_SCAN_NAME_FILTER | BT_SCAN_ADDR_FILTER, false);
    if(err) {
        DEBUG_PRINT("scan filter added failed.");
        return err;
    }
    return bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
}

int MOCNordicBLEMgr::BLEStackConnInit()
{
    
    return bt_conn_cb_register(&callbacks.conn_cb);
}

int MOCNordicBLEMgr::BLEStackAuthInit()
{
    int err = 0;

    err = bt_conn_auth_cb_register(&callbacks.auth_cb);
    if(0 != err)
        return err;
    
    err = bt_conn_auth_info_cb_register(&callbacks.auth_info_cb);
    if(0 != err)
        return err;

    return err;
}

int MOCNordicBLEMgr::BLEStackInit()
{
    int err = 0;
    
    subscribeWorkCtl.init();


    /* callbacks must be inited before any stack function */
    BLEStackCallbacksInit();

    err = bt_enable(NULL);
    if (err) {
        DEBUG_PRINT("Bluetooth init failed (err %d)", err);
        return err;
    }

    err = BLEStackConnInit();
    if (err) {
        DEBUG_PRINT("Bluetooth init failed (err %d)", err);
        return err;
    }
    
    err = BLEStackAuthInit();
    if (err) {
        DEBUG_PRINT("Bluetooth init failed (err %d)", err);
        return err;
    }

    err = BLEStackScanInit();
    if (err) {
        DEBUG_PRINT("Bluetooth init failed (err %d)", err);
        return err;
    }
    
    
    return err;
}

int MOCNordicBLEMgr::setScanTarget(uint8_t index, uint8_t *target)
{
    if(PeripheralSequence[index].conn) {
        return -1;
    }

    PeripheralSequence[index].reset();
    memcpy(&PeripheralSequence[index].targetMac.a.val, target, 6);
    return 0;
    

}


int MOCNordicBLEMgr::setScanTarget(bt_addr_le_t *target)
{
    for(auto &it: PeripheralSequence) {
        if(memcmp(it.targetMac.a.val, target->a.val, 6) == 0) {
            return 1;
        }
        if(it.occupied || it.conn)
            continue;
        it.reset();
        it.occupied = 1;
        memcpy(&it.targetMac, target, sizeof(bt_addr_le_t));
        return 0;
    }
    return 1;
}

int MOCNordicBLEMgr::setScanTarget(uint8_t *target)
{
    for(auto &it: PeripheralSequence) {
        if(memcmp(it.targetMac.a.val, target, 6) == 0) {
            return 1;
        }
        if(it.occupied || it.conn)
            continue;
        
        it.reset();
        it.occupied = 1;
        memcpy(&it.targetMac.a.val, target, 6);
        DEBUG_PRINT("emplaced back new mac to vector.");
        return 0;
    }

    return 1;
}

int MOCNordicBLEMgr::setScanTarget(const std::string_view &generalName)
{
    int err = 0;
    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, generalName.data());
	if (err) {
		DEBUG_PRINT("Set name filter error: %d", err);
	}
    else {
        auto index = getAvailableIndex();
        PeripheralSequence[index].reset();
        PeripheralSequence[index].occupied = 1;
    }
    return err;
}

int MOCNordicBLEMgr::removeScanTarget(bt_addr_le_t *target)
{
    for(auto &it: PeripheralSequence) {
        if(memcmp(it.targetMac.a.val, target->a.val, BT_ADDR_SIZE) == 0) {
            it.reset();
            DEBUG_PRINT("erased target from vector.");
            return 0;
        }
    }
    DEBUG_PRINT("Failed to erased target from vector.");
    return -1;
    
}

int MOCNordicBLEMgr::removeScanTarget(uint8_t *target)
{
    for(auto &it: PeripheralSequence) {

        if(memcmp(it.targetMac.a.val, target, BT_ADDR_SIZE) == 0) {
            it.reset();
            DEBUG_PRINT("erased target from vector.");
            return 0;
        }
    }
    DEBUG_PRINT("Failed to erased target from vector.");
    return -1;
}

void MOCNordicBLEMgr::printSequenceInfo()
{
    DEBUG_PRINT("----------------SEQINFO-----------------");
    for(auto &it: PeripheralSequence) {

        DEBUG_PRINT_HEX("MAC", it.targetMac.a.val, 6);
    }
    DEBUG_PRINT("----------------SEQ END-----------------");
}

int MOCNordicBLEMgr::removeScanTarget(uint8_t index)
{
    if(index > PeripheralSequence.size() - 1) {
        DEBUG_PRINT("index doesn't exists.");
        return -1;
    }
    PeripheralSequence[index].reset();
    return 0;
    
}

int MOCNordicBLEMgr::disconnectTarget(uint8_t index)
{
    if(index > PeripheralSequence.size() - 1) {
        DEBUG_PRINT("index doesn't exists.");
        return -1;
    }
    /* bt_scan_filter_remove_all(); */
	while(0 != bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(PeripheralSequence[index].conn))) {
        DEBUG_PRINT("unpair failed, try again...");
    }

    PeripheralSequence[index].reset();
    return 0;
    
}

} /* MOCNordic */