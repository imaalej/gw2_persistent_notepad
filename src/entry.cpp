///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - All rights reserved.
///
/// This code is licensed under the MIT license.
/// You should have received a copy of the license along with this source file.
/// You may obtain a copy of the license at: https://opensource.org/license/MIT
/// 
/// Name         :  entry.cpp
///----------------------------------------------------------------------------------------------------

#include <windows.h>

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"
#include "icon.h"

#include "json.hpp"
using json = nlohmann::json;

#include <fstream>
#include <chrono>
#include <vector>

/* proto */
void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();


void onKeybind(const char* aId, bool aIsRelease);
// void LoadNotepadData();
// void SaveNotepadData();
void LoadSettings();
void SaveSettings();

/* globals */
AddonDefinition AddonDef = {};
HMODULE hSelf            = nullptr;
AddonAPI* APIDefs        = nullptr;
NexusLinkData* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

/* globals */
// ... your existing globals ...

static std::string myNotepadText = "";
static std::vector<char> textBuffer;
bool isTextDirty = false;
bool isDataLoaded = false;
bool showWindow = false;
std::chrono::steady_clock::time_point lastTypeTime;
const std::string dirPath = "addons/Notepad";
const std::string savePath = dirPath + "/settings.json";
const char* gId = "KB_NOTEPAD_TOGGLE_UI";
const char* gTexId = "TEX_NOTEPAD_ICON";

//Keybinding
char gKeybind[128] = "ALT+SHIFT+E";
bool gRecordingKeybind = false;


///----------------------------------------------------------------------------------------------------
/// DllMain:
/// 	Main entry point for DLL.
/// 	We are not interested in this, all we get is our own HMODULE in case we need it.
///----------------------------------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH: hSelf = hModule; break;
		case DLL_PROCESS_DETACH: break;
		case DLL_THREAD_ATTACH: break;
		case DLL_THREAD_DETACH: break;
	}
	return TRUE;
}

///----------------------------------------------------------------------------------------------------
/// GetAddonDef:
/// 	Export needed to give Nexus information about the addon.
///----------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
	AddonDef.Signature = -173867; // set to random unused negative integer
	AddonDef.APIVersion = NEXUS_API_VERSION;
	AddonDef.Name = "Notepad";
	AddonDef.Version.Major = 0;
	AddonDef.Version.Minor = 1;
	AddonDef.Version.Build = 0;
	AddonDef.Version.Revision = 1;
	AddonDef.Author = "gumibo.1643";
	AddonDef.Description = "Keep persistent notes for your journey.";
	AddonDef.Load = AddonLoad;
	AddonDef.Unload = AddonUnload;
	AddonDef.Flags = EAddonFlags_None;

	/* not necessary if hosted on Raidcore, but shown anyway for the example also useful as a backup resource */
	//AddonDef.Provider = EUpdateProvider_GitHub;
	//AddonDef.UpdateLink = "https://github.com/RaidcoreGG/GW2Nexus-AddonTemplate";

	return &AddonDef;
}

///----------------------------------------------------------------------------------------------------
/// AddonLoad:
/// 	Load function for the addon, will receive a pointer to the API.
/// 	(You probably want to store it.)
///----------------------------------------------------------------------------------------------------
void AddonLoad(AddonAPI* aApi)
{
	FreeConsole();
	APIDefs = aApi; // store the api somewhere easily accessible

	ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext); // cast to ImGuiContext*
	ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))APIDefs->ImguiMalloc, (void(*)(void*, void*))APIDefs->ImguiFree); // on imgui 1.80+

	NexusLink = (NexusLinkData*)APIDefs->DataLink.Get("DL_NEXUS_LINK");
	MumbleLink = (Mumble::Data*)APIDefs->DataLink.Get("DL_MUMBLE_LINK");

	// Add an options window and a regular render callback
	APIDefs->Renderer.Register(ERenderType_Render, AddonRender);
	APIDefs->Renderer.Register(ERenderType_OptionsRender, AddonOptions);

    // Set Keybinding
    APIDefs->InputBinds.RegisterWithString(gId, onKeybind, gKeybind);

    // Set up QuickAccess
    APIDefs->Textures.LoadFromMemory(
        gTexId,
        (void*)icon_png,
        icon_png_len,
        nullptr
    );

    APIDefs->QuickAccess.Add(
        "QA_NOTEPAD",
        gTexId,
        gTexId,
        gId,
        "Toggle Notepad"
    );

    // Load notepad and settings.
    LoadSettings();

	APIDefs->Log(ELogLevel_DEBUG, "Notepad", "<c=#00ff00>Notepad loaded.</c>");
}

