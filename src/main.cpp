
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
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usb_hid.h>
#include <MOCNordic/MOCNordicBLEMgr.h>
#include <MOCNordic/MOCNordicLogger.h>
#include <MOCNordic/MOCNordicHIDevice.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static void getReportMapDev0(uint8_t *data, uint32_t length)
{
    MOCNordic::ReportDesc desc;
    desc.clear();
    desc.insert(data, length, MOCNordic::ReportDescType::UNKNOWN);

    MOCNordic::MOCNordicHIDevice::deviceUnitInit(0, desc);
    DEBUG_PRINT("registered hid devcie 0");

}


static void getNotifyDev0(uint8_t *data, uint32_t length)
{
    if(!length || !data)
        return;

    MOCNordic::MOCNordicHIDevice::writeToDevice(0, data, length);

}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;
    if(DK_BTN1_MSK & button) {
        DEBUG_PRINT("button pressed.");

    }
    else {
        DEBUG_PRINT("button released.");


    }

}

int main(void)
{
    

	if(!MOCNordic::MOCNordicBLEMgr::BLEStackInit()) {
        DEBUG_PRINT("mt nordic ble stack init successful.");
    }

    int err = dk_buttons_init(button_handler);
    
	if (err) {
		DEBUG_PRINT("Failed to initialize buttons (err %d)", err);
		return 0;
	}

    if(!MOCNordic::MOCNordicHIDevice::init()) {
        DEBUG_PRINT("mt nordic hid init successful.");
    }

    MOCNordic::MOCNordicBLEMgr::registerGetReportMapCallbackToIndex(0, getReportMapDev0);
    MOCNordic::MOCNordicBLEMgr::registerNotifyToIndex(0, getNotifyDev0);

    while(1) {

        MOCNordic::MOCNordicBLEMgr::setScanTarget("LANGTU BT5.0_1");
        
        k_msleep(1000000000);

    }
}