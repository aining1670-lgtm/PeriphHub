#include "royuan_hid.h"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

namespace royuan {
namespace {

std::atomic<bool> g_force_bulk_keymap{false};

const GUID kHidClassGuid = {0x4D1E55B2, 0xF16F, 0x11CF,
                            {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

std::string Hex4(std::uint16_t v) {
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(v));
    return buf;
}

std::string Hex2(std::uint8_t v) {
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned>(v));
    return buf;
}

}  // namespace

Device::~Device() {
    Close();
}

void Device::Close() {
    std::lock_guard<std::mutex> guard(lock_);
    if (handle_ != nullptr) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
    vendor_id_ = 0;
    product_id_ = 0;
    is_wireless_ = false;
}

bool Device::Open(std::uint16_t preferred_pid, std::string* error) {
    Close();

    HDEVINFO set = SetupDiGetClassDevsW(&kHidClassGuid, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) {
        if (error != nullptr) *error = "SetupDiGetClassDevs failed";
        return false;
    }

    HANDLE found_handle = nullptr;
    std::uint16_t found_vid = 0;
    std::uint16_t found_pid = 0;

    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA ifdata{};
        ifdata.cbSize = sizeof(ifdata);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &kHidClassGuid, index, &ifdata)) {
            break;
        }
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifdata, nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<BYTE> buf(required);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifdata, detail, required, nullptr, nullptr)) {
            continue;
        }

        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attr{};
        attr.Size = sizeof(attr);
        PHIDP_PREPARSED_DATA pp = nullptr;
        HIDP_CAPS caps{};
        const bool ok = HidD_GetAttributes(h, &attr) &&
                        attr.VendorID == kVid &&
                        (preferred_pid == 0u || attr.ProductID == preferred_pid) &&
                        HidD_GetPreparsedData(h, &pp) &&
                        HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS;
        if (pp != nullptr) HidD_FreePreparsedData(pp);

        if (ok && caps.UsagePage == kUsagePage && caps.Usage == kUsage &&
            caps.FeatureReportByteLength == kPacketLen) {
            found_handle = h;
            found_vid = attr.VendorID;
            found_pid = attr.ProductID;
            break;
        }
        CloseHandle(h);
    }

    SetupDiDestroyDeviceInfoList(set);

    if (found_handle == nullptr) {
        if (error != nullptr) *error = "device not found";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    handle_ = found_handle;
    vendor_id_ = found_vid;
    product_id_ = found_pid;
    is_wireless_ = IsDonglePid(found_pid);
    return true;
}

bool Device::RawSetFeature(const std::uint8_t data[kPacketLen]) {
    HANDLE h = static_cast<HANDLE>(handle_);
    if (h == nullptr) return false;
    std::uint8_t buf[kPacketLen]{};
    std::memcpy(buf, data, kPacketLen);
    return HidD_SetFeature(h, buf, static_cast<ULONG>(kPacketLen)) != FALSE;
}

bool Device::RawGetFeature(std::uint8_t reply[kPacketLen]) {
    HANDLE h = static_cast<HANDLE>(handle_);
    if (h == nullptr) return false;
    std::memset(reply, 0, kPacketLen);
    return HidD_GetFeature(h, reply, static_cast<ULONG>(kPacketLen)) != FALSE;
}

