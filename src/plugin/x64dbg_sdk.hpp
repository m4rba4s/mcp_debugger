#pragma once

// x64dbg SDK Headers
// TODO: Include actual x64dbg SDK headers when available

#ifdef _WIN32

// Placeholder definitions for x64dbg plugin SDK
// In real implementation, would include official x64dbg headers

// Plugin information structure
struct PLUG_SETUPSTRUCT {
    int cbsize;
    int hMenu;
    int hMenuDisasm;
    int hMenuDump;
    int hMenuStack;
    int sdkVersion;
};

// Plugin handle type
typedef int duint;

// Command callback type
typedef bool (*CBPLUGINCOMMAND)(int argc, char* argv[]);

// Function declarations that would come from x64dbg SDK
extern "C" {
    // TODO: Include actual x64dbg SDK function declarations
    // These are placeholders
    bool _plugin_registercommand(duint pluginHandle, const char* command, CBPLUGINCOMMAND cbCommand, bool debugonly);
    void _plugin_logprintf(const char* format, ...);
    bool _plugin_menuadd(int hMenu, const char* title);
}

#endif // _WIN32