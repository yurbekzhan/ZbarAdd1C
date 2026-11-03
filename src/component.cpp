#include "zbar1c/component.hpp"
#include "zbar1c/qr_reader.hpp"
#include "zbar1c/platform_capabilities.hpp"

#include <algorithm>
#include <array>
#include <codecvt>
#include <limits>
#include <locale>
#include <new>
#include <stdexcept>

namespace {
static_assert(sizeof(WCHAR_T) == 2, "1C Native API requires UTF-16 code units");
template<std::size_t N> constexpr auto native(const char16_t (&text)[N]) {
    std::array<WCHAR_T, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = static_cast<WCHAR_T>(text[i]);
    return result;
}
constexpr auto class_name = native(u"ZbarQR");
constexpr auto version_en = native(u"Version");
constexpr auto version_ru = native(u"Версия");
constexpr auto read_en = native(u"ReadQR");
constexpr auto read_ru = native(u"ПрочитатьQR");
constexpr auto all_en = native(u"ReadQRs");
constexpr auto all_ru = native(u"ПрочитатьВсеQR");
const WCHAR_T* method_name(long method, long alias) {
    if (alias != 0 && alias != 1) return nullptr;
    switch (method) {
    case 0: return alias ? version_ru.data() : version_en.data();
    case 1: return alias ? read_ru.data() : read_en.data();
    case 2: return alias ? all_ru.data() : all_en.data();
    default: return nullptr;
    }
}
WCHAR_T fold(WCHAR_T c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 0x410 && c <= 0x42f)) return c + 32;
    return c == 0x401 ? 0x451 : c;
}
bool equal(const WCHAR_T* a, const WCHAR_T* b) {
    if (!a || !b) return false;
    while (*a && *b && fold(*a) == fold(*b)) { ++a; ++b; }
    return *a == *b;
}
std::u16string from_utf8(const std::string& value) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(value);
}
std::filesystem::path path_from_variant(const tVariant& value) {
    if (value.vt != VTYPE_PWSTR || !value.pwstrVal || !value.wstrLen)
        throw std::runtime_error("Expected a nonempty PNG file path string");
    std::u16string text;
    text.reserve(value.wstrLen);
    for (uint32_t i = 0; i < value.wstrLen; ++i) {
        if (!value.pwstrVal[i]) throw std::runtime_error("PNG path contains a null character");
        text.push_back(static_cast<char16_t>(value.pwstrVal[i]));
    }
    // Validate UTF-16 before passing it to the filesystem implementation.
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    (void)convert.to_bytes(text);
    return std::filesystem::path(text);
}
std::u16string json(const std::vector<std::string>& codes) {
    std::u16string result = u"[";
    bool first = true;
    constexpr char16_t hex[] = u"0123456789abcdef";
    for (const auto& code : codes) {
        if (!first) result += u",";
        first = false;
        result += u'"';
        for (auto c : from_utf8(code)) {
            if (c == u'"' || c == u'\\') { result += u'\\'; result += c; }
            else if (c < 0x20) {
                result += u"\\u00";
                result += hex[(c >> 4) & 15]; result += hex[c & 15];
            } else result += c;
        }
        result += u'"';
    }
    result += u"]";
    return result;
}
}

