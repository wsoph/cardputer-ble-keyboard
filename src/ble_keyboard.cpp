#include "ble_keyboard.h"
#include "hid_device_info.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <esp_bt_main.h>
#include <atomic>
#include <cstring>

namespace {
BLEServer* server = nullptr;
BLEHIDDevice* hid = nullptr;
BLECharacteristic *input = nullptr, *bootInput = nullptr;
std::atomic<bool> connected{false}, authenticated{false}, subscribed{false}, bootSubscribed{false};
std::atomic<bool> bootMode{false}, suspended{false}, advertisingNeeded{false}, rejectNeeded{false};
std::atomic<bool> disconnectSent{false};
std::atomic<bool> pairEnabled{false}, capsLock{false};
std::atomic<bool> newPairAttempt{false};
std::atomic<uint16_t> connectionId{0};
std::atomic<uint16_t> gattInterface{ESP_GATT_IF_NONE};
std::atomic<uint32_t> pairStarted{0}, linkStarted{0}, linkGeneration{0};
std::atomic<uint8_t> error{0};
std::atomic<int32_t> sendError{0};
PairingPrompt prompt;
portMUX_TYPE promptMux = portMUX_INITIALIZER_UNLOCKED;
esp_bd_addr_t currentPeer{}, pendingPeer{};

bool pairingAllowed() { return pairingOpen(millis(), pairStarted.load(), pairEnabled.load()); }
void reject() {
    authenticated = false;
    rejectNeeded = true;
}
void gattsEvent(esp_gatts_cb_event_t event, esp_gatt_if_t interface, esp_ble_gatts_cb_param_t* param) {
    if (event == ESP_GATTS_REG_EVT && param->reg.status == ESP_GATT_OK) {
        gattInterface = interface;
        Serial.printf("CARDKEY_BLE=gatt_registered interface=%u app=%u\n", unsigned(interface), unsigned(param->reg.app_id));
    }
}
void clearPrompt() {
    portENTER_CRITICAL(&promptMux);
    prompt.clear(); std::memset(pendingPeer, 0, sizeof(pendingPeer));
    portEXIT_CRITICAL(&promptMux);
}
bool isCurrentPeer(const uint8_t* address) {
    portENTER_CRITICAL(&promptMux);
    const bool matches = std::memcmp(currentPeer, address, sizeof(currentPeer)) == 0;
    portEXIT_CRITICAL(&promptMux);
    return connected.load() && matches;
}
PairingPromptStatus promptStatus() {
    portENTER_CRITICAL(&promptMux);
    const auto status = prompt.status();
    portEXIT_CRITICAL(&promptMux);
    return status;
}
void beginPrompt(PairingKind kind, uint32_t number, const uint8_t* address) {
    if (!isCurrentPeer(address)) return;
    const bool window = pairingAllowed();
    const char* type = kind == PairingKind::Entry ? "entry" : kind == PairingKind::Comparison ? "compare" : "display";
    Serial.printf("CARDKEY_BLE=challenge type=%s window=%u\n", type, window ? 1U : 0U);
    newPairAttempt = true;
    portENTER_CRITICAL(&promptMux);
    const bool started = prompt.begin(kind, number, millis(), window);
    if (started) std::memcpy(pendingPeer, address, sizeof(pendingPeer));
    portEXIT_CRITICAL(&promptMux);
    if (!started) { error = 3; reject(); }
}
// The Arduino callback interface immediately replies to NC/passkey callbacks, so it
// cannot wait for a human without blocking the Bluetooth task. Its public GAP hook
// lets the main loop supply esp_ble_confirm_reply / esp_ble_passkey_reply later.
void gapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT:
        if (isCurrentPeer(param->ble_security.ble_req.bd_addr))
            Serial.printf("CARDKEY_BLE=security_request window=%u\n", pairingAllowed() ? 1U : 0U);
        // Arduino accepts this request with no security callbacks. Fresh pairing
        // still requires an in-window challenge and authenticated SC+MITM completion.
        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        beginPrompt(PairingKind::Entry, 0, param->ble_security.ble_req.bd_addr);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        beginPrompt(PairingKind::Comparison, param->ble_security.key_notif.passkey,
                    param->ble_security.key_notif.bd_addr);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        beginPrompt(PairingKind::Display, param->ble_security.key_notif.passkey,
                    param->ble_security.key_notif.bd_addr);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        const auto& result = param->ble_security.auth_cmpl;
        if (!isCurrentPeer(result.bd_addr)) break;
        const uint8_t required = ESP_LE_AUTH_REQ_SC_MITM;
        const bool window = pairingAllowed(), fresh = newPairAttempt.load();
        const auto pending = promptStatus().kind;
        const bool allowed = !rejectNeeded.load() && !disconnectSent.load() &&
            pending != PairingKind::Entry && pending != PairingKind::Comparison &&
            authenticationAllowed(result.success, (result.auth_mode & required) == required, fresh, window);
        Serial.printf("CARDKEY_BLE=auth success=%u mode=%u reason=%u new=%u window=%u accepted=%u\n",
            result.success ? 1U : 0U, unsigned(result.auth_mode), unsigned(result.fail_reason),
            fresh ? 1U : 0U, window ? 1U : 0U, allowed ? 1U : 0U);
        clearPrompt();
        if (!allowed) { error = 4; reject(); break; }
        authenticated = true; pairEnabled = false; error = 0; sendError = 0;
        break;
    }
    default: break;
    }
}