bool Device::ExchangeFeature(const std::uint8_t request[kPacketLen],
                             std::uint8_t reply[kPacketLen],
                             bool want_reply,
                             std::string* error) {
    if (handle_ == nullptr) {
        if (error != nullptr) *error = "device not open";
        return false;
    }

    if (!is_wireless_) {
        if (!RawSetFeature(request)) {
            if (error != nullptr) {
                *error = "HidD_SetFeature failed for " + Hex2(request[1]);
            }
            return false;
        }
        Sleep(35);
        if (want_reply && reply != nullptr) {
            if (!RawGetFeature(reply)) {
                if (error != nullptr) {
                    *error = "HidD_GetFeature failed for " + Hex2(request[1]);
                }
                return false;
            }
        }
        return true;
    }

    // 2.4G receiver relay flow:
    // 1. Poll 0xF7 until keyboard is online and dongle can accept relay (p[5] == 1).
    std::uint8_t f7_req[kPacketLen]{};
    BuildDongleStatusPacket(f7_req);
    bool relay_ready = false;
    bool seen_paired = false;
    bool seen_online = false;

    for (int attempt = 0; attempt < 5; ++attempt) {
        std::uint8_t f7_rep[kPacketLen]{};
        if (RawSetFeature(f7_req)) {
            Sleep(25);
            if (RawGetFeature(f7_rep)) {
                DongleStatus st;
                if (ParseDongleStatus(f7_rep, &st)) {
                    seen_paired = true;
                    seen_online = st.online;
                    if (!st.online) {
                        if (error != nullptr) {
                            *error = "device sleeping (press any key on the keyboard to wake it)";
                        }
                        return false;
                    }
                    if (st.can_accept_relay) {
                        relay_ready = true;
                        break;
                    }
                }
            }
        }
        if (attempt + 1 < 5) Sleep(50);
    }

    if (!relay_ready) {
        if (error != nullptr) {
            if (!seen_paired) {
                *error = "2.4G dongle is not paired with a keyboard";
            } else if (!seen_online) {
                *error = "device sleeping (press any key on the keyboard to wake it)";
            } else {
                *error = "2.4G receiver relay busy";
            }
        }
        return false;
    }

    // 2. Select keyboard relay target (0xF6, 10).
    std::uint8_t sel_req[kPacketLen]{};
    BuildRelaySelectPacket(kRelayTargetKeyboard, sel_req);
    if (!RawSetFeature(sel_req)) {
        if (error != nullptr) *error = "2.4G relay select (0xF6) failed";
        return false;
    }
    Sleep(20);

    // 3. Send target packet.
    if (!RawSetFeature(request)) {
        if (error != nullptr) *error = "2.4G relayed write failed for " + Hex2(request[1]);
        return false;
    }
    Sleep(35);

    if (!want_reply || reply == nullptr) {
        return true;
    }

    // 4. Poll 0xF7 until reply_ready (p[0] == 1), then release (0xFC) & GetFeature.
    bool has_reply = false;
    for (int attempt = 0; attempt < 6; ++attempt) {
        std::uint8_t f7_rep[kPacketLen]{};
        if (RawSetFeature(f7_req)) {
            Sleep(25);
            if (RawGetFeature(f7_rep)) {
                DongleStatus st;
                if (ParseDongleStatus(f7_rep, &st) && st.reply_ready) {
                    has_reply = true;
                    break;
                }
            }
        }
        if (attempt + 1 < 6) Sleep(50);
    }
    if (!has_reply) {
        if (error != nullptr) {
            *error = "2.4G relayed reply timeout for " + Hex2(request[1]) +
                     " (press any key on the keyboard to wake it)";
        }
        return false;
    }

    std::uint8_t rel_req[kPacketLen]{};
    BuildRelayReleasePacket(rel_req);
    if (!RawSetFeature(rel_req)) {
        if (error != nullptr) *error = "2.4G relay release (0xFC) failed";
        return false;
    }
    Sleep(20);

    if (!RawGetFeature(reply)) {
        if (error != nullptr) *error = "2.4G relayed GetFeature failed for " + Hex2(request[1]);
        return false;
    }
    return true;
}

bool Device::EnsureSupportedModelUnlocked(std::uint16_t* dev_id_out, std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetDevIdPacket(req);

    std::uint16_t dev_id = 0;
    std::string last_err;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        std::string ex_err;
        if (ExchangeFeature(req, rep, true, &ex_err)) {
            if (ParseDevIdReply(rep, &dev_id)) {
                break;
            }
            last_err = "0x8F device ID reply invalid";
        } else {
            last_err = ex_err;
        }
        if (dev_id != 0) break;
        if (attempt + 1 < 3) Sleep(40);
    }

    if (dev_id == 0) {
        if (error != nullptr) {
            *error = last_err.empty() ? "failed to read 0x8F device ID" : last_err;
        }
        return false;
    }
    if (!IsSupportedYc500Board(dev_id)) {
        if (error != nullptr) {
            *error = "unsupported ROYUAN device ID " + Hex4(dev_id) +
                     " (only AKOS BC98 / 0x03AE is supported)";
        }
        return false;
    }
    if (dev_id_out != nullptr) *dev_id_out = dev_id;
    return true;
}

bool Device::EnsureSupportedModel(std::uint16_t* dev_id_out, std::string* error) {
    std::lock_guard<std::mutex> guard(lock_);
    return EnsureSupportedModelUnlocked(dev_id_out, error);
}

bool Device::ReadFirmwareUnlocked(std::string* version_out) {
    std::uint8_t req[kPacketLen]{};
    BuildGetFirmwarePacket(req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        if (ExchangeFeature(req, rep, true, nullptr) && ParseFirmwareReply(rep, version_out)) {
            return true;
        }
        if (attempt + 1 < 3) Sleep(40);
    }
    return false;
}

bool Device::ReadProfileUnlocked(int* profile_out, std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetProfilePacket(req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        if (ExchangeFeature(req, rep, true, error) && ParseProfileReply(rep, profile_out)) {
            return true;
        }
        if (attempt + 1 < 3) Sleep(40);
    }
    if (error != nullptr && error->empty()) *error = "failed to read 0x85 profile";
    return false;
}