namespace zbar1c {
bool Component::Init(void* connection) {
    connection_ = static_cast<IAddInDefBase*>(connection);
    return connection_ != nullptr;
}
bool Component::setMemManager(void* memory) {
    memory_ = static_cast<IMemoryManager*>(memory);
    return memory_ != nullptr;
}
void Component::Done() { memory_ = nullptr; connection_ = nullptr; }
WCHAR_T* Component::allocate(const std::u16string& value) {
    if (!memory_ || value.size() >= std::numeric_limits<uint32_t>::max() ||
        value.size() >= std::numeric_limits<unsigned long>::max() / sizeof(WCHAR_T))
        return nullptr;
    void* allocation = nullptr;
    if (!memory_->AllocMemory(&allocation, static_cast<unsigned long>((value.size() + 1) * sizeof(WCHAR_T))) ||
        !allocation) return nullptr;
    auto* result = static_cast<WCHAR_T*>(allocation);
    std::copy(value.begin(), value.end(), result);
    result[value.size()] = 0;
    return result;
}
bool Component::return_string(tVariant* result, const std::u16string& value) {
    auto* buffer = allocate(value);
    if (!buffer) throw std::runtime_error("1C memory allocation failed or memory manager is not set");
    result->pwstrVal = buffer;
    result->wstrLen = static_cast<uint32_t>(value.size());
    result->vt = VTYPE_PWSTR;
    return true;
}
void Component::error(const char* message) noexcept {
    if (!connection_) return;
    try {
        const auto text = from_utf8(message);
        std::basic_string<WCHAR_T> description(text.begin(), text.end());
        connection_->AddError(ADDIN_E_FAIL, class_name.data(), description.c_str(), 0);
    } catch (...) { /* No exception may cross the Native API boundary. */ }
}
bool Component::RegisterExtensionAs(WCHAR_T** name) {
    if (!name) return false;
    *name = nullptr;
    try { *name = allocate(u"ZbarQR"); return *name != nullptr; }
    catch (...) { return false; }
}
long Component::FindMethod(const WCHAR_T* name) {
    for (long i = 0; i < GetNMethods(); ++i)
        if (equal(name, method_name(i, 0)) || equal(name, method_name(i, 1))) return i;
    return -1;
}
const WCHAR_T* Component::GetMethodName(long method, long alias) {
    const auto* name = method_name(method, alias);
    if (!name) return nullptr;
    try {
        std::u16string text;
        while (*name) text.push_back(static_cast<char16_t>(*name++));
        return allocate(text);
    } catch (...) { return nullptr; }
}
long Component::GetNParams(long method) { return method == 1 || method == 2 ? 1 : 0; }
bool Component::HasRetVal(long method) { return method >= 0 && method < GetNMethods(); }
bool Component::CallAsFunc(long method, tVariant* result, tVariant* args, long count) {
    if (!result) return false;
    tVarInit(result);
    try {
        if (!HasRetVal(method) || count != GetNParams(method) || (count && !args))
            throw std::runtime_error("Unknown method or invalid argument count");
        if (method == 0) return return_string(result, u"0.1.0");
        const auto codes = read_qr_png(path_from_variant(args[0]));
        if (method == 1) return return_string(result, codes.empty() ? u"" : from_utf8(codes.front()));
        return return_string(result, json(codes));
    } catch (const std::exception& e) { error(e.what()); }
    catch (...) { error("Unexpected QR component error"); }
    return false;
}
}

#if defined(_WIN32)
#define ZBAR1C_EXPORT __declspec(dllexport)
#else
#define ZBAR1C_EXPORT __attribute__((visibility("default")))
#endif
extern "C" ZBAR1C_EXPORT const WCHAR_T* GetClassNames() { return class_name.data(); }
extern "C" ZBAR1C_EXPORT long GetClassObject(const WCHAR_T* name, IComponentBase** object) {
    if (!object || *object || !equal(name, class_name.data())) return 0;
    try {
        *object = new zbar1c::Component;
        // The interface is returned via object, never truncated to a 32-bit long.
        return 1;
    } catch (...) { return 0; }
}
extern "C" ZBAR1C_EXPORT long DestroyObject(IComponentBase** object) {
    if (!object || !*object) return -1;
    delete *object;
    *object = nullptr;
    return 0;
}
extern "C" ZBAR1C_EXPORT AppCapabilities SetPlatformCapabilities(AppCapabilities capabilities) {
    return capabilities >= eAppCapabilities1 ? eAppCapabilities1 : eAppCapabilitiesInvalid;
}
extern "C" ZBAR1C_EXPORT AttachType GetAttachType() { return eCanAttachAny; }
