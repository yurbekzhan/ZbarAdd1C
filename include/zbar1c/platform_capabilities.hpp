#pragma once
// The supplied legacy SDK predates these enums. Values and C function signatures
// are taken from the Native API methodology supplied by the user.
enum AppCapabilities {
    eAppCapabilitiesInvalid = -1,
    eAppCapabilities1 = 1,
    eAppCapabilitiesLast = eAppCapabilities1
};
enum AttachType {
    eCanAttachNotIsolated = 1,
    eCanAttachIsolated = 2,
    eCanAttachAny = 3
};
