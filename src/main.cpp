
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

class BLEDevice {
public:
    explicit BLEDevice(std::string_view deviceName) : name(deviceName)
    {
        k_sem_init(&connectSem, 0, 1);
    }

    void connect()
    {
        MOCNordic::MOCNordicBLEMgr::setScanTarget(name);
    }


    void init()
    {
        auto index = MOCNordic::MOCNordicBLEMgr::getAvailableIndex();
        MOCNordic::MOCNordicBLEMgr::registerGetReportMapCallbackToIndex(index, [this, index](uint8_t *data, uint32_t length) {
            MOCNordic::ReportDesc desc;
            desc.clear();
            desc.insert(data, length, MOCNordic::ReportDescType::UNKNOWN);
            
            
            MOCNordic::MOCNordicHIDevice::deviceUnitInit(0, desc);
            k_sem_give(&connectSem);
            
        });

        MOCNordic::MOCNordicBLEMgr::registerNotifyToIndex(index, [index](uint8_t *data, uint32_t length){
            if(!length || !data)
                return;

            MOCNordic::MOCNordicHIDevice::writeToDevice(index, data, length);

        });
    }

    void waitForConnect(uint32_t timeoutMs)
    {
        k_sem_take(&connectSem, K_MSEC(timeoutMs));
    }


private:
    const std::string name;
    struct k_sem connectSem;
    
};

int main(void)
{
    
    /* int err = dk_buttons_init(button_handler);
    
	if (err) {
		DEBUG_PRINT("Failed to initialize buttons (err %d)", err);
		return 0;
    } */

	if(!MOCNordic::MOCNordicBLEMgr::BLEStackInit()) {
        DEBUG_PRINT("moc nordic ble stack init successful.");
    }

    if(!MOCNordic::MOCNordicHIDevice::init()) {
        DEBUG_PRINT("moc nordic hid init successful.");
    }

    BLEDevice device("Brydge C-Touch");
    device.init();
    device.connect();
    device.waitForConnect(2000000);
    DEBUG_PRINT("device connected.");
    while(1) {
        
        k_msleep(1000000000);

    }

}