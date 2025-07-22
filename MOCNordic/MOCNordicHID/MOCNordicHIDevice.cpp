#include <MOCNordic/MOCNordicHIDevice.h>
#include <MOCNordic/MOCNordicLogger.h>

LOG_MODULE_REGISTER(HIDevice, CONFIG_LOG_DEFAULT_LEVEL);
namespace MOCNordic {


int MOCNordicHIDevice::writeToDevice(uint8_t index, uint8_t *data, uint32_t length)
{

    return hid_int_ep_write(deviceUnits[index].device, data, length, NULL);
}

int MOCNordicHIDevice::writeToDevice(uint8_t index, uint8_t reportId, uint8_t *data, uint32_t length)
{
    
    uint8_t fullReport[64] = {0};
    fullReport[0] = reportId;
    memcpy(&fullReport[1], data, length);
    

    return hid_int_ep_write(deviceUnits[index].device, fullReport, ((length + 1 > sizeof(fullReport)) ? sizeof(fullReport) : length + 1), NULL);

}

/* this function can be called multiple times */
int MOCNordicHIDevice::deviceUnitInit(uint8_t index, ReportDesc &desc)
{
    int err = 0;
    if(index > deviceUnits.size() - 1) {
        DEBUG_PRINT("index outof range, check.");
        return -1;
    }
   
    if(deviceUnits[index].device) {
        usb_disable();
        for(int i = 0; i < static_cast<int>(deviceUnits.size()); i++) {
            if(i == index) {
                DEBUG_PRINT("hot enumerating for HID_%d", index);
                /* deviceUnits[index].reportDesc.insert(desc.data(), desc.size(), ReportDescType::Keyboard); */
                deviceUnits[index].reportDesc = desc;
                auto touchpadRec = deviceUnits[index].reportDesc.touchpadRec();
                if(touchpadRec.isValid()) {
                    deviceUnits[index].reportDesc.reportContactCnt[0] = touchpadRec.contactCountReportId;
                    deviceUnits[index].reportDesc.reportContactCnt[1] = touchpadRec.fingerCnt;
                }
                usb_hid_register_device(deviceUnits[index].device, deviceUnits[index].reportDesc.data(), deviceUnits[index].reportDesc.size(), &deviceUnits[index].callbacks);
                
                err = usb_hid_init(deviceUnits[index].device);
                if(err) {
                    DEBUG_PRINT("can not initilize hid device, err: %d", err);
                }
                DEBUG_PRINT_HEX("newDesc", deviceUnits[index].reportDesc.data(), deviceUnits[index].reportDesc.size());
            }
            /* this step is for other device response host, otherwise usb get stucked. */
            else {
                usb_hid_register_device(deviceUnits[i].device, deviceUnits[i].reportDesc.data(), deviceUnits[i].reportDesc.size(), &deviceUnits[i].callbacks);
                usb_hid_init(deviceUnits[i].device);
                DEBUG_PRINT("reinitialized for HID_%d.", i);
            }
        }

        int ret = usb_enable(NULL);
      
        
        return ret;
    }


    char deviceName[16];
    snprintf(deviceName, sizeof(deviceName), "HID_%d", index);
    auto hid_dev = device_get_binding(deviceName);
    if (hid_dev == NULL) {
		return -1;
	}
    /* deviceUnits[index].msgqCtl.init(); */
    deviceUnits[index].reportDesc = desc;
    deviceUnits[index].device = hid_dev;
    
    deviceUnits[index].callbacks.get_report = [] (const struct device *dev, struct usb_setup_packet *setup, int32_t *len, uint8_t **data) {
        uint8_t index = getIndexFromDev(dev);
        if(deviceUnits[index].reportDesc.getType(0) == ReportDescType::Touchpad) {
            if((setup->wValue & 0xFF) == deviceUnits[index].reportDesc.reportContactCnt[0]) {
                *data = deviceUnits[index].reportDesc.reportContactCnt.data();
                *len = deviceUnits[index].reportDesc.reportContactCnt.size();
                return 0;
            }
 
        }
        
        return -ENOTSUP;
        
    };

    /* deviceUnits[index].callbacks.set_report = [] (const struct device *dev, struct usb_setup_packet *setup, int32_t *len, uint8_t **data) {

        
        return 0;
    }; */

    
    /* use std::bind will be better */
    deviceUnits[index].callbacks.int_in_ready = [] (const struct device *dev) {
        /* DEBUG_PRINT("int_in_ready_cb"); */
        /* int index = getIndexFromDev(dev);
        if(-1 != index) {
            k_sem_give(&deviceUnits[index].write_pending);
        } */
        
    };


    deviceUnits[index].callbacks.int_out_ready = [] (const struct device *dev) {
        DEBUG_PRINT("int_out_ready_cb");
        static uint8_t out_buf[CONFIG_HID_INTERRUPT_EP_MPS] = {0};
        int ret;
        uint32_t retBytes = 0;
        ret = hid_int_ep_read(dev, out_buf, CONFIG_HID_INTERRUPT_EP_MPS, &retBytes);
        if (ret < 0) {
            DEBUG_PRINT("hid_int_ep_read failed: %d", ret);
            return;
        }
        /* DEBUG_PRINT_HEX("out ep", out_buf, retBytes);
        hid_int_ep_write(dev, out_buf, retBytes, NULL); */
    };

    /* deviceUnits[index].callbacks.protocol_change = [] (const struct device *dev, uint8_t protocol) {
        DEBUG_PRINT("protocol_change_cb");

    };

    deviceUnits[index].callbacks.on_idle = [] (const struct device *dev, uint16_t report_id) {
        DEBUG_PRINT("on_idle_cb");

    }; */

    usb_hid_register_device(hid_dev, deviceUnits[index].reportDesc.data(), deviceUnits[index].reportDesc.size(), &deviceUnits[index].callbacks);
    err = usb_hid_init(hid_dev);

    if(err) {
        DEBUG_PRINT("can not initilize hid device, err: %d", err);
        deviceUnits[index].device = nullptr;
        return err;
    }
    DEBUG_PRINT("default initialize for %s", deviceName);
    return 0;
}

int MOCNordicHIDevice::createDefault(uint8_t index)
{

    /* SPP reportId defaults to 0xff */
    SPPReportDesc newSPPReport(0x01);
    return deviceUnitInit(index, newSPPReport);
}




int MOCNordicHIDevice::init()
{
    /* usb_disable(); */
    /* delayLogger.init(); */
    k_mutex_init(&initMutex);
    
    for(int i = 0; i < maxHIDevice; i++) {
        createDefault(i);
    }
    
    return usb_enable([] (enum usb_dc_status_code status, const uint8_t *param)
    {
        switch (status) {
        case USB_DC_RESET:
            usb_dc_status.configured = false;
            usb_dc_status.suspended = false;
            break;
        case USB_DC_CONFIGURED:
            usb_dc_status.configured = true;
            break;
        case USB_DC_DISCONNECTED:
            usb_dc_status.configured = false;
            usb_dc_status.suspended = false;
            break;
        case USB_DC_SUSPEND:
            usb_dc_status.suspended = true;
            break;
        case USB_DC_RESUME:
            usb_dc_status.suspended = false;
            break;
        default:
            break;
        }
    });
}

void MOCNordicHIDevice::printDesc(uint8_t index)
{
    DEBUG_PRINT_HEX("desc", deviceUnits[index].reportDesc.data(), deviceUnits[index].reportDesc.size());
}


} /* MOCNordic */