bool Device::ReadOptionsUnlocked(KbOptions* out, RawKbOptionsFrame* raw_out, std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetKbOptionsPacket(req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        if (ExchangeFeature(req, rep, true, error) && ParseKbOptionsReply(rep, out, raw_out)) {
            return true;
        }
        if (attempt + 1 < 3) Sleep(40);
    }
    if (error != nullptr && error->empty()) *error = "failed to read 0x86 options";
    return false;
}

bool Device::ReadLedUnlocked(bool side, LedParams* out, std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetLedPacket(side, req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        if (ExchangeFeature(req, rep, true, error) && ParseLedReply(side, rep, out)) {
            return true;
        }
        if (attempt + 1 < 3) Sleep(40);
    }
    if (error != nullptr && error->empty()) {
        *error = side ? "failed to read 0x88 side LED" : "failed to read 0x87 main LED";
    }
    return false;
}

bool Device::ReadDebounceUnlocked(int* level_out, std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetDebouncePacket(req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        if (ExchangeFeature(req, rep, true, error) && ParseDebounceReply(rep, level_out)) {
            return true;
        }
        if (attempt + 1 < 3) Sleep(40);
    }
    if (error != nullptr && error->empty()) *error = "failed to read 0x91 debounce";
    return false;
}

bool Device::ReadSleepUnlocked(SleepTimers* out, std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetSleepPacket(req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::uint8_t rep[kPacketLen]{};
        if (ExchangeFeature(req, rep, true, error) && ParseSleepReply(rep, out)) {
            return true;
        }
        if (attempt + 1 < 3) Sleep(40);
    }
    if (error != nullptr && error->empty()) *error = "failed to read 0x92 sleep timers";
    return false;
}

bool Device::ReadFullStateUnlocked(FullState* out, std::string* error) {
    if (out == nullptr) return false;
    *out = FullState{};

    if (!EnsureSupportedModelUnlocked(&out->dev_id, error)) {
        return false;
    }

    ReadFirmwareUnlocked(&out->firmware);

    std::string sub_err;
    out->have_profile = ReadProfileUnlocked(&out->profile, &sub_err);
    sub_err.clear();
    out->have_options = ReadOptionsUnlocked(&out->options, &out->raw_options, &sub_err);
    sub_err.clear();
    out->have_main_led = ReadLedUnlocked(false, &out->main_led, &sub_err);
    sub_err.clear();
    out->have_side_led = ReadLedUnlocked(true, &out->side_led, &sub_err);
    sub_err.clear();
    out->have_debounce = ReadDebounceUnlocked(&out->debounce, &sub_err);
    sub_err.clear();
    out->have_sleep = ReadSleepUnlocked(&out->sleep, &sub_err);
    return true;
}

bool Device::ReadFullState(FullState* out, std::string* error) {
    std::lock_guard<std::mutex> guard(lock_);
    return ReadFullStateUnlocked(out, error);
}

