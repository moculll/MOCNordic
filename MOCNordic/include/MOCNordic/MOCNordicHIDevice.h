#pragma once
#include <cstdint>
#include <vector>
#include <zephyr/device.h>
#include <functional>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory>
#include <array>
#include <MOCNordic/MOCZephyrType.h>
namespace MOCNordic {


enum class ReportDescType {
    Keyboard,
    Mouse,
    Touchpad,
    CustomSPP,
    UNKNOWN,
};

struct ReportDesc {
    inline static uint8_t reportCnt[] = {0x0D, 0x05};
    inline static uint8_t reportCertIn[] =
    {
        0x0C, 0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87, 0x0d, 0xbe, 0x57, 0x3c, 0xb6, 0x70, 0x09, 0x88, 0x07, 0x97, 0x2d, 0x2b, 0xe3, 0x38, 0x34, 0xb6, 0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6,0xc2, 
        0x2e, 0x84, 0x1b, 0xe8, 0xb4, 0x51, 0x78, 0x43, 0x1f, 0x28, 0x4b, 0x7c, 0x2d, 0x53, 0xaf, 0xfc, 0x47, 0x70, 0x1b, 0x59, 0x6f, 0x74, 0x43, 0xc4, 0xf3, 0x47, 0x18, 0x53, 0x1a, 0xa2, 0xa1,0x71, 
        0xc7, 0x95, 0x0e, 0x31, 0x55, 0x21, 0xd3, 0xb5, 0x1e, 0xe9, 0x0c, 0xba, 0xec, 0xb8, 0x89, 0x19, 0x3e, 0xb3, 0xaf, 0x75, 0x81, 0x9d, 0x53, 0xb9, 0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29,0x7c, 
        0x87, 0xd9, 0xb4, 0x98, 0x45, 0x7d, 0xa7, 0x26, 0x9c, 0x65, 0x3b, 0x85, 0x68, 0x89, 0xd7, 0x3b, 0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0, 0x2a, 0x41, 0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c,0xf8, 
        0xc0, 0x8f, 0x33, 0x13, 0x03, 0xf1, 0xd3, 0xc1, 0x0b, 0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51, 0xb7, 0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b, 0xe8, 0x8a, 0x56, 0xf0, 0x8c, 0xaa, 0xfa,0x35, 
        0xe9, 0x42, 0xc4, 0xd8, 0x55, 0xc3, 0x38, 0xcc, 0x2b, 0x53, 0x5c, 0x69, 0x52, 0xd5, 0xc8, 0x73, 0x02, 0x38, 0x7c, 0x73, 0xb6, 0x41, 0xe7, 0xff, 0x05, 0xd8, 0x2b, 0x79, 0x9a, 0xe2, 0x34,0x60, 
        0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78, 0x62, 0xbc, 0x80, 0xe3, 0x0f, 0xbd, 0x65, 0x20, 0x08, 0x13, 0xc1, 0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7, 0x5a, 0xc5, 0xd3, 0x7d, 0x98, 0xbe, 0x31,0x48, 
        0x1f, 0xfb, 0xda, 0xaf, 0xa2, 0xa8, 0x6a, 0x89, 0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a, 0xe4, 0xcf, 0x17, 0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08, 0x24, 0x8b, 0xc4, 0x43, 0xa5, 0xe5, 0x24,0xc2
    };
    /* can't save with desc because usb initilize use one memory block */
    struct DescApartStorage {
        ReportDescType type;
        uint32_t length;
    };
    /* 4 device compose max */
    std::array<DescApartStorage, 16> storageSequence;
    /* spp + reportMap may not over 512 bytes, use vector will cause memory error */
    std::array<uint8_t, 768> desc;
    
    template <size_t N>
    static uint8_t findTouchpadReportID(const std::array<uint8_t, N>& descriptor) {
        uint8_t currentReportID = 0x00;

        for (size_t i = 0; i + 1 < N; ++i) {
            // 查找 Usage (0x09) 后面跟着 Usage (Touchpad, 0x05)
            if (descriptor[i] == 0x09 && descriptor[i + 1] == 0x05) {
                // 向后查找最近的 Report ID (0x85)
                for (size_t j = i + 2; j + 1 < N; ++j) {
                    if (descriptor[j] == 0x85) {
                        // 返回 0x85 后面的字节作为 report ID
                        return descriptor[j + 1];
                    }
                    
                    
                }
            }
        }

        return 0x00; // 没找到 Touchpad 的 report ID
    }


    uint8_t *data()
    {
        return desc.data();
    }

    size_t size()
    {
        return getDescLength();
    }

    bool insert(const uint8_t *data, uint32_t length, ReportDescType type)
    {
        uint32_t previoutLength = 0;

        for(auto &it: storageSequence) {
            if(it.length) {
                previoutLength += it.length;
            }
            else {
                it.length = length;
                it.type = type;
                
                memcpy(desc.data() + previoutLength, data, length);
                return true;
            }

        }

        return false;
    }

    uint32_t getDescLength()
    {
        uint32_t retval = 0;
        for(auto &it: storageSequence) {
            if(it.length)
                retval += it.length;
            
        }
        return retval;
    }

    bool remove(size_t index)
    {
        if(index > storageSequence.size() - 1)
            return false;

        storageSequence[index].length = 0;
    }


    void clear()
    {
        desc.fill(0x00);
        for(auto &it: storageSequence) {
            it.length = 0;
            it.type = ReportDescType::UNKNOWN;
        }
    }

    ReportDesc()
    {
        clear();
    }

    ReportDesc(const uint8_t *data, uint32_t length)
    {
        insert(data, length, ReportDescType::UNKNOWN);

    }

