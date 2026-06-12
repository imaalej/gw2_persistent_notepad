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
#include <direct.h>

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"
#include "icon.h"
#include "notes.h"

#include "json.hpp"
using json = nlohmann::json;

#include <fstream>
#include <chrono>
#include <vector>

void AddonLoad(AddonAPI* aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();

void OnKeybind(const char* aId, bool aIsRelease);
void LoadSettings();
void SaveSettings();

AddonDefinition AddonDef = {};
HMODULE hSelf            = nullptr;
AddonAPI* APIDefs        = nullptr;
NexusLinkData* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

Notepad gNotepad;
static std::vector<char> textBuffer;
static int gActiveNoteId = 0;
static int gPendingNoteId = -1;
static int gNoteToDeleteId = -1; 
bool isTextDirty = false;
bool isDataLoaded = false;
bool showWindow = false;
std::chrono::steady_clock::time_point lastTypeTime;
const std::string dirPath = "addons/Notepad";
const std::string savePath = dirPath + "/settings.json";
const char* gId = "KB_NOTEPAD_TOGGLE_UI";
const char* gTexId = "TEX_NOTEPAD_ICON";
char gKeybind[128] = "ALT+SHIFT+E";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
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

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
    AddonDef.Signature = -173867;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Notepad";
    AddonDef.Version.Major = 1;
    AddonDef.Version.Minor = 0;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "gumibo.1643";
    AddonDef.Description = "Keep persistent notes for your journey.";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = EAddonFlags_None;
    return &AddonDef;
}

void AddonLoad(AddonAPI* aApi)
{
    APIDefs = aApi;

    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))APIDefs->ImguiMalloc,
                                 (void(*)(void*, void*))APIDefs->ImguiFree);

    NexusLink = (NexusLinkData*)APIDefs->DataLink.Get("DL_NEXUS_LINK");
    MumbleLink = (Mumble::Data*)APIDefs->DataLink.Get("DL_MUMBLE_LINK");

    APIDefs->Renderer.Register(ERenderType_Render, AddonRender);
    APIDefs->Renderer.Register(ERenderType_OptionsRender, AddonOptions);
    APIDefs->InputBinds.RegisterWithString(gId, OnKeybind, gKeybind);

    APIDefs->Textures.LoadFromMemory(gTexId, (void*)icon_png, icon_png_len, nullptr);
    APIDefs->QuickAccess.Add("QA_NOTEPAD", gTexId, gTexId, gId, "Toggle Notepad");

    LoadSettings();
    APIDefs->Log(ELogLevel_DEBUG, "Notepad", "<c=#00ff00>Notepad loaded.</c>");
}

void AddonUnload()
{
    APIDefs->Renderer.Deregister(AddonRender);
    APIDefs->Renderer.Deregister(AddonOptions);
    APIDefs->InputBinds.Deregister(gId);
    APIDefs->QuickAccess.Remove("QA_NOTEPAD");
    SaveSettings();
    APIDefs->Log(ELogLevel_DEBUG, "Notepad", "<c=#ff0000>Notepad unloaded.</c>");
}

