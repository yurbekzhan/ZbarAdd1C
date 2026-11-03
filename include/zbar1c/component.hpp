#pragma once
#include "zbar1c/sdk.hpp"
#include <string>

namespace zbar1c {
class Component final : public IComponentBase {
public:
    bool ADDIN_API Init(void*) override;
    bool ADDIN_API setMemManager(void*) override;
    long ADDIN_API GetInfo() override { return 100; }
    void ADDIN_API Done() override;
    bool ADDIN_API RegisterExtensionAs(WCHAR_T**) override;
    long ADDIN_API GetNProps() override { return 0; }
    long ADDIN_API FindProp(const WCHAR_T*) override { return -1; }
    const WCHAR_T* ADDIN_API GetPropName(long, long) override { return nullptr; }
    bool ADDIN_API GetPropVal(long, tVariant* value) override {
        if (value) tVarInit(value);
        return false;
    }
    bool ADDIN_API SetPropVal(long, tVariant*) override { return false; }
    bool ADDIN_API IsPropReadable(long) override { return false; }
    bool ADDIN_API IsPropWritable(long) override { return false; }
    long ADDIN_API GetNMethods() override { return 3; }
    long ADDIN_API FindMethod(const WCHAR_T*) override;
    const WCHAR_T* ADDIN_API GetMethodName(long, long) override;
    long ADDIN_API GetNParams(long) override;
    bool ADDIN_API GetParamDefValue(long, long, tVariant* value) override {
        if (!value) return false;
        tVarInit(value);
        return true;
    }
    bool ADDIN_API HasRetVal(long) override;
    bool ADDIN_API CallAsProc(long, tVariant*, long) override { return false; }
    bool ADDIN_API CallAsFunc(long, tVariant*, tVariant*, long) override;
    void ADDIN_API SetLocale(const WCHAR_T*) override {}
private:
    IMemoryManager* memory_ = nullptr;
    IAddInDefBase* connection_ = nullptr;
    WCHAR_T* allocate(const std::u16string&);
    bool return_string(tVariant*, const std::u16string&);
    void error(const char*) noexcept;
};
}