    ReportDesc(const uint8_t *data, uint32_t length, ReportDescType type)
    {
        insert(data, length, type);

    }

    ReportDesc(ReportDesc &src)
    {
        clear();
        /* std::copy(desc.begin(), src.desc.begin(), src.desc.end()); */
        /* std::copy(storageSequence.begin(), src.storageSequence.begin(), src.storageSequence.end()); */
        memcpy(desc.data(), src.desc.data(), src.desc.size());
        memcpy(storageSequence.data(), src.storageSequence.data(), storageSequence.size() * sizeof(DescApartStorage));
    }

    ReportDesc &operator=(ReportDesc &src) noexcept
    {
        clear();
        /* std::copy(desc.begin(), src.desc.begin(), src.desc.end()); */
        /* std::copy(storageSequence.begin(), src.storageSequence.begin(), src.storageSequence.end()); */
        memcpy(desc.data(), src.desc.data(), src.desc.size());
        memcpy(storageSequence.data(), src.storageSequence.data(), storageSequence.size() * sizeof(DescApartStorage));
        return *this;
    }

    ReportDesc &operator=(ReportDesc &&src) noexcept
    {
        desc = std::move(src.desc);
        storageSequence = std::move(src.storageSequence);
        return *this;
    }

    static uint32_t getReportSize(uint8_t reportId)
    {
        return 0;
    }

    void recognize()
    {

        for(auto seqMember: storageSequence) {
            if(!seqMember.length || seqMember.type != ReportDescType::UNKNOWN)
                continue;
            
        }
        
    }

};

struct SPPReportDesc : ReportDesc {
    

    inline static const uint8_t SPPReportDescBase[] = {
        0x06, 0x01,0xff, // USAGE_PAGE (Generic Desktop)
        0x09, 0x00, // USAGE (0)
        0xa1, 0x01, // COLLECTION (Application)
        0x85, 0x0C,
        0x15, 0x00, //     LOGICAL_MINIMUM (0)
        0x25, 0x01, //     LOGICAL_MAXIMUM (255)�
        0x19, 0x00, //     USAGE_MINIMUM (1)
        0x29, 0xff, //     USAGE_MAXIMUM (8)
        0x95, 63, //     REPORT_COUNT (8)
        0x75, 0x08, //     REPORT_SIZE (8)
        0x09,0x01,
        0x81, 0x01, //     INPUT (Data,Var,Abs)
        0x09,0x02,
        0x91, 0x02, //   OUTPUT (Data,Var,Abs)
        0x09,0x04,
        0xB1, 0x02, //   OUTPUT (Data,Var,Abs)
        0xc0 ,       // END_COLLECTION

        
    };




    SPPReportDesc(uint8_t reportId)
    {
        insert(SPPReportDescBase, sizeof(SPPReportDescBase), ReportDescType::CustomSPP);
        /* desc[8] = reportId; */
    }

    

};

struct MOCNordicHIDeviceUnit {
    ReportDesc reportDesc;
    const struct device *device;
    struct hid_ops callbacks;
    /* struct k_sem write_pending; */
    
    int write(uint8_t *buffer, uint32_t length)
    {
        
        
        /* printk("--------take one sem---------\r\n"); */
        return hid_int_ep_write(device, buffer, length, NULL);
    }


    /* ZMsgqControl msgqCtl; */
    MOCNordicHIDeviceUnit(MOCNordicHIDeviceUnit &&src) noexcept
    {
        /* printk("MOCNordicHIDeviceUnit called move func.\r\n"); */
        device = std::move(src.device);
        callbacks = std::move(src.callbacks);
        reportDesc = std::move(src.reportDesc);

    }

    MOCNordicHIDeviceUnit()
    {
        /* printk("MOCNordicHIDeviceUnit called create func.\r\n"); */
        device = nullptr;
        memset(&callbacks, 0, sizeof(hid_ops));
        /* k_sem_init(&write_pending, 0, 1); */

    }

    MOCNordicHIDeviceUnit(MOCNordicHIDeviceUnit &src)
    {
        /* printk("MOCNordicHIDeviceUnit called copy func.\r\n"); */
        device = src.device;
        callbacks = src.callbacks;
        reportDesc = src.reportDesc;

    }
};

class MOCNordicHIDevice {
public:

    static int init();
    /* defaults to be spp */
    static int createDefault(uint8_t index);
    static int deviceUnitInit(uint8_t index, ReportDesc &desc);

    static int writeToDevice(uint8_t index, uint8_t *data, uint32_t length);
    static int writeToDevice(uint8_t index, uint8_t reportId, uint8_t *data, uint32_t length);
    
    /* int create(uint8_t index); */

private:

#if CONFIG_USB_HID_DEVICE_COUNT
    inline static constexpr int maxHIDevice = CONFIG_USB_HID_DEVICE_COUNT;
#else
    inline static constexpr int maxHIDevice = 2;
#endif
    /* inline static MOCZephyr::ZWorkControl<5> delayLogger; */
    inline static std::array<MOCNordicHIDeviceUnit, maxHIDevice> deviceUnits;

    inline static struct k_mutex initMutex;

    static int getIndexFromDev(const struct device *dev)
    {
        const char* name = dev->name;
        int index = -1;
        sscanf(name, "HID_%d", &index);
        return index;
    }
    struct usb_controller_status {
        bool suspended;
        bool configured;
    };
    inline static struct usb_controller_status usb_dc_status;
    inline static struct k_thread HIDWriteThread[maxHIDevice];
    /* inline static uint8_t HIDWriteThreadStack[256]; */
    static void reportThread(void *p1, void *p2, void *p3);
};

} /* MOCNordic */