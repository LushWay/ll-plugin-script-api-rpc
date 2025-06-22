#pragma once

#include "ll/api/mod/NativeMod.h"


namespace script_api_rpc {

class Mod {

public:
    static Mod& getInstance();

    Mod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    [[nodiscard]] bool load() const;

    /// @return True if the mod is enabled successfully.
    [[nodiscard]] bool enable() const;

    /// @return True if the mod is disabled successfully.
    [[nodiscard]] bool disable() const;

    /// @return True if the mod is unloaded successfully.
    [[nodiscard]] bool unload() const;


private:
    ll::mod::NativeMod& mSelf;

    void writeMapToFile() const;
};

} // namespace script_api_rpc
