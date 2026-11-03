#include "zbar1c/component.hpp"
#include "zbar1c/platform_capabilities.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::basic_string<WCHAR_T> native(const std::u16string& value) {
    return {value.begin(), value.end()};
}
std::u16string text(const WCHAR_T* value, std::size_t size) {
    std::u16string result;
    for (std::size_t i = 0; i < size; ++i) result.push_back(static_cast<char16_t>(value[i]));
    return result;
}
struct Memory final : IMemoryManager {
    std::set<void*> allocations;
    bool fail = false;
    ~Memory() { for (auto* p : allocations) std::free(p); }
    bool ADDIN_API AllocMemory(void** out, unsigned long size) override {
        *out = nullptr;
        if (fail) return false;
        *out = std::malloc(size);
        if (*out) allocations.insert(*out);
        return *out != nullptr;
    }
    void ADDIN_API FreeMemory(void** pointer) override {
        check(pointer && allocations.erase(*pointer) == 1, "Unknown memory returned by component");
        std::free(*pointer); *pointer = nullptr;
    }
    void release(const WCHAR_T* p) { void* buffer = const_cast<WCHAR_T*>(p); FreeMemory(&buffer); }
};
struct Host final : IAddInDefBase {
    int errors = 0;
    bool ADDIN_API AddError(unsigned short code, const WCHAR_T* source, const WCHAR_T* description, long) override {
        check(code == ADDIN_E_FAIL && source && description && *description, "Error details");
        ++errors; return true;
    }
    bool ADDIN_API Read(WCHAR_T*, tVariant*, long*, WCHAR_T**) override { return false; }
    bool ADDIN_API Write(WCHAR_T*, tVariant*) override { return false; }
    bool ADDIN_API RegisterProfileAs(WCHAR_T*) override { return false; }
    bool ADDIN_API SetEventBufferDepth(long) override { return false; }
    long ADDIN_API GetEventBufferDepth() override { return 0; }
    bool ADDIN_API ExternalEvent(WCHAR_T*, WCHAR_T*, WCHAR_T*) override { return false; }
    void ADDIN_API CleanEventBuffer() override {}
    bool ADDIN_API SetStatusLine(WCHAR_T*) override { return false; }
    void ADDIN_API ResetStatusLine() override {}
};
struct Library {
#ifdef _WIN32
    HMODULE handle;
    explicit Library(const char* path) : handle(LoadLibraryW(std::filesystem::path(path).c_str())) {}
    ~Library() { if (handle) FreeLibrary(handle); }
    template<class T> T symbol(const char* name) {
        auto p = GetProcAddress(handle, name); check(p != nullptr, name); return reinterpret_cast<T>(p);
    }
#else
    void* handle;
    explicit Library(const char* path) : handle(dlopen(path, RTLD_NOW | RTLD_LOCAL)) {}
    ~Library() { if (handle) dlclose(handle); }
    template<class T> T symbol(const char* name) {
        auto p = dlsym(handle, name); check(p != nullptr, name); return reinterpret_cast<T>(p);
    }
#endif
};
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Library and fixture paths required");
        Library library(argv[1]);
        check(library.handle != nullptr, "Cannot load component and its dependencies");
        auto names = library.symbol<GetClassNamesPtr>("GetClassNames");
        auto create = library.symbol<GetClassObjectPtr>("GetClassObject");
        auto destroy = library.symbol<DestroyObjectPtr>("DestroyObject");
        auto capabilities = library.symbol<AppCapabilities(*)(AppCapabilities)>("SetPlatformCapabilities");
        auto attach = library.symbol<AttachType(*)()>("GetAttachType");
        check(capabilities(eAppCapabilities1) == eAppCapabilities1, "Capabilities");
        check(attach() == eCanAttachAny, "Attachment mode");
        auto expected_name = native(u"ZbarQR");
        check(text(names(), 6) == u"ZbarQR", "Class name");
        check(create(names(), nullptr) == 0, "Null factory output");
        IComponentBase* object = nullptr;
        check(create(native(u"Unknown").c_str(), &object) == 0 && !object, "Unknown class");
        check(create(expected_name.c_str(), &object) != 0 && object, "Create class");
        check(create(names(), &object) == 0, "Existing object must be preserved");
        Memory memory;
        Host host;
        check(object->Init(&host) && object->setMemManager(&memory), "Initialize");
        check(object->GetInfo() == 100, "Component version");
        WCHAR_T* extension = nullptr;
        check(object->RegisterExtensionAs(&extension) && text(extension, 6) == u"ZbarQR", "Extension name");
        memory.release(extension);
        check(object->GetNProps() == 0 && object->FindProp(nullptr) == -1, "Properties");
        tVariant value{};
        value.vt = VTYPE_I4;
        check(!object->GetPropVal(0, &value) && value.vt == VTYPE_EMPTY, "Missing property value");
        check(object->GetNMethods() == 3, "Method count");
        for (long method = 0; method < 3; ++method) {
            for (long alias = 0; alias < 2; ++alias) {
                const auto* name = object->GetMethodName(method, alias);
                check(name && object->FindMethod(name) == method, "Method aliases");
                memory.release(name);
            }
        }
        check(object->FindMethod(native(u"пРОЧИТАТЬqr").c_str()) == 1, "Case-insensitive lookup");
        check(object->FindMethod(nullptr) == -1 && object->GetMethodName(-1, 0) == nullptr &&
              object->GetMethodName(0, 2) == nullptr, "Invalid method metadata");
        check(object->GetParamDefValue(1, 0, &value) && value.vt == VTYPE_EMPTY, "No default argument");
        check(object->CallAsFunc(0, &value, nullptr, 0), "Version call");
        check(value.vt == VTYPE_PWSTR && text(value.pwstrVal, value.wstrLen) == u"0.1.0", "Version result");
        memory.release(value.pwstrVal);

        auto call = [&](long method, const std::filesystem::path& path) {
            auto file = native(path.u16string());
            tVariant argument{};
            argument.vt = VTYPE_PWSTR;
            argument.pwstrVal = file.data();
            argument.wstrLen = static_cast<uint32_t>(file.size());
            check(object->CallAsFunc(method, &value, &argument, 1), "PNG call");
            check(value.vt == VTYPE_PWSTR, "String result type");
            auto result = text(value.pwstrVal, value.wstrLen);
            memory.release(value.pwstrVal);
            return result;
        };
        std::filesystem::path fixtures(argv[2]);
        check(call(1, fixtures / "qr.png") == u"https://example.org/1c?qr=12345", "Read one QR");
        check(call(1, fixtures / "unicode.png") == u"Привет, 1С!", "UTF-16 result");
        check(call(1, fixtures / "blank.png").empty(), "No QR string");
        check(call(2, fixtures / "blank.png") == u"[]", "No QR JSON");
        auto multiple = call(2, fixtures / "multiple.png");
        check(multiple == u"[\"FIRST\",\"SECOND\"]" || multiple == u"[\"SECOND\",\"FIRST\"]", "Multiple QR JSON");
        check(call(2, fixtures / "escaped.png") == u"[\"quote\\\" slash\\\\ line\\u000a\"]", "JSON escaping");
        // A non-ASCII path exercises filesystem conversion, independently of payload.
        auto unicode_path = fixtures / std::filesystem::path(u"тест_QR_1С.png");
        check(call(1, unicode_path) == u"https://example.org/1c?qr=12345", "Unicode path");

        check(!object->CallAsFunc(1, &value, nullptr, 0) && value.vt == VTYPE_EMPTY, "Argument count error");
        tVariant bad{}; bad.vt = VTYPE_I4;
        check(!object->CallAsFunc(1, &value, &bad, 1), "Argument type error");
        memory.fail = true;
        check(!object->CallAsFunc(0, &value, nullptr, 0) && value.vt == VTYPE_EMPTY, "Allocation failure");
        memory.fail = false;
        auto missing = native((fixtures / "missing.png").u16string());
        bad.vt = VTYPE_PWSTR; bad.pwstrVal = missing.data(); bad.wstrLen = static_cast<uint32_t>(missing.size());
        check(!object->CallAsFunc(1, &value, &bad, 1), "Missing file error");
        check(host.errors == 4, "Errors reported through host API");
        check(memory.allocations.empty(), "All returned buffers belong to host allocator and are released");
        object->Done();
        check(destroy(&object) == 0 && !object, "Destroy object");
        check(destroy(&object) == -1 && destroy(nullptr) == -1, "Invalid destruction");
        std::cout << "Native API dynamic loading, metadata, memory, QR, Unicode, JSON and errors passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