bool Device::ApplyPatchVerified(const StatePatch& patch, FullState* after_out, std::string* error) {
    if (!patch.Any()) {
        if (error != nullptr) *error = "empty keyboard patch";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    if (patch.have_profile) {
        if (!ProfileValid(patch.profile)) {
            if (error != nullptr) *error = "profile must be 0..3";
            return false;
        }
        std::uint8_t req[kPacketLen]{};
        BuildSetProfilePacket(patch.profile, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        Sleep(60);
        int rb_profile = -1;
        bool matched = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            std::string rerr;
            if (ReadProfileUnlocked(&rb_profile, &rerr) && rb_profile == patch.profile) {
                matched = true;
                break;
            }
            if (attempt + 1 < 4) Sleep(50);
        }
        if (!matched) {
            if (error != nullptr) {
                *error = "readback mismatch: wrote profile " + std::to_string(patch.profile) +
                         ", got " + (rb_profile >= 0 ? std::to_string(rb_profile) : "unread");
            }
            return false;
        }
    }

    if (patch.have_options && patch.options.Any()) {
        KbOptions before_opt{};
        RawKbOptionsFrame base_raw{};
        if (!ReadOptionsUnlocked(&before_opt, &base_raw, error)) {
            return false;
        }
        std::uint8_t req[kPacketLen]{};
        BuildSetKbOptionsPacket(base_raw, patch.options, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        Sleep(60);
        KbOptions after_opt{};
        bool read_any = false;
        bool matched = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            std::string rerr;
            if (ReadOptionsUnlocked(&after_opt, nullptr, &rerr)) {
                read_any = true;
                if (KbOptionsMatchPatch(after_opt, patch.options)) {
                    matched = true;
                    break;
                }
            }
            if (attempt + 1 < 4) Sleep(50);
        }
        if (!matched) {
            if (error != nullptr) {
                *error = read_any ? "readback mismatch after 0x06 keyboard options write (got flags=" +
                                        Hex2(after_opt.raw_flags) + ")"
                                  : "0x86 readback failed after 0x06 write";
            }
            return false;
        }
    }

    auto apply_led = [&](bool side, const LedPatch& lpatch) -> bool {
        if (lpatch.have_mode) {
            const bool valid = side ? SideLedModeValid(lpatch.mode) : MainLedModeValid(lpatch.mode);
            if (!valid) {
                if (error != nullptr) *error = side ? "sideLed.mode must be 0..5" : "mainLed.mode must be 1..19 (excluding 13)";
                return false;
            }
        }
        if (lpatch.have_speed && !LedSpeedValid(lpatch.speed)) {
            if (error != nullptr) *error = "LED speed must be 0..4";
            return false;
        }
        if (lpatch.have_brightness && !LedBrightnessValid(lpatch.brightness)) {
            if (error != nullptr) *error = "LED brightness must be 0..4";
            return false;
        }
        if (lpatch.have_direction && !LedDirectionValid(lpatch.direction)) {
            if (error != nullptr) *error = "LED direction must be 0..3";
            return false;
        }
        if (lpatch.have_color &&
            (!RgbChannelValid(lpatch.r) || !RgbChannelValid(lpatch.g) || !RgbChannelValid(lpatch.b))) {
            if (error != nullptr) *error = "LED color RGB channels must be 0..255";
            return false;
        }

        LedParams base{};
        if (!ReadLedUnlocked(side, &base, error)) {
            return false;
        }
        const LedParams target = MergeLedPatch(base, lpatch, side);
        std::uint8_t req[kPacketLen]{};
        BuildSetLedPacket(side, target, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        Sleep(60);

        LedParams actual{};
        bool read_any = false;
        bool matched = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            std::string rerr;
            if (ReadLedUnlocked(side, &actual, &rerr)) {
                read_any = true;
                if (LedMatchesTarget(actual, target, side)) {
                    matched = true;
                    break;
                }
            }
            if (attempt + 1 < 4) Sleep(50);
        }
        if (!matched) {
            if (error != nullptr) {
                if (!read_any) {
                    *error = side ? "0x88 readback failed after 0x08 write"
                                  : "0x87 readback failed after 0x07 write";
                } else {
                    *error = std::string(side ? "sideLed" : "mainLed") +
                             " readback mismatch: wrote mode=" + std::to_string(target.mode) +
                             " bri=" + std::to_string(target.brightness) +
                             " spd=" + std::to_string(target.speed) +
                             ", got mode=" + std::to_string(actual.mode) +
                             " bri=" + std::to_string(actual.brightness) +
                             " spd=" + std::to_string(actual.speed);
                }
            }
            return false;
        }
        return true;
    };

    if (patch.have_main_led && patch.main_led.Any()) {
        if (!apply_led(false, patch.main_led)) return false;
    }
    if (patch.have_side_led && patch.side_led.Any()) {
        if (!apply_led(true, patch.side_led)) return false;
    }

    if (patch.have_debounce) {
        if (!DebounceValid(patch.debounce)) {
            if (error != nullptr) *error = "debounce must be 1..10";
            return false;
        }
        std::uint8_t req[kPacketLen]{};
        BuildSetDebouncePacket(patch.debounce, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        Sleep(60);
        int rb_deb = -1;
        bool matched = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            std::string rerr;
            if (ReadDebounceUnlocked(&rb_deb, &rerr) && rb_deb == patch.debounce) {
                matched = true;
                break;
            }
            if (attempt + 1 < 4) Sleep(50);
        }
        if (!matched) {
            if (error != nullptr) {
                *error = "readback mismatch: wrote debounce " + std::to_string(patch.debounce) +
                         ", got " + (rb_deb >= 0 ? std::to_string(rb_deb) : "unread");
            }
            return false;
        }
    }

    if (patch.have_sleep && patch.sleep.Any()) {
        if ((patch.sleep.have_sleep_bt && !SleepSecValid(patch.sleep.sleep_bt)) ||
            (patch.sleep.have_sleep_24g && !SleepSecValid(patch.sleep.sleep_24g)) ||
            (patch.sleep.have_deep_bt && !SleepSecValid(patch.sleep.deep_bt)) ||
            (patch.sleep.have_deep_24g && !SleepSecValid(patch.sleep.deep_24g))) {
            if (error != nullptr) *error = "sleep timers must be 60..3600 seconds";
            return false;
        }
        SleepTimers base_sleep{};
        if (!ReadSleepUnlocked(&base_sleep, error)) {
            return false;
        }
        const SleepTimers target = MergeSleepPatch(base_sleep, patch.sleep);
        std::uint8_t req[kPacketLen]{};
        BuildSetSleepPacket(target, req, true);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        Sleep(60);

        SleepTimers actual{};
        bool read_any = false;
        bool matched = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            std::string rerr;
            if (ReadSleepUnlocked(&actual, &rerr)) {
                read_any = true;
                if (SleepMatchesTarget(actual, target)) {
                    matched = true;
                    break;
                }
            }
            if (attempt + 1 < 4) Sleep(50);
        }
        if (!matched) {
            if (error != nullptr) {
                *error = read_any
                             ? "readback mismatch: wrote sleep=(" +
                                   std::to_string(target.sleep_bt) + "," +
                                   std::to_string(target.sleep_24g) + "," +
                                   std::to_string(target.deep_bt) + "," +
                                   std::to_string(target.deep_24g) + ") got=(" +
                                   std::to_string(actual.sleep_bt) + "," +
                                   std::to_string(actual.sleep_24g) + "," +
                                   std::to_string(actual.deep_bt) + "," +
                                   std::to_string(actual.deep_24g) + ")"
                             : "0x92 readback failed after 0x12 write";
            }
            return false;
        }
    }

    if (after_out != nullptr) {
        std::string read_err;
        if (!ReadFullStateUnlocked(after_out, &read_err)) {
            *after_out = FullState{};
            after_out->dev_id = dev_id;
        }
    }
    return true;
}