// Standard 8-byte keyboard input and 1-byte LED output; report ID lives in GATT 0x2908,
// and is not prefixed to the characteristic payload. Based on official HID keyboard usages.
uint8_t reportMap[] = {
    0x05,0x01, 0x09,0x06, 0xa1,0x01, 0x85,0x01,
    0x05,0x07, 0x19,0xe0, 0x29,0xe7, 0x15,0x00, 0x25,0x01,
    0x75,0x01, 0x95,0x08, 0x81,0x02,
    0x95,0x01, 0x75,0x08, 0x81,0x01,
    0x95,0x05, 0x75,0x01, 0x05,0x08, 0x19,0x01, 0x29,0x05, 0x91,0x02,
    0x95,0x01, 0x75,0x03, 0x91,0x01,
    0x95,0x06, 0x75,0x08, 0x15,0x00, 0x25,0x65,
    0x05,0x07, 0x19,0x00, 0x29,0x65, 0x81,0x00, 0xc0
};

class SubscriptionCallbacks : public BLEDescriptorCallbacks {
public:
    explicit SubscriptionCallbacks(std::atomic<bool>& target) : target_(target) {}
    void onWrite(BLEDescriptor* descriptor) override {
        target_ = descriptor->getLength() == 2 && (descriptor->getValue()[0] & 1);
    }
private:
    std::atomic<bool>& target_;
};
SubscriptionCallbacks reportSubscription(subscribed), bootSubscription(bootSubscribed);

class LedCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        const auto value = characteristic->getValue();
        if (value.size() == 1) capsLock = (uint8_t(value[0]) & 2) != 0;
    }
} ledCallbacks;
class ModeCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        const auto value = characteristic->getValue();
        if (value.size() == 1 && uint8_t(value[0]) <= 1) bootMode = value[0] == 0;
    }
} modeCallbacks;
class ControlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        const auto value = characteristic->getValue();
        if (value.size() == 1 && uint8_t(value[0]) <= 1) suspended = value[0] == 0;
    }
} controlCallbacks;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* active, esp_ble_gatts_cb_param_t* param) override {
        if (connected.load()) { active->disconnect(param->connect.conn_id); return; }
        authenticated = false; subscribed = false; bootSubscribed = false;
        bootMode = false; suspended = false; capsLock = false;
        newPairAttempt = false;
        rejectNeeded = false; disconnectSent = false; error = 0; sendError = 0;
        portENTER_CRITICAL(&promptMux);
        prompt.clear(); std::memcpy(currentPeer, param->connect.remote_bda, sizeof(currentPeer));
        std::memset(pendingPeer, 0, sizeof(pendingPeer));
        portEXIT_CRITICAL(&promptMux);
        ++linkGeneration;
        uint8_t reportProtocol = 1;
        hid->protocolMode()->setValue(&reportProtocol, 1);
        connectionId = param->connect.conn_id; linkStarted = millis(); connected = true;
        Serial.printf("CARDKEY_BLE=connected id=%u\n", unsigned(param->connect.conn_id));
        // BLEDevice::setEncryptionLevel starts encryption before this callback.
        // Do not enqueue a duplicate request while the phone is negotiating.
    }
    void onDisconnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
        if (param->disconnect.conn_id != connectionId.load()) return;
        connected = false; authenticated = false; subscribed = false; bootSubscribed = false;
        ++linkGeneration;
        clearPrompt(); rejectNeeded = false; disconnectSent = false;
        suspended = false; advertisingNeeded = true;
        Serial.printf("CARDKEY_BLE=disconnected reason=%u\n", unsigned(param->disconnect.reason));
    }
} serverCallbacks;

