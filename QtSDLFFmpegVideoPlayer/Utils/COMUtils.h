#pragma once
#include <Unknwn.h>

class IUnknownImpl : public IUnknown {
    ULONG m_refCount = 1;
    bool selfLifeCycleManagement = false;
public:
    // selfLifeCycleManaged: 将IUnknownImpl的生命周期交给IUnknownImpl自身管理
    IUnknownImpl(bool selfLifeCycleManaged) : selfLifeCycleManagement(selfLifeCycleManaged) {}
    // IUnknown 方法实现
    HRESULT __stdcall QueryInterface(const IID& iid, void** ppv) override
    {
        if (iid == IID_IUnknown) {
            *ppv = this;
            AddRef(); // 增加引用计数
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG __stdcall AddRef() override
    {
        return InterlockedIncrement(&m_refCount);
    }

    // 返回引用递减后的值
    ULONG __stdcall Release() override
    {
        LONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0)
            if (selfLifeCycleManagement)
                delete this; // 计数为0时，删除自身
        return ref;
    }

    // 无锁，需注意线程相关问题
    virtual ULONG __stdcall GetRefCount() {
        return m_refCount;
    }
};