void AddonRender()
{
    if (isTextDirty)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTypeTime).count();
        if (elapsed >= 1)
            SaveSettings();
    }

    static uint32_t lastTick = 0;
    static int frozenFor = 0;
    if (MumbleLink && MumbleLink->UITick != lastTick)
    {
        lastTick = MumbleLink->UITick;
        frozenFor = 0;
    }
    else
    {
        frozenFor++;
    }

    if (!isDataLoaded || !showWindow || (NexusLink && !NexusLink->IsGameplay) || frozenFor > 2)
        return;

    if (gPendingNoteId != -1)
    {
        gNotepad.setNoteText(gActiveNoteId, textBuffer.data());
        SaveSettings();

        std::string incoming = gNotepad.getNoteText(gPendingNoteId);
        textBuffer.assign(incoming.begin(), incoming.end());
        textBuffer.push_back('\0');

        gActiveNoteId = gPendingNoteId;
        gPendingNoteId = -1;
        isTextDirty = false;
    }

    auto textCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            auto* vec = static_cast<std::vector<char>*>(data->UserData);
            vec->resize(data->BufTextLen + 1);
            data->Buf = vec->data();
        }
        return 0;
    };

    std::string windowTitle = isTextDirty ? "Notepad*###NotepadWindow" : "Notepad###NotepadWindow";
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin(windowTitle.c_str(), &showWindow, flags))
    {
        ImGui::Text(isTextDirty ? "Notepad*" : "Notepad");
        ImGui::Separator();

        bool wantAdd = false;
        bool openPopup = false;
        ImGuiTabBarFlags tbFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll;

        if (ImGui::BeginTabBar("##NotepadTabs", tbFlags))
        {
            for (auto& note : gNotepad.notes)
            {
                std::string label = "#" + std::to_string(note.mId) + "###Tab_" + std::to_string(note.mId);
                bool open = true;
                bool* p_open = (note.mId == gActiveNoteId && gNotepad.getLength() > 1) ? &open : nullptr;

                ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
                if (gNoteToDeleteId == note.mId)
                    tabFlags |= ImGuiTabItemFlags_SetSelected;

                if (ImGui::BeginTabItem(label.c_str(), p_open, tabFlags))
                {
                    if (note.mId != gActiveNoteId && gNoteToDeleteId == -1)
                        gPendingNoteId = note.mId;

                    if (note.mId == gActiveNoteId)
                    {
                        if (ImGui::InputTextMultiline("##NotepadField",
                                textBuffer.data(),
                                textBuffer.size(),
                                ImVec2(-1.0f, -1.0f),
                                ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize,
                                textCallback,
                                &textBuffer))
                        {
                            gNotepad.setNoteText(gActiveNoteId, textBuffer.data());
                            isTextDirty = true;
                            lastTypeTime = std::chrono::steady_clock::now();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (p_open && !open)
                {
                    gNoteToDeleteId = note.mId;
                    openPopup = true;
                }
            }

            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
                wantAdd = true;

            ImGui::EndTabBar();
        }

        if (openPopup)
        {
            ImGui::OpenPopup("ConfirmDeleteNote");
        }

        bool modalDrawn = false;
        if (gNotepad.getLength() > 1 && ImGui::BeginPopupModal("ConfirmDeleteNote", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            modalDrawn = true;
            ImGui::Text("Are you sure you want to delete this note?");
            ImGui::Separator();

            if (ImGui::Button("Delete", ImVec2(120, 0)))
            {
                int noteToDelete = gNoteToDeleteId;
                int newActiveId = -1;
                for (auto& note : gNotepad.notes)
                {
                    if (note.mId != noteToDelete)
                    {
                        newActiveId = note.mId;
                        break;
                    }
                }
                if (newActiveId != -1)
                {
                    gActiveNoteId = newActiveId;
                    std::string newText = gNotepad.getNoteText(newActiveId);
                    textBuffer.assign(newText.begin(), newText.end());
                    textBuffer.push_back('\0');
                    gPendingNoteId = -1;
                    isTextDirty = false;
                }
                gNotepad.removeNoteInstance(noteToDelete);
                SaveSettings();
                gNoteToDeleteId = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                gNoteToDeleteId = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (!modalDrawn && !openPopup)
        {
            gNoteToDeleteId = -1;
        }

        if (wantAdd)
        {
            gNotepad.addNoteInstance(false);
            gPendingNoteId = gNotepad.notes.back().mId;
            SaveSettings();
        }
    }
    ImGui::End();
}

void AddonOptions()
{
    ImGui::Separator();
    ImGui::Text("Notepad");
    if (ImGui::Checkbox("Show Window", &showWindow))
    {
        SaveSettings();
    }
}

void OnKeybind(const char* aId, bool aIsRelease)
{
    if (strcmp(aId, gId) == 0 && !aIsRelease)
    {
        if (showWindow && isTextDirty)
            SaveSettings();
        showWindow = !showWindow;
    }
}

void LoadSettings()
{
    if (CreateDirectoryA(dirPath.c_str(), NULL) == 0)
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS && APIDefs)
        {
            APIDefs->Log(ELogLevel_WARNING, "Notepad",
                ("Failed to create directory " + dirPath + ", error: " + std::to_string(err)).c_str());
        }
    }

    std::ifstream file(savePath);
    if (file.is_open())
    {
        try
        {
            json j;
            file >> j;
            file.close();

            if (j.contains("notepad_data"))
                gNotepad = j.at("notepad_data").get<Notepad>();

            showWindow = j.value("show_window", false);
        }
        catch (...)
        {
            showWindow = false;
        }
    }
    else
    {
        showWindow = false;
    }

    if (gNotepad.notes.empty())
    {
        gNotepad.mIdCounter = 0;
        gNotepad.notes.push_back({gNotepad.mIdCounter++, false, "Type anything here..."});
    }

    gActiveNoteId = gNotepad.notes.front().mId;
    gPendingNoteId = -1;
    std::string text = gNotepad.getNoteText(gActiveNoteId);
    textBuffer.assign(text.begin(), text.end());
    textBuffer.push_back('\0');

    isDataLoaded = true;
    lastTypeTime = std::chrono::steady_clock::now();
}

void SaveSettings()
{
    json j;
    j["notepad_data"] = gNotepad;
    j["show_window"] = showWindow;

    std::ofstream file(savePath);
    if (file.is_open())
    {
        file << j.dump(4);
        file.close();
        isTextDirty = false;
        if (APIDefs)
            APIDefs->Log(ELogLevel_DEBUG, "Notepad", "Settings and notepad saved to disk.");
    }
}