void configureSubscription(BLECharacteristic* characteristic, SubscriptionCallbacks* callbacks) {
    auto* descriptor = characteristic->getDescriptorByUUID(BLEUUID(uint16_t(0x2902)));
    descriptor->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
    descriptor->setCallbacks(callbacks);
}
}

bool BleKeyboard::begin() {
    // Official Arduino-ESP32 BLEHIDDevice supplies GATT services and report reference descriptors.
    // https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/BLE/src/BLEHIDDevice.cpp
    BLEDevice::init("Cardputer Keyboard");
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) return false;
    // Public event hook supplies the interface; BLEServer::getGattsIf is private in 2.0.17.
    BLEDevice::setCustomGattsHandler(gattsEvent);
    BLEDevice::setCustomGapHandler(gapEvent);
    server = BLEDevice::createServer();
    server->setCallbacks(&serverCallbacks);
    BLEDevice::setSecurityCallbacks(nullptr);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    BLESecurity security;
    security.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    security.setCapability(ESP_IO_CAP_KBDISP);
    security.setKeySize(16);
    security.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    uint8_t onlySpecified = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;
    if (esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH,
                                      &onlySpecified, sizeof(onlySpecified)) != ESP_OK) return false;
    hid = new BLEHIDDevice(server);
    input = hid->inputReport(1);
    input->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
    configureSubscription(input, &reportSubscription);
    auto* output = hid->outputReport(1);
    output->setCallbacks(&ledCallbacks);
    uint8_t noLeds = 0;
    output->setValue(&noLeds, 1);
    bootInput = hid->bootInput();
    bootInput->setReadProperty(true);
    bootInput->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
    configureSubscription(bootInput, &bootSubscription);
    auto* bootOutput = hid->bootOutput();
    bootOutput->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM);
    bootOutput->setCallbacks(&ledCallbacks);
    bootOutput->setValue(&noLeds, 1);
    hid->protocolMode()->setCallbacks(&modeCallbacks);
    hid->hidControl()->setCallbacks(&controlCallbacks);
    if (!initializeHidManufacturer(*hid)) return false;
    // Unassigned USB vendor/product; declare a complete PnP value without claiming an OEM ID.
    uint8_t pnp[] = {2, 0, 0, 0, 0, 0, 1};
    hid->deviceInfo()->getCharacteristic(BLEUUID(uint16_t(0x2a50)))->setValue(pnp, sizeof(pnp));
    hid->hidInfo(0, 2);
    hid->reportMap(reportMap, sizeof(reportMap));
    uint8_t zeros[8] = {};
    input->setValue(zeros, sizeof(zeros)); bootInput->setValue(zeros, sizeof(zeros));
    hid->startServices();
    auto* advertising = BLEDevice::getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06); advertising->setMaxPreferred(0x12);
    pairStarted = millis();
    pairEnabled = esp_ble_get_bond_device_num() == 0;
    advertising->start();
    return true;
}

