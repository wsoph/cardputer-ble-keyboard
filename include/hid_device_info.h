#pragma once
#include <string>

// Keep the optional Device Information characteristic setup testable without a radio.
template<typename HidDevice>
bool initializeHidManufacturer(HidDevice& device) {
    // In 2.0.17 the no-argument overload creates the optional characteristic.
    // Calling the string overload first dereferences an uninitialized pointer.
    auto* manufacturer = device.manufacturer();
    if (!manufacturer) return false;
    manufacturer->setValue(std::string("Cardputer Keyboard"));
    return true;
}
