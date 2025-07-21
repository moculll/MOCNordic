#pragma once
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/kernel.h>
#include <array>
#include <cstdint>
#include <functional>
namespace MOCZephyr {

template <size_t BufferSize>
struct ZRingbufControl {
    struct ring_buf ringbuf;
    std::array<uint8_t, BufferSize> buffer;

    /**
     * @retval actual length
     */
    virtual uint32_t get(uint8_t *dst, uint32_t length)
    {
        return ring_buf_get(&ringbuf, dst, length);
    }

    /**
     * @retval number of bytes written
     */
    virtual uint32_t put(uint8_t *src, uint32_t length)
    {
        return ring_buf_put(&ringbuf, src, length);
    }

    virtual void init()
    {
        buffer.fill(0x00);
        ring_buf_init(&ringbuf, BufferSize, buffer.data());
    }

};



template <typename T, size_t length>
struct ZMsgqControl {
    struct k_msgq queue;

    std::array<uint8_t, sizeof(T) * length> buffer;
    
    virtual void init()
    {
        buffer.fill(0x00);
        k_msgq_init(&queue, (char *)buffer.data(), sizeof(T), length);
    }

    virtual int put(uint8_t *src)
    {
        return k_msgq_put(&queue, src, K_NO_WAIT);
    }

    virtual int put(uint8_t *src, k_timeout_t timeout)
    {
        return k_msgq_put(&queue, src, timeout);
    }
    
    virtual int get(uint8_t *dst)
    {
        return k_msgq_get(&queue, dst, K_FOREVER);
    }

    virtual int get(uint8_t *dst, k_timeout_t timeout)
    {
        return k_msgq_get(&queue, dst, timeout);
    }

};

static K_THREAD_STACK_DEFINE(ZWorkControlStackArea, 4096);
template <size_t WorkPoolSize>
struct ZWorkControl {
    
    struct DelayableUnit {
        k_work_delayable work;
        bool use_mutex;
        k_mutex mutex;
        
        std::function<void()> work_fn;

        DelayableUnit() : work_fn(nullptr) {}
    };
    std::array<DelayableUnit, WorkPoolSize> delayableWorks;
    
    inline static k_work_q work_q;

    inline static constexpr size_t stack_size = 4096;

    static void work_fn_static(struct k_work *work)
    {
        auto *self = CONTAINER_OF(work, DelayableUnit, work);
        if(self->work_fn) {
            if(self->use_mutex) {
                /* work shouldn't be called async */
                k_mutex_lock(&self->mutex, K_FOREVER);
                
                
            }

            self->work_fn();
            if(self->use_mutex)
                k_mutex_unlock(&self->mutex);
        

        }

        
    }
    inline static bool workQInited = 0;
    static int workQInit(uint8_t priority)
    {
        printk("inited for work stack\r\n");
        k_work_queue_init(&work_q);
        k_work_queue_start(&work_q, ZWorkControlStackArea, stack_size, priority, NULL);
        workQInited = 1;
        return 0;
    }

    int init(uint8_t priority = 5)
    {
        if(!workQInited) {
            workQInit(priority);
        }
      
        for(auto &it: delayableWorks) {
            k_mutex_init(&it.mutex);
            k_work_init_delayable(&it.work, work_fn_static);
            
        }
        
        return 0;
    }

    int schedule(uint32_t index)
    {
        return k_work_reschedule(&delayableWorks[index].work, K_NO_WAIT);
    }

    int schedule(uint32_t index, k_timeout_t delay)
    {
        return k_work_reschedule(&delayableWorks[index].work, delay);
    }

    template <typename Func, typename... Args>
    int submitToQueue(Func &&fn, k_timeout_t delay, bool use_mutex, Args &&... args)
    {
        for (auto &it : delayableWorks) {
            if (0 == k_work_delayable_busy_get(&it.work)) {
                it.work_fn = std::bind(std::forward<Func>(fn), std::forward<Args>(args)...);
                it.use_mutex = use_mutex;
                return k_work_schedule_for_queue(&work_q, &it.work, delay);
            }
        }
        return -1;
    }


};


} /* MOCZephyr */