void BleKeyboard::poll() {
    if (pairEnabled.load() && !pairingAllowed()) {
        pairEnabled = false;
        if (promptStatus().kind != PairingKind::None) rejectNeeded = true;
    }
    const bool timeout = connected.load() && !authenticated.load() && uint32_t(millis() - linkStarted.load()) > 30000U;
    if ((rejectNeeded.load() || timeout) && connected.load() && !disconnectSent.exchange(true)) {
        authenticated = false;
        clearPrompt();
        server->disconnect(connectionId.load());
    }
    if (advertisingNeeded.exchange(false)) BLEDevice::getAdvertising()->start();
}
void BleKeyboard::openPairing() {
    pairStarted = millis(); pairEnabled = true; error = 0; sendError = 0;
    clearPrompt();
    if (connected.load()) {
        authenticated = false; disconnectSent = true;
        server->disconnect(connectionId.load());
    }
    else BLEDevice::getAdvertising()->start();
}
bool BleKeyboard::pairingInput(uint64_t pressed) {
    esp_bd_addr_t peer{};
    portENTER_CRITICAL(&promptMux);
    const uint32_t generation = linkGeneration.load();
    const auto result = prompt.update(pressed, millis(), pairingAllowed());
    std::memcpy(peer, pendingPeer, sizeof(peer));
    if (result.reply != PairingReply::None) std::memset(pendingPeer, 0, sizeof(pendingPeer));
    portEXIT_CRITICAL(&promptMux);
    if (result.reply == PairingReply::None || generation != linkGeneration.load() || !isCurrentPeer(peer))
        return result.consumed;
    const bool accept = result.reply == PairingReply::Accept;
    esp_err_t sent = ESP_OK;
    if (result.kind == PairingKind::Comparison) sent = esp_ble_confirm_reply(peer, accept);
    else if (result.kind == PairingKind::Entry) sent = esp_ble_passkey_reply(peer, accept, result.passkey);
    else if (!accept) reject();
    Serial.printf("CARDKEY_BLE=pairing_reply type=%s accepted=%u result=%d\n",
        result.kind == PairingKind::Entry ? "entry" : result.kind == PairingKind::Comparison ? "compare" : "display",
        accept ? 1U : 0U, int(sent));
    if (!accept || sent != ESP_OK) { error = 6; reject(); }
    return result.consumed;
}
bool BleKeyboard::ready() const {
    return hidReady(connected.load(), authenticated.load(),
                    bootMode.load() ? bootSubscribed.load() : subscribed.load(), suspended.load());
}
bool BleKeyboard::send(const HidReport& report) {
    if (!ready()) return false;
    auto* characteristic = bootMode.load() ? bootInput : input;
    auto bytes = report.bytes;
    characteristic->setValue(bytes.data(), bytes.size());
    // Direct official GATT API returns enqueue errors. BLECharacteristic::notify() defaults
    // to indications in 2.0.17 and does not return send success; never rely on that default.
    const auto result = esp_ble_gatts_send_indicate(gattInterface.load(), connectionId.load(),
        characteristic->getHandle(), bytes.size(), bytes.data(), false);
    if (result != ESP_OK) {
        if (sendError.exchange(result) != result)
            Serial.printf("CARDKEY_BLE=send_failed result=%d interface=%u id=%u handle=%u\n",
                int(result), unsigned(gattInterface.load()), unsigned(connectionId.load()), unsigned(characteristic->getHandle()));
        error = 5; return false;
    }
    if (sendError.exchange(0) != 0) error = 0;
    return true;
}
BleKeyboardStatus BleKeyboard::status() const {
    BleKeyboardStatus result;
    result.connected = connected.load(); result.authenticated = authenticated.load();
    result.subscribed = bootMode.load() ? bootSubscribed.load() : subscribed.load();
    result.pairing = pairingAllowed(); result.prompt = promptStatus();
    result.capsLock = capsLock.load(); result.error = error.load(); result.sendError = sendError.load();
    const uint32_t elapsed = uint32_t(millis() - pairStarted.load());
    if (result.pairing && elapsed < 180000U) result.pairingSeconds = (180000U - elapsed + 999U) / 1000U;
    return result;
}
void BleKeyboard::battery(uint8_t percent) { if (hid) hid->setBatteryLevel(percent > 100 ? 100 : percent); }