bool Device::ExchangeRawPage(const std::uint8_t request[kPacketLen],
                             std::uint8_t page_out[kPayloadLen],
                             std::string* error) {
    std::uint8_t rep[kPacketLen]{};
    if (!ExchangeFeature(request, rep, true, error)) {
        return false;
    }
    if (page_out != nullptr) {
        std::memcpy(page_out, rep + 1, kPayloadLen);
    }
    return true;
}

bool Device::SwitchProfileUnlocked(int profile, std::string* error) {
    if (!ProfileValid(profile)) {
        if (error != nullptr) *error = "profile must be 0..3";
        return false;
    }
    std::uint8_t req[kPacketLen]{};
    BuildSetProfilePacket(profile, req);
    if (!ExchangeFeature(req, nullptr, false, error)) {
        return false;
    }
    Sleep(120);
    int rb_profile = -1;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::string rerr;
        if (ReadProfileUnlocked(&rb_profile, &rerr) && rb_profile == profile) {
            return true;
        }
        if (attempt + 1 < 4) Sleep(50);
    }
    if (error != nullptr) {
        *error = "failed to switch active profile to " + std::to_string(profile) +
                 " (got " + (rb_profile >= 0 ? std::to_string(rb_profile) : "unread") + ")";
    }
    return false;
}

bool Device::ReadKeymapPageUnlocked(int profile,
                                    int page,
                                    bool fn_layer,
                                    std::uint8_t page_out[kPayloadLen],
                                    std::string* error) {
    std::uint8_t req[kPacketLen]{};
    BuildGetKeymapPagePacket(profile, page, fn_layer, req);
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string ex_err;
        if (ExchangeRawPage(req, page_out, &ex_err)) {
            return true;
        }
        if (error != nullptr) *error = ex_err;
        if (attempt + 1 < 3) Sleep(30);
    }
    return false;
}

bool Device::ReadKeymapLayerUnlocked(int profile,
                                     bool fn_layer,
                                     KeyMatrix* out,
                                     std::string* error) {
    if (out == nullptr) return false;
    std::memset(out->bytes, 0, sizeof(out->bytes));
    for (int page = 0; page < kKeymapReadPages; ++page) {
        if (!ReadKeymapPageUnlocked(profile, page, fn_layer,
                                    out->bytes + static_cast<std::size_t>(page) * kPayloadLen,
                                    error)) {
            return false;
        }
        if (page + 1 < kKeymapReadPages) Sleep(15);
    }
    return true;
}