///----------------------------------------------------------------------------------------------------
/// AddonUnload:
/// 	Everything you registered in AddonLoad, you should "undo" here.
///----------------------------------------------------------------------------------------------------
void AddonUnload()
{
	/* let's clean up after ourselves */
	APIDefs->Renderer.Deregister(AddonRender);
	APIDefs->Renderer.Deregister(AddonOptions);

    APIDefs->InputBinds.Deregister(gId);

    APIDefs->QuickAccess.Remove("QA_NOTEPAD");

	SaveSettings();

	APIDefs->Log(ELogLevel_DEBUG, "Notepad", "<c=#ff0000>Notepad unloaded.</c>");
}

///----------------------------------------------------------------------------------------------------
/// AddonRender:
/// 	Called every frame. Safe to render any ImGui.
/// 	You can control visibility on loading screens with NexusLink->IsGameplay.
///----------------------------------------------------------------------------------------------------

void AddonRender()
{
    if (!isDataLoaded){return;}
    if (!showWindow){return;}

	// Dynamic resize callback for when ImGui outgrows the current vector size.
	// resizes vector safely and ipdates internal buffer pointer.
    auto textCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::vector<char>* vec = static_cast<std::vector<char>*>(data->UserData);
            vec->resize(data->BufTextLen + 1); // +1 accounts for the null-terminator '\0'
            data->Buf = vec->data();           // Re-point ImGui to the new memory location
        }
        return 0;
    };

	// Dynamic window title for when notes are saved/unsaved
    std::string windowTitle;
    if (isTextDirty) {
        windowTitle = "Notepad*###NotepadWindow";
    } else {
        windowTitle = "Notepad###NotepadWindow";
    }

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(windowTitle.c_str()))
    {
		// move loaded file into vector workspace at startup
        if (textBuffer.empty())
        {
            textBuffer.assign(myNotepadText.begin(), myNotepadText.end());
            textBuffer.push_back('\0');
        }

		// Callback flag along with pointer to our vector metadata (&textBuffer)
        if (ImGui::InputTextMultiline("##NotepadField", 
            textBuffer.data(), 
            textBuffer.size(), 
            ImVec2(-FLT_MIN, -FLT_MIN), 
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize,
            textCallback, 
            &textBuffer
        ))
        {
            // Sync the master string back from our safe char array
            myNotepadText = textBuffer.data();
            isTextDirty = true;
            lastTypeTime = std::chrono::steady_clock::now();
        }
    }
    ImGui::End();

    // Autosave logic
    if (isTextDirty)
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTypeTime).count();

        if (elapsed >= 1) 
        {
            SaveSettings();
        }
    }    
}

///----------------------------------------------------------------------------------------------------
/// AddonOptions:
/// 	Basically an ImGui callback that doesn't need its own Begin/End calls.
///----------------------------------------------------------------------------------------------------
void AddonOptions()
{
	ImGui::Separator();
	ImGui::Text("Notepad");
    if (ImGui::Checkbox("Show Window", &showWindow)){
        SaveSettings();
    }
}

void onKeybind(const char* aId, bool aIsRelease){
    if (strcmp(aId, gId) == 0 && !aIsRelease){
        showWindow = !showWindow;
    }
}

void LoadSettings()
{
    CreateDirectoryA(dirPath.c_str(), NULL);

    std::ifstream file(savePath);
    if (file.is_open())
    {
        // std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // myNotepadText = content;
        // file.close();
        try{
            json j;
            file >> j;
            file.close();

            myNotepadText = j.value("notepad_text", "Type anything here...");
            showWindow = j.value("show_window", false);
        } catch (...) {
            //malformed JSON -- fallback to defaults
            myNotepadText = "Type anything here...";
            showWindow = false;
        }
    }
    else
    {
        myNotepadText = "Type anything here...";
    }

    isDataLoaded = true;
	lastTypeTime = std::chrono::steady_clock::now();
}

void SaveSettings()
{
    json j;
    j["notepad_text"] = myNotepadText;
    j["show_window"] = showWindow;

    std::ofstream file(savePath);
    if (file.is_open())
    {
        file << j.dump(4);
        file.close();
        isTextDirty = false; // Reset our flag
        if (APIDefs) {
            APIDefs->Log(ELogLevel_DEBUG, "Notepad", "Settings and notepad saved to disk.");
        }
    }
}