bool Device::WriteKeymapLayerBulkUnlocked(int profile,
                                          bool fn_layer,
                                          const KeyMatrix& target,
                                          std::string* error) {
    for (int page = 0; page < kKeymapBulkPages; ++page) {
        std::uint8_t req[kPacketLen]{};
        BuildBulkKeymapPagePacket(profile, page, fn_layer, target, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        Sleep(100);
    }
    Sleep(400);
    return true;
}

bool Device::ReadKeymap(int profile,
                        KeyMatrix* base_out,
                        KeyMatrix* fn_out,
                        int* active_profile_out,
                        std::string* error) {
    if (profile >= 0 && !ProfileValid(profile)) {
        if (error != nullptr) *error = "profile must be 0..3";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    int target_profile = profile;
    int active_profile = -1;
    if (!ReadProfileUnlocked(&active_profile, error)) {
        if (target_profile < 0) return false;
    } else if (target_profile < 0) {
        target_profile = active_profile;
    }
    if (active_profile_out != nullptr) *active_profile_out = target_profile;

    if (base_out != nullptr && !ReadKeymapLayerUnlocked(target_profile, false, base_out, error)) {
        return false;
    }
    if (fn_out != nullptr && !ReadKeymapLayerUnlocked(target_profile, true, fn_out, error)) {
        return false;
    }
    return true;
}

bool Device::WriteKeySlotVerified(int profile,
                                  bool fn_layer,
                                  int slot,
                                  const KeyEntry& entry,
                                  KeyEntry* readback_out,
                                  bool* bulk_used_out,
                                  std::string* error) {
    if (profile >= 0 && !ProfileValid(profile)) {
        if (error != nullptr) *error = "profile must be 0..3";
        return false;
    }
    if (!KeySlotValid(slot)) {
        if (error != nullptr) *error = "slot must be 0..125";
        return false;
    }
    if (!KeyEntryValid(entry, fn_layer, error)) {
        return false;
    }
    if (fn_layer && slot == kKeymapFnPhysicalSlot) {
        if (error != nullptr) *error = "cannot remap the Fn key (slot 65) on the Fn layer";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    int active_profile = -1;
    if (!ReadProfileUnlocked(&active_profile, error)) {
        return false;
    }
    int target_profile = (profile >= 0) ? profile : active_profile;
    if (target_profile != active_profile) {
        if (!SwitchProfileUnlocked(target_profile, error)) {
            return false;
        }
    }

    // Protect Fn key: do not overwrite slot 65 away from Fn ([10,1,0,0]) unless
    // another base-layer slot already holds Fn.
    if (!fn_layer && slot == kKeymapFnPhysicalSlot && !(entry.b[0] == 10u && entry.b[1] == 1u)) {
        KeyMatrix cur_base;
        if (!ReadKeymapLayerUnlocked(target_profile, false, &cur_base, error)) {
            return false;
        }
        bool has_other_fn = false;
        for (int s = 0; s <= kKeymapWriteMaxSlot; ++s) {
            if (s == slot) continue;
            const KeyEntry other = cur_base.Slot(s);
            if (other.b[0] == 10u && other.b[1] == 1u && other.b[2] == 0u && other.b[3] == 0u) {
                has_other_fn = true;
                break;
            }
        }
        if (!has_other_fn) {
            if (error != nullptr) {
                *error = "cannot overwrite the last Fn key (assign Fn to another key first)";
            }
            return false;
        }
    }

    const int page_idx = slot / 16;
    const int slot_in_page = slot % 16;
    auto read_slot_from_page = [&](KeyEntry* out_entry, std::uint8_t reject_echo_op, std::string* err) -> bool {
        std::uint8_t page_buf[kPayloadLen]{};
        if (!ReadKeymapPageUnlocked(target_profile, page_idx, fn_layer, page_buf, err)) {
            return false;
        }
        if (reject_echo_op != 0u && page_buf[0] == reject_echo_op) {
            if (err != nullptr) *err = "stale write opcode echo during keymap readback";
            return false;
        }
        std::memcpy(out_entry->b, page_buf + static_cast<std::size_t>(slot_in_page) * 4u, 4);
        return true;
    };

    if (!g_force_bulk_keymap.load()) {
        std::uint8_t req[kPacketLen]{};
        BuildSetKeyOnePacket(target_profile, slot, fn_layer, entry, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        const int waits[3] = {50, 80, 150};
        for (int w = 0; w < 3; ++w) {
            Sleep(static_cast<DWORD>(waits[w]));
            KeyEntry got;
            std::string rerr;
            if (read_slot_from_page(&got, req[1], &rerr) && got == entry) {
                if (readback_out != nullptr) *readback_out = got;
                if (bulk_used_out != nullptr) *bulk_used_out = false;
                return true;
            }
        }
        g_force_bulk_keymap.store(true);
    }

    // Whole-layer fallback (0x09 / 0x10).
    KeyMatrix layer;
    if (!ReadKeymapLayerUnlocked(target_profile, fn_layer, &layer, error)) {
        return false;
    }
    layer.SetSlot(slot, entry);
    if (!WriteKeymapLayerBulkUnlocked(target_profile, fn_layer, layer, error)) {
        return false;
    }

    KeyEntry actual;
    bool read_any = false;
    bool matched = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string rerr;
        if (read_slot_from_page(&actual, 0u, &rerr)) {
            read_any = true;
            if (actual == entry) {
                matched = true;
                break;
            }
        }
        if (attempt + 1 < 3) Sleep(60);
    }
    if (!matched) {
        if (error != nullptr) {
            if (!read_any) {
                *error = "keymap readback failed after bulk write";
            } else {
                *error = "readback mismatch at slot " + std::to_string(slot) +
                         ": wrote [" + std::to_string(entry.b[0]) + "," + std::to_string(entry.b[1]) +
                         "," + std::to_string(entry.b[2]) + "," + std::to_string(entry.b[3]) +
                         "], got [" + std::to_string(actual.b[0]) + "," + std::to_string(actual.b[1]) +
                         "," + std::to_string(actual.b[2]) + "," + std::to_string(actual.b[3]) + "]";
            }
        }
        return false;
    }

    if (readback_out != nullptr) *readback_out = actual;
    if (bulk_used_out != nullptr) *bulk_used_out = true;
    return true;
}

bool Device::WriteKeymapLayerBulkVerified(int profile,
                                          bool fn_layer,
                                          const KeyMatrix& target,
                                          KeyMatrix* after_out,
                                          std::string* error) {
    if (profile >= 0 && !ProfileValid(profile)) {
        if (error != nullptr) *error = "profile must be 0..3";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    int active_profile = -1;
    if (!ReadProfileUnlocked(&active_profile, error)) {
        return false;
    }
    int target_profile = (profile >= 0) ? profile : active_profile;
    if (target_profile != active_profile) {
        if (!SwitchProfileUnlocked(target_profile, error)) {
            return false;
        }
    }

    // Preserve slots 126..127 (unwritten by 9x56=504B bulk upload).
    KeyMatrix current_before;
    if (!ReadKeymapLayerUnlocked(target_profile, fn_layer, &current_before, error)) {
        return false;
    }

    if (!WriteKeymapLayerBulkUnlocked(target_profile, fn_layer, target, error)) {
        return false;
    }

    KeyMatrix actual;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!ReadKeymapLayerUnlocked(target_profile, fn_layer, &actual, error)) {
            return false;
        }
        bool ok = true;
        int first_mismatch = -1;
        for (int s = 0; s <= kKeymapWriteMaxSlot; ++s) {
            if (actual.Slot(s) != target.Slot(s)) {
                ok = false;
                first_mismatch = s;
                break;
            }
        }
        if (ok) {
            if (after_out != nullptr) *after_out = actual;
            return true;
        }
        if (attempt == 0) {
            Sleep(120);
            continue;
        }
        if (error != nullptr) {
            const KeyEntry w = target.Slot(first_mismatch);
            const KeyEntry g = actual.Slot(first_mismatch);
            *error = "keymap bulk readback mismatch at slot " + std::to_string(first_mismatch) +
                     ": wrote [" + std::to_string(w.b[0]) + "," + std::to_string(w.b[1]) +
                     "," + std::to_string(w.b[2]) + "," + std::to_string(w.b[3]) +
                     "], got [" + std::to_string(g.b[0]) + "," + std::to_string(g.b[1]) +
                     "," + std::to_string(g.b[2]) + "," + std::to_string(g.b[3]) + "]";
        }
        return false;
    }
    return false;
}

bool Device::ReadMacroSlotRawUnlocked(int slot,
                                      std::uint8_t blob_out[kMacroBytes],
                                      std::string* error) {
    std::memset(blob_out, 0, kMacroBytes);
    for (int page = 0; page < kMacroReadPages; ++page) {
        std::uint8_t req[kPacketLen]{};
        BuildGetMacroPagePacket(slot, page, req);
        bool page_ok = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (ExchangeRawPage(req, blob_out + static_cast<std::size_t>(page) * kPayloadLen, error)) {
                page_ok = true;
                break;
            }
            if (attempt + 1 < 3) Sleep(30);
        }
        if (!page_ok) return false;
        if (page + 1 < kMacroReadPages) Sleep(15);
    }
    return true;
}

namespace {

int ComputeDecodedMacroByteLength(const MacroData& macro, const std::uint8_t raw[kMacroBytes]) {
    if (macro.events.empty()) return 0;
    int stream_bytes = 0;
    std::string ignore_err;
    std::uint8_t reencoded[kMacroBytes]{};
    if (EncodeMacroBlob(macro, reencoded, &ignore_err, &stream_bytes)) {
        return stream_bytes;
    }
    // Fallback if a legacy hardware macro omits a trailing key-up: scan to first 0 terminator.
    std::size_t i = 2;
    while (i + 1u < kMacroBytes) {
        const std::uint8_t code = raw[i];
        if (code == 0u) break;
        if (code == 0xF9u) {
            if (i + 3u >= kMacroBytes) break;
            i += 4u;
            continue;
        }
        const std::uint8_t low = static_cast<std::uint8_t>(raw[i + 1] & 0x7Fu);
        if (low != 0u) {
            i += 2u;
        } else {
            if (i + 3u >= kMacroBytes) break;
            i += 4u;
        }
    }
    return static_cast<int>(i >= 2u ? i - 2u : 0u);
}

}  // namespace

bool Device::ReadMacroSlot(int slot,
                           MacroData* out,
                           bool* empty_out,
                           std::uint8_t raw_out[kMacroBytes],
                           int* stream_bytes_out,
                           std::string* error) {
    if (!MacroSlotValid(slot)) {
        if (error != nullptr) *error = "macro slot must be 0..49";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    std::uint8_t blob[kMacroBytes]{};
    if (!ReadMacroSlotRawUnlocked(slot, blob, error)) {
        return false;
    }
    if (raw_out != nullptr) std::memcpy(raw_out, blob, kMacroBytes);

    bool empty = true;
    MacroData decoded = DecodeMacroBlob(blob, &empty);
    if (empty_out != nullptr) *empty_out = empty;
    if (stream_bytes_out != nullptr) {
        *stream_bytes_out = ComputeDecodedMacroByteLength(decoded, blob);
    }
    if (out != nullptr) *out = std::move(decoded);
    return true;
}

bool Device::ListMacroSummaries(int count,
                                std::vector<MacroSlotSummary>* out,
                                std::string* error) {
    if (count < 1 || count > kMacroSlotCount) {
        if (error != nullptr) *error = "macro list count must be 1..50";
        return false;
    }
    if (out == nullptr) return false;
    out->clear();

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    out->reserve(static_cast<std::size_t>(count));
    for (int slot = 0; slot < count; ++slot) {
        std::uint8_t req0[kPacketLen]{};
        BuildGetMacroPagePacket(slot, 0, req0);
        std::uint8_t blob[kMacroBytes]{};
        bool p0_ok = false;
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (ExchangeRawPage(req0, blob, error)) {
                p0_ok = true;
                break;
            }
            if (attempt + 1 < 3) Sleep(30);
        }
        if (!p0_ok) return false;

        MacroSlotSummary item;
        item.slot = slot;
        if (blob[2] == 0u) {
            item.empty = true;
            item.repeat = static_cast<int>(blob[0]) | (static_cast<int>(blob[1]) << 8);
            item.event_count = 0;
            item.byte_length = 0;
        } else {
            for (int page = 1; page < kMacroReadPages; ++page) {
                Sleep(15);
                std::uint8_t req[kPacketLen]{};
                BuildGetMacroPagePacket(slot, page, req);
                bool pok = false;
                for (int attempt = 0; attempt < 3; ++attempt) {
                    if (ExchangeRawPage(req, blob + static_cast<std::size_t>(page) * kPayloadLen, error)) {
                        pok = true;
                        break;
                    }
                    if (attempt + 1 < 3) Sleep(30);
                }
                if (!pok) return false;
            }
            bool empty = true;
            const MacroData decoded = DecodeMacroBlob(blob, &empty);
            item.empty = empty;
            item.repeat = decoded.repeat;
            item.event_count = static_cast<int>(decoded.events.size());
            item.byte_length = ComputeDecodedMacroByteLength(decoded, blob);
        }
        out->push_back(item);
        if (slot + 1 < count) Sleep(15);
    }
    return true;
}

bool Device::WriteMacroSlotVerified(int slot,
                                    const std::uint8_t target_blob[kMacroBytes],
                                    MacroData* after_out,
                                    bool* empty_out,
                                    int* stream_bytes_out,
                                    std::string* error) {
    if (!MacroSlotValid(slot)) {
        if (error != nullptr) *error = "macro slot must be 0..49";
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);
    std::uint16_t dev_id = 0;
    if (!EnsureSupportedModelUnlocked(&dev_id, error)) {
        return false;
    }

    std::uint8_t old_blob[kMacroBytes]{};
    if (!ReadMacroSlotRawUnlocked(slot, old_blob, error)) {
        return false;
    }

    const int pages_target = CountMacroWritePages(target_blob);
    const int pages_old = CountMacroWritePages(old_blob);
    const int pages_to_send = std::max(pages_target, pages_old);

    for (int page = 0; page < pages_to_send; ++page) {
        std::uint8_t req[kPacketLen]{};
        BuildSetMacroPagePacket(slot, page, page + 1 == pages_to_send, target_blob, req);
        if (!ExchangeFeature(req, nullptr, false, error)) {
            return false;
        }
        if (page + 1 < pages_to_send) Sleep(100);
    }
    Sleep(600);

    const std::size_t compare_len =
        std::min<std::size_t>(kMacroBytes, static_cast<std::size_t>(pages_to_send) * kMacroWritePageData);
    std::uint8_t after_blob[kMacroBytes]{};
    bool matched = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (!ReadMacroSlotRawUnlocked(slot, after_blob, error)) {
            return false;
        }
        if (std::memcmp(after_blob, target_blob, compare_len) == 0) {
            matched = true;
            break;
        }
        if (attempt + 1 < 3) Sleep(150);
    }
    if (!matched) {
        if (error != nullptr) {
            *error = "macro slot " + std::to_string(slot) + " readback mismatch after write";
        }
        return false;
    }

    bool empty = true;
    MacroData decoded = DecodeMacroBlob(after_blob, &empty);
    if (empty_out != nullptr) *empty_out = empty;
    if (stream_bytes_out != nullptr) {
        *stream_bytes_out = ComputeDecodedMacroByteLength(decoded, after_blob);
    }
    if (after_out != nullptr) *after_out = std::move(decoded);
    return true;
}

}  // namespace royuan
