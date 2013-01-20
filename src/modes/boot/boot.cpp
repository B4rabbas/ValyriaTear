///////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2010 by The Allacrost Project
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software
// and you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
///////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    boot.cpp
*** \author  Viljami Korhonen, mindflayer@allacrost.org
*** \brief   Source file for boot mode interface.
*** ***************************************************************************/

#include "modes/boot/boot.h"

#include "engine/audio/audio.h"
#include "engine/script/script_modify.h"
#include "engine/input.h"
#include "engine/system.h"

#include "common/global/global.h"

#include "modes/map/map.h"
#include "modes/save/save_mode.h"

#include "modes/mode_help_window.h"

#ifdef DEBUG_MENU
// Files below are used for boot mode to do a test launch of other modes
#include "modes/battle/battle.h"
#include "modes/menu/menu.h"
#include "modes/shop/shop.h"
#endif

#include <iostream>
#include <sstream>

using namespace hoa_utils;

using namespace hoa_audio;
using namespace hoa_video;
using namespace hoa_gui;
using namespace hoa_input;
using namespace hoa_mode_manager;
using namespace hoa_script;
using namespace hoa_system;

using namespace hoa_global;

using namespace hoa_boot::private_boot;

namespace hoa_boot
{

bool BOOT_DEBUG = false;

BootMode *BootMode::_current_instance = NULL;

const std::string _LANGUAGE_FILE = "dat/config/languages.lua";

// ****************************************************************************
// ***** BootMode public methods
// ****************************************************************************

BootMode::BootMode() :
    _boot_state(BOOT_STATE_INTRO),
    _exiting_to_new_game(false),
    _has_modified_settings(false),
    _key_setting_function(NULL),
    _joy_setting_function(NULL),
    _joy_axis_setting_function(NULL),
    _message_window(ustring(), 210.0f, 733.0f)
{
    // Remove potential previous ambient overlays
    VideoManager->DisableFadeEffect();

    IF_PRINT_DEBUG(BOOT_DEBUG) << "BootMode constructor invoked" << std::endl;
    mode_type = MODE_MANAGER_BOOT_MODE;

    _version_text.SetStyle(TextStyle("text20"));
    std::string date_string = " - ";
    date_string.append(__DATE__);
    _version_text.SetText(UTranslate("Half-Episode I Release Candidate 2") + MakeUnicodeString(date_string));

    // Test the existence and validity of the boot script.
    ReadScriptDescriptor boot_script;
    if(!boot_script.OpenFile("dat/config/boot.lua")) {
        PRINT_ERROR << "Failed to load boot data file" << std::endl;
        SystemManager->ExitGame();
        return;
    }

    // Open the boot table spacename
    if(boot_script.OpenTablespace().empty()) {
        PRINT_ERROR << "The boot script file has not set a correct tablespace" << std::endl;
        SystemManager->ExitGame();
        return;
    }
    boot_script.CloseTable(); // The namespace
    boot_script.CloseFile();

    // Trigger the Initialize functions in the scene script component
    GetScriptSupervisor().AddScript("dat/config/boot.lua");
    GetScriptSupervisor().Initialize(this);

    _options_window.Create(300.0f, 550.0f);
    _options_window.SetPosition(360.0f, 188.0f);
    _options_window.SetDisplayMode(VIDEO_MENU_INSTANT);
    _options_window.Hide();

    // Setup all boot menu options and properties
    _SetupMainMenu();
    _SetupOptionsMenu();
    _SetupVideoOptionsMenu();
    _SetupAudioOptionsMenu();
    _SetupLanguageOptionsMenu();
    _SetupKeySettingsMenu();
    _SetupJoySettingsMenu();
    _SetupResolutionMenu();
    _active_menu = &_main_menu;

    // make sure message window is not visible
    _message_window.Hide();
} // BootMode::BootMode()



BootMode::~BootMode()
{
    _options_window.Destroy();
    _SaveSettingsFile("");

    IF_PRINT_WARNING(BOOT_DEBUG)
            << "BOOT: BootMode destructor invoked." << std::endl;

    _key_setting_function = NULL;
    _joy_setting_function = NULL;
    _joy_axis_setting_function = NULL;
}

void BootMode::Reset()
{
    // Set the coordinate system that BootMode uses
    VideoManager->SetStandardCoordSys();
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_CENTER, 0);

    GlobalManager->ClearAllData(); // Resets the game universe to a NULL state
    _current_instance = this;

    GetScriptSupervisor().Reset();
}



void BootMode::Update()
{
    _options_window.Update(SystemManager->GetUpdateTime());

    // Update the game mode generic members.
    GameMode::Update();

    if(_exiting_to_new_game) {
        // When the dae out is done, we start a new game.
        if(!VideoManager->IsFading())
            GlobalManager->NewGame();
        return;
    }

    // The intro is being played
    if(_boot_state == BOOT_STATE_INTRO) {
        if(InputManager->AnyKeyPress()) {
            ChangeState(BOOT_STATE_MENU);
            return;
        } else {
            return; // Otherwise skip rest of the event handling for now
        }
    }

    HelpWindow *help_window = ModeManager->GetHelpWindow();
    if(help_window && help_window->IsActive()) {
        // Any key, except F1
        if(!InputManager->HelpPress() && InputManager->AnyKeyPress()) {
            AudioManager->PlaySound("snd/confirm.wav");
            help_window->Hide();
        }

        // save the settings (automatically changes the first_start variable to 0)
        _has_modified_settings = true;
        _SaveSettingsFile("");

        return;
    }

    // Check for waiting keypresses or joystick button presses
    SDL_Event ev = InputManager->GetMostRecentEvent();
    if(_joy_setting_function != NULL) {
        if(InputManager->AnyKeyPress() && ev.type == SDL_JOYBUTTONDOWN) {
            (this->*_joy_setting_function)(InputManager->GetMostRecentEvent().jbutton.button);
            _joy_setting_function = NULL;
            _has_modified_settings = true;
            _RefreshJoySettings();
            _message_window.Hide();
        }
        if(InputManager->CancelPress()) {
            _joy_setting_function = NULL;
            _message_window.Hide();
        }
        return;
    }

    if(_joy_axis_setting_function != NULL) {
        int8 x = InputManager->GetLastAxisMoved();
        if(x != -1) {
            (this->*_joy_axis_setting_function)(x);
            _joy_axis_setting_function = NULL;
            _has_modified_settings = true;
            _RefreshJoySettings();
            _message_window.Hide();
        }
        if(InputManager->CancelPress()) {
            _joy_axis_setting_function = NULL;
            _message_window.Hide();
        }
        return;
    }

    if(_key_setting_function != NULL) {
        if(InputManager->AnyKeyPress() && ev.type == SDL_KEYDOWN) {
            (this->*_key_setting_function)(InputManager->GetMostRecentEvent().key.keysym.sym);
            _key_setting_function = NULL;
            _has_modified_settings = true;
            _RefreshKeySettings();
            _message_window.Hide();
        }
        if(InputManager->CancelPress()) {
            _key_setting_function = NULL;
            _message_window.Hide();
        }
        return;
    }

    _active_menu->Update();

    // Only quit when we are at the main menu level
    if(_active_menu == &_main_menu && InputManager->QuitPress()) {
        SystemManager->ExitGame();
        return;
    }

    if(InputManager->ConfirmPress()) {
        // Play 'confirm sound' if the selection isn't grayed out and it has a confirm handler
        if(_active_menu->IsOptionEnabled(_active_menu->GetSelection())) {
            // Don't play the sound on New Games as they have their own sound
            if(_active_menu != &_main_menu && _active_menu->GetSelection() != -1)
                AudioManager->PlaySound("snd/confirm.wav");
        } else {
            // Otherwise play a different sound
            AudioManager->PlaySound("snd/bump.wav");
        }

        _active_menu->InputConfirm();

    } else if(InputManager->LeftPress()) {
        _active_menu->InputLeft();
    } else if(InputManager->RightPress()) {
        _active_menu->InputRight();
    } else if(InputManager->UpPress()) {
        _active_menu->InputUp();
    } else if(InputManager->DownPress()) {
        _active_menu->InputDown();
    } else if(InputManager->CancelPress() || InputManager->QuitPress()) {
        if(_active_menu == &_main_menu) {

        } else if(_active_menu == &_options_menu) {
            _options_window.Hide();
            _active_menu = &_main_menu;
        } else if(_active_menu == &_video_options_menu) {
            _active_menu = &_options_menu;
        } else if(_active_menu == &_audio_options_menu) {
            _active_menu = &_options_menu;
        } else if(_active_menu == &_language_options_menu) {
            _active_menu = &_options_menu;
        } else if(_active_menu == &_key_settings_menu) {
            _active_menu = &_options_menu;
        } else if(_active_menu == &_joy_settings_menu) {
            _active_menu = &_options_menu;
        } else if(_active_menu == &_resolution_menu) {
            _active_menu = &_video_options_menu;
        }

        // Play cancel sound
        AudioManager->PlaySound("snd/cancel.wav");
    }
} // void BootMode::Update()



void BootMode::Draw()
{
    VideoManager->PushState();
    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, VIDEO_BLEND, 0);
    VideoManager->SetStandardCoordSys();

    GetScriptSupervisor().DrawBackground();
    GetScriptSupervisor().DrawForeground();
    VideoManager->PopState();
}

void BootMode::DrawPostEffects()
{
    VideoManager->PushState();
    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_TOP, VIDEO_BLEND, 0);
    VideoManager->SetStandardCoordSys();

    GetScriptSupervisor().DrawPostEffects();

    if(_boot_state == BOOT_STATE_MENU) {
        _options_window.Draw();

        // Test whether the welcome window should be shown once
        static bool help_window_shown = false;
        if(!help_window_shown) {
            _ShowHelpWindow();
            help_window_shown = true;
        }

        if(_active_menu)
            _active_menu->Draw();

        VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, 0);
        VideoManager->Move(10.0f, 758.0f);
        _version_text.Draw();
        VideoManager->SetDrawFlags(VIDEO_X_RIGHT, VIDEO_Y_BOTTOM, 0);

        VideoManager->Move(0.0f, 0.0f);
        _message_window.Draw();
    }
    VideoManager->PopState();
}

// ****************************************************************************
// ***** BootMode menu setup and refresh methods
// ****************************************************************************
bool BootMode::_SavesAvailable(int32 maxId)
{
    assert(maxId > 0);
    int32 savesAvailable = 0;
    std::string data_path = GetUserDataPath(true);
    for(int id = 0; id < maxId; ++id) {
        std::ostringstream f;
        f << data_path + "saved_game_" << id << ".lua";
        const std::string filename = f.str();

        if(DoesFileExist(filename)) {
            ++savesAvailable;
        }
    }
    return (savesAvailable > 0);
}


void BootMode::_ReloadTranslatableMenus()
{
    _SetupMainMenu();
    _SetupOptionsMenu();
    _SetupVideoOptionsMenu();
    _SetupAudioOptionsMenu();
    _SetupKeySettingsMenu();
    _SetupJoySettingsMenu();
    _SetupResolutionMenu();
}


void BootMode::_SetupMainMenu()
{
    _main_menu.ClearOptions();
    _main_menu.SetPosition(512.0f, 688.0f);
    _main_menu.SetTextStyle(TextStyle("title24"));
    _main_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _main_menu.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _main_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _main_menu.SetHorizontalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _main_menu.SetCursorOffset(-50.0f, -28.0f);
    _main_menu.SetSkipDisabled(true);

    _main_menu.AddOption(UTranslate("New Game"), &BootMode::_OnNewGame);
    _main_menu.AddOption(UTranslate("Load Game"), &BootMode::_OnLoadGame);
    _main_menu.AddOption(UTranslate("Options"), &BootMode::_OnOptions);

    // Insert the debug options
#ifdef DEBUG_MENU
    _main_menu.SetDimensions(1000.0f, 50.0f, 7, 1, 7, 1);
    _main_menu.AddOption(UTranslate("Battle"), &BootMode::_DEBUG_OnBattle);
    _main_menu.AddOption(UTranslate("Menu"), &BootMode::_DEBUG_OnMenu);
    _main_menu.AddOption(UTranslate("Shop"), &BootMode::_DEBUG_OnShop);
#else
    _main_menu.SetDimensions(800.0f, 50.0f, 4, 1, 4, 1);
#endif
    _main_menu.AddOption(UTranslate("Quit"), &BootMode::_OnQuit);


    if(!_SavesAvailable()) {
        _main_menu.EnableOption(1, false);
        _main_menu.SetSelection(0);
    } else {
        _main_menu.SetSelection(1);
    }

    // Preload main sounds
    AudioManager->LoadSound("snd/confirm.wav");
    AudioManager->LoadSound("snd/cancel.wav");
    AudioManager->LoadSound("snd/bump.wav");
}


void BootMode::_SetupOptionsMenu()
{
    _options_menu.ClearOptions();
    _options_menu.SetPosition(512.0f, 468.0f);
    _options_menu.SetDimensions(300.0f, 600.0f, 1, 5, 1, 5);
    _options_menu.SetTextStyle(TextStyle("title22"));
    _options_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _options_menu.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _options_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _options_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _options_menu.SetCursorOffset(-50.0f, -28.0f);
    _options_menu.SetSkipDisabled(true);

    _options_menu.AddOption(UTranslate("Video"), &BootMode::_OnVideoOptions);
    _options_menu.AddOption(UTranslate("Audio"), &BootMode::_OnAudioOptions);
    _options_menu.AddOption(UTranslate("Language"), &BootMode::_OnLanguageOptions);
    _options_menu.AddOption(UTranslate("Key Settings"), &BootMode::_OnKeySettings);
    _options_menu.AddOption(UTranslate("Joystick Settings"), &BootMode::_OnJoySettings);

    _options_menu.SetSelection(0);
}


void BootMode::_SetupVideoOptionsMenu()
{
    _video_options_menu.ClearOptions();
    _video_options_menu.SetPosition(512.0f, 468.0f);
    _video_options_menu.SetDimensions(300.0f, 400.0f, 1, 4, 1, 4);
    _video_options_menu.SetTextStyle(TextStyle("title22"));
    _video_options_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _video_options_menu.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _video_options_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _video_options_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _video_options_menu.SetCursorOffset(-50.0f, -28.0f);
    _video_options_menu.SetSkipDisabled(true);

    _video_options_menu.AddOption(UTranslate("Resolution:"), &BootMode::_OnResolution);
    // Left & right will change window mode as well as confirm
    _video_options_menu.AddOption(UTranslate("Window mode:"), &BootMode::_OnToggleFullscreen, NULL, NULL, &BootMode::_OnToggleFullscreen, &BootMode::_OnToggleFullscreen);
    _video_options_menu.AddOption(UTranslate("Brightness:"), NULL, NULL, NULL, &BootMode::_OnBrightnessLeft, &BootMode::_OnBrightnessRight);
    _video_options_menu.AddOption(UTranslate("Map tiles: "), &BootMode::_OnTogglePixelArtSmoothed, NULL, NULL, &BootMode::_OnTogglePixelArtSmoothed, &BootMode::_OnTogglePixelArtSmoothed);

    _video_options_menu.SetSelection(0);
}


void BootMode::_SetupAudioOptionsMenu()
{
    _audio_options_menu.ClearOptions();
    _audio_options_menu.SetPosition(512.0f, 468.0f);
    _audio_options_menu.SetDimensions(300.0f, 200.0f, 1, 2, 1, 2);
    _audio_options_menu.SetTextStyle(TextStyle("title22"));
    _audio_options_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _audio_options_menu.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _audio_options_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _audio_options_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _audio_options_menu.SetCursorOffset(-50.0f, -28.0f);
    _audio_options_menu.SetSkipDisabled(true);

    _audio_options_menu.AddOption(UTranslate("Sound Volume: "), NULL, NULL, NULL, &BootMode::_OnSoundLeft, &BootMode::_OnSoundRight);
    _audio_options_menu.AddOption(UTranslate("Music Volume: "), NULL, NULL, NULL, &BootMode::_OnMusicLeft, &BootMode::_OnMusicRight);

    _audio_options_menu.SetSelection(0);

    // Preload test sound
    AudioManager->LoadSound("snd/volume_test.wav", this);
}


void BootMode::_SetupLanguageOptionsMenu()
{
    _language_options_menu.SetPosition(512.0f, 468.0f);
    _language_options_menu.SetTextStyle(TextStyle("title22"));
    _language_options_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _language_options_menu.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _language_options_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _language_options_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _language_options_menu.SetCursorOffset(-50.0f, -28.0f);
    _language_options_menu.SetSkipDisabled(true);


    // Get the list of languages from the Lua file.
    ReadScriptDescriptor read_data;
    if(!read_data.OpenFile(_LANGUAGE_FILE)) {
        PRINT_ERROR << "Failed to load language file: " << _LANGUAGE_FILE << std::endl;
        PRINT_ERROR << "The language list will be empty." << std::endl;
        return;
    }

    read_data.OpenTable("languages");
    uint32 table_size = read_data.GetTableSize();

    // Set up the dimensions of the window according to how many languages are available.
    _language_options_menu.SetDimensions(300.0f, 200.0f, 1, table_size, 1, table_size);

    _po_files.clear();
    for(uint32 i = 1; i <= table_size; i++) {
        read_data.OpenTable(i);
        _po_files.push_back(read_data.ReadString(2));
        _language_options_menu.AddOption(MakeUnicodeString(read_data.ReadString(1)),
                                         &BootMode::_OnLanguageSelect);
        read_data.CloseTable();
    }

    read_data.CloseTable();
    if(read_data.IsErrorDetected())
        PRINT_ERROR << "Error occurred while loading language list: " << read_data.GetErrorMessages() << std::endl;
    read_data.CloseFile();
}


void BootMode::_SetupKeySettingsMenu()
{
    _key_settings_menu.ClearOptions();
    _key_settings_menu.SetPosition(512.0f, 468.0f);
    _key_settings_menu.SetDimensions(250.0f, 500.0f, 1, 8, 1, 8);
    _key_settings_menu.SetTextStyle(TextStyle("title22"));
    _key_settings_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _key_settings_menu.SetOptionAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _key_settings_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _key_settings_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _key_settings_menu.SetCursorOffset(-50.0f, -28.0f);
    _key_settings_menu.SetSkipDisabled(true);

    _key_settings_menu.AddOption(UTranslate("Up: "), &BootMode::_RedefineUpKey);
    _key_settings_menu.AddOption(UTranslate("Down: "), &BootMode::_RedefineDownKey);
    _key_settings_menu.AddOption(UTranslate("Left: "), &BootMode::_RedefineLeftKey);
    _key_settings_menu.AddOption(UTranslate("Right: "), &BootMode::_RedefineRightKey);
    _key_settings_menu.AddOption(UTranslate("Confirm: "), &BootMode::_RedefineConfirmKey);
    _key_settings_menu.AddOption(UTranslate("Cancel: "), &BootMode::_RedefineCancelKey);
    _key_settings_menu.AddOption(UTranslate("Menu: "), &BootMode::_RedefineMenuKey);
    _key_settings_menu.AddOption(UTranslate("Pause: "), &BootMode::_RedefinePauseKey);
    _key_settings_menu.AddOption(UTranslate("Restore defaults"), &BootMode::_OnRestoreDefaultKeys);
}


void BootMode::_SetupJoySettingsMenu()
{
    _joy_settings_menu.ClearOptions();
    _joy_settings_menu.SetPosition(512.0f, 468.0f);
    _joy_settings_menu.SetDimensions(250.0f, 500.0f, 1, 6, 1, 6);
    _joy_settings_menu.SetTextStyle(TextStyle("title22"));
    _joy_settings_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _joy_settings_menu.SetOptionAlignment(VIDEO_X_LEFT, VIDEO_Y_CENTER);
    _joy_settings_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _joy_settings_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _joy_settings_menu.SetCursorOffset(-50.0f, -28.0f);
    _joy_settings_menu.SetSkipDisabled(true);

    ustring dummy;
    // TODO: Add missing joystick options in the GUI
//	_joy_settings_menu.AddOption(dummy, &BootMode::_ToggleJoystickEnabled);
    _joy_settings_menu.AddOption(dummy, &BootMode::_RedefineXAxisJoy);
    _joy_settings_menu.AddOption(dummy, &BootMode::_RedefineYAxisJoy);
//	_joy_settings_menu.AddOption(dummy, &BootMode::_RedefineThresholdJoy);

    _joy_settings_menu.AddOption(dummy, &BootMode::_RedefineConfirmJoy);
    _joy_settings_menu.AddOption(dummy, &BootMode::_RedefineCancelJoy);
    _joy_settings_menu.AddOption(dummy, &BootMode::_RedefineMenuJoy);
    _joy_settings_menu.AddOption(dummy, &BootMode::_RedefinePauseJoy);
//	_joy_settings_menu.AddOption(dummy, &BootMode::_RedefineQuitJoy);

    _joy_settings_menu.AddOption(UTranslate("Restore defaults"), &BootMode::_OnRestoreDefaultJoyButtons);
}


void BootMode::_SetupResolutionMenu()
{
    _resolution_menu.SetPosition(512.0f, 468.0f);
    _resolution_menu.SetDimensions(300.0f, 200.0f, 1, 4, 1, 4);
    _resolution_menu.SetTextStyle(TextStyle("title22"));
    _resolution_menu.SetAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _resolution_menu.SetOptionAlignment(VIDEO_X_CENTER, VIDEO_Y_CENTER);
    _resolution_menu.SetSelectMode(VIDEO_SELECT_SINGLE);
    _resolution_menu.SetVerticalWrapMode(VIDEO_WRAP_MODE_STRAIGHT);
    _resolution_menu.SetCursorOffset(-50.0f, -28.0f);
    _resolution_menu.SetSkipDisabled(true);

    _resolution_menu.AddOption(MakeUnicodeString("640 x 480"), &BootMode::_OnResolution640x480);
    _resolution_menu.AddOption(MakeUnicodeString("800 x 600"), &BootMode::_OnResolution800x600);
    _resolution_menu.AddOption(MakeUnicodeString("1024 x 768"), &BootMode::_OnResolution1024x768);
    _resolution_menu.AddOption(MakeUnicodeString("1280 x 1024"), &BootMode::_OnResolution1280x1024);

    if(VideoManager->GetScreenWidth() == 640)
        _resolution_menu.SetSelection(0);
    else if(VideoManager->GetScreenWidth() == 800)
        _resolution_menu.SetSelection(1);
    else if(VideoManager->GetScreenWidth() == 1024)
        _resolution_menu.SetSelection(2);
    else if(VideoManager->GetScreenWidth() == 1280)
        _resolution_menu.SetSelection(3);
}

void BootMode::_RefreshVideoOptions()
{
    // Update resolution text
    std::ostringstream resolution("");
    resolution << VideoManager->GetScreenWidth() << " x " << VideoManager->GetScreenHeight();
    _video_options_menu.SetOptionText(0, UTranslate("Resolution: ") + MakeUnicodeString(resolution.str()));

    // Update text on current video mode
    if(VideoManager->IsFullscreen())
        _video_options_menu.SetOptionText(1, UTranslate("Window mode: ") + UTranslate("Fullscreen"));
    else
        _video_options_menu.SetOptionText(1, UTranslate("Window mode: ") + UTranslate("Windowed"));

    // Update brightness
    _video_options_menu.SetOptionText(2, UTranslate("Brightness: ") + MakeUnicodeString(NumberToString(VideoManager->GetGamma() * 50.0f + 0.5f) + " %"));

    // Update the image quality text
    if(VideoManager->ShouldSmoothPixelArt())
        _video_options_menu.SetOptionText(3, UTranslate("Map tiles: ") + UTranslate("Smoothed"));
    else
        _video_options_menu.SetOptionText(3, UTranslate("Map tiles: ") + UTranslate("Normal"));
}



void BootMode::_RefreshAudioOptions()
{
    _audio_options_menu.SetOptionText(0, UTranslate("Sound Volume: ") + MakeUnicodeString(NumberToString(static_cast<int32>(AudioManager->GetSoundVolume() * 100.0f + 0.5f)) + " %"));
    _audio_options_menu.SetOptionText(1, UTranslate("Music Volume: ") + MakeUnicodeString(NumberToString(static_cast<int32>(AudioManager->GetMusicVolume() * 100.0f + 0.5f)) + " %"));
}



void BootMode::_RefreshKeySettings()
{
    // Update key names
    _key_settings_menu.SetOptionText(0, UTranslate("Move Up") + MakeUnicodeString("<r>" + InputManager->GetUpKeyName()));
    _key_settings_menu.SetOptionText(1, UTranslate("Move Down") + MakeUnicodeString("<r>" + InputManager->GetDownKeyName()));
    _key_settings_menu.SetOptionText(2, UTranslate("Move Left") + MakeUnicodeString("<r>" + InputManager->GetLeftKeyName()));
    _key_settings_menu.SetOptionText(3, UTranslate("Move Right") + MakeUnicodeString("<r>" + InputManager->GetRightKeyName()));
    _key_settings_menu.SetOptionText(4, UTranslate("Confirm") + MakeUnicodeString("<r>" + InputManager->GetConfirmKeyName()));
    _key_settings_menu.SetOptionText(5, UTranslate("Cancel") + MakeUnicodeString("<r>" + InputManager->GetCancelKeyName()));
    _key_settings_menu.SetOptionText(6, UTranslate("Menu") + MakeUnicodeString("<r>" + InputManager->GetMenuKeyName()));
    _key_settings_menu.SetOptionText(7, UTranslate("Pause") + MakeUnicodeString("<r>" + InputManager->GetPauseKeyName()));
}



void BootMode::_RefreshJoySettings()
{
    int32 i = 0;
    _joy_settings_menu.SetOptionText(i++, UTranslate("X Axis") + MakeUnicodeString("<r>" + NumberToString(InputManager->GetXAxisJoy())));
    _joy_settings_menu.SetOptionText(i++, UTranslate("Y Axis") + MakeUnicodeString("<r>" + NumberToString(InputManager->GetYAxisJoy())));
    _joy_settings_menu.SetOptionText(i++, UTranslate("Confirm: Button") + MakeUnicodeString("<r>" + NumberToString(InputManager->GetConfirmJoy())));
    _joy_settings_menu.SetOptionText(i++, UTranslate("Cancel: Button") + MakeUnicodeString("<r>" + NumberToString(InputManager->GetCancelJoy())));
    _joy_settings_menu.SetOptionText(i++, UTranslate("Menu: Button") + MakeUnicodeString("<r>" + NumberToString(InputManager->GetMenuJoy())));
    _joy_settings_menu.SetOptionText(i++, UTranslate("Pause: Button") + MakeUnicodeString("<r>" + NumberToString(InputManager->GetPauseJoy())));
}

// ****************************************************************************
// ***** BootMode menu handler methods
// ****************************************************************************

void BootMode::_OnNewGame()
{
    AudioManager->StopAllMusic();
    VideoManager->FadeScreen(Color::black, 2000);

    AudioManager->PlaySound("snd/new_game.wav");
    _exiting_to_new_game = true;
}



void BootMode::_OnLoadGame()
{
    hoa_save::SaveMode *SVM = new hoa_save::SaveMode(false);
    ModeManager->Push(SVM);
}



void BootMode::_OnOptions()
{
    _active_menu = &_options_menu;
    _options_window.Show();
}


void BootMode::_OnQuit()
{
    SystemManager->ExitGame();
}


#ifdef DEBUG_MENU
void BootMode::_DEBUG_OnBattle()
{
    ReadScriptDescriptor read_data;
    read_data.RunScriptFunction("dat/debug/debug_battle.lua",
                                "BootBattleTest", true);
}



void BootMode::_DEBUG_OnMenu()
{
    ReadScriptDescriptor read_data;
    read_data.RunScriptFunction("dat/debug/debug_menu.lua",
                                "BootMenuTest", true);
}



void BootMode::_DEBUG_OnShop()
{
    ReadScriptDescriptor read_data;
    read_data.RunScriptFunction("dat/debug/debug_shop.lua",
                                "BootShopTest", true);
}
#endif // #ifdef DEBUG_MENU


void BootMode::_OnVideoOptions()
{
    _active_menu = &_video_options_menu;
    _RefreshVideoOptions();
}



void BootMode::_OnAudioOptions()
{
    // Switch the current menu
    _active_menu = &_audio_options_menu;
    _RefreshAudioOptions();
}



void BootMode::_OnLanguageOptions()
{
    // Switch the current menu
    _active_menu = &_language_options_menu;
    //_UpdateLanguageOptions();
}



void BootMode::_OnKeySettings()
{
    _active_menu = &_key_settings_menu;
    _RefreshKeySettings();
}



void BootMode::_OnJoySettings()
{
    _active_menu = &_joy_settings_menu;
    _RefreshJoySettings();
}

void BootMode::_OnToggleFullscreen()
{
    // Toggle fullscreen / windowed
    VideoManager->ToggleFullscreen();
    VideoManager->ApplySettings();
    _RefreshVideoOptions();
    _has_modified_settings = true;
}

void BootMode::_OnTogglePixelArtSmoothed()
{
    // Toggle smooth texturing
    VideoManager->SetPixelArtSmoothed(!VideoManager->ShouldSmoothPixelArt());
    VideoManager->ApplySettings();
    _RefreshVideoOptions();
    _has_modified_settings = true;
}


void BootMode::_OnResolution()
{
    _active_menu = &_resolution_menu;
}



void BootMode::_OnResolution640x480()
{
    if(VideoManager->GetScreenWidth() != 640 && VideoManager->GetScreenHeight() != 480)
        _ChangeResolution(640, 480);
}



void BootMode::_OnResolution800x600()
{
    if(VideoManager->GetScreenWidth() != 800 && VideoManager->GetScreenHeight() != 600)
        _ChangeResolution(800, 600);
}



void BootMode::_OnResolution1024x768()
{
    if(VideoManager->GetScreenWidth() != 1024 && VideoManager->GetScreenHeight() != 768)
        _ChangeResolution(1024, 768);
}



void BootMode::_OnResolution1280x1024()
{
    if(VideoManager->GetScreenWidth() != 1280 && VideoManager->GetScreenHeight() != 1024)
        _ChangeResolution(1280, 1024);
}



void BootMode::_OnBrightnessLeft()
{
    VideoManager->SetGamma(VideoManager->GetGamma() - 0.1f);
    _RefreshVideoOptions();
}



void BootMode::_OnBrightnessRight()
{
    VideoManager->SetGamma(VideoManager->GetGamma() + 0.1f);
    _RefreshVideoOptions();
}



void BootMode::_OnSoundLeft()
{
    AudioManager->SetSoundVolume(AudioManager->GetSoundVolume() - 0.1f);
    _RefreshAudioOptions();
    // Play a sound for user to hear new volume level.
    AudioManager->PlaySound("snd/volume_test.wav");
    _has_modified_settings = true;
}



void BootMode::_OnSoundRight()
{
    AudioManager->SetSoundVolume(AudioManager->GetSoundVolume() + 0.1f);
    _RefreshAudioOptions();
    // Play a sound for user to hear new volume level.
    AudioManager->PlaySound("snd/volume_test.wav");
    _has_modified_settings = true;
}



void BootMode::_OnMusicLeft()
{
    AudioManager->SetMusicVolume(AudioManager->GetMusicVolume() - 0.1f);
    _RefreshAudioOptions();
    _has_modified_settings = true;
}



void BootMode::_OnMusicRight()
{
    AudioManager->SetMusicVolume(AudioManager->GetMusicVolume() + 0.1f);
    _RefreshAudioOptions();
    _has_modified_settings = true;
}



void BootMode::_OnLanguageSelect()
{
    SystemManager->SetLanguage(_po_files[_language_options_menu.GetSelection()]);
    _has_modified_settings = true;

    // Reload all the translatable text in the boot menus.
    _ReloadTranslatableMenus();

    // Reloads the global scripts to update their inner translatable strings
    GlobalManager->ReloadGlobalScripts();
}



void BootMode::_OnRestoreDefaultKeys()
{
    InputManager->RestoreDefaultKeys();
    _RefreshKeySettings();
    _has_modified_settings = true;
}



void BootMode::_OnRestoreDefaultJoyButtons()
{
    InputManager->RestoreDefaultJoyButtons();
    _RefreshJoySettings();
    _has_modified_settings = true;
}

// ****************************************************************************
// ***** BootMode helper methods
// ****************************************************************************

void BootMode::_ShowHelpWindow()
{
    // Load the settings file for reading in the welcome variable
    ReadScriptDescriptor settings_lua;
    std::string file = GetSettingsFilename();
    if(!settings_lua.OpenFile(file)) {
        PRINT_WARNING << "failed to load the boot settings file" << std::endl;
    }

    settings_lua.OpenTable("settings");
    if(settings_lua.ReadInt("first_start") == 1) {
        ModeManager->GetHelpWindow()->Show();
    }
    settings_lua.CloseTable();
    settings_lua.CloseFile();
}


void BootMode::_ShowMessageWindow(bool joystick)
{
    if(joystick)
        _ShowMessageWindow(WAIT_JOY_BUTTON);
    else
        _ShowMessageWindow(WAIT_KEY);
}



void BootMode::_ShowMessageWindow(WAIT_FOR wait)
{
    std::string message = "";
    if(wait == WAIT_JOY_BUTTON)
        _message_window.SetText(UTranslate("Please press a new joystick button."));
    else if(wait == WAIT_KEY)
        _message_window.SetText(UTranslate("Please press a new key."));
    else if(wait == WAIT_JOY_AXIS)
        _message_window.SetText(UTranslate("Please move an axis."));
    else {
        PRINT_WARNING << "Undefined wait value." << std::endl;
        return;
    }

    _message_window.Show();
}



void BootMode::_ChangeResolution(int32 width, int32 height)
{
    VideoManager->SetResolution(width, height);
    VideoManager->ApplySettings();
// 	_active_menu = &_video_options_menu; // return back to video options
    _RefreshVideoOptions();
    _has_modified_settings = true;
}



bool BootMode::_LoadSettingsFile(const std::string &filename)
{
    ReadScriptDescriptor settings;

    if(!settings.OpenFile(filename))
        return false;

    IF_PRINT_WARNING(BOOT_DEBUG)
            << "BOOT: Opened file to load settings " << settings.GetFilename() << std::endl;

    settings.OpenTable("settings");

    SystemManager->SetLanguage(static_cast<std::string>(settings.ReadString("language")));

    settings.OpenTable("key_settings");
    InputManager->SetUpKey(static_cast<SDLKey>(settings.ReadInt("up")));
    InputManager->SetDownKey(static_cast<SDLKey>(settings.ReadInt("down")));
    InputManager->SetLeftKey(static_cast<SDLKey>(settings.ReadInt("left")));
    InputManager->SetRightKey(static_cast<SDLKey>(settings.ReadInt("right")));
    InputManager->SetConfirmKey(static_cast<SDLKey>(settings.ReadInt("confirm")));
    InputManager->SetCancelKey(static_cast<SDLKey>(settings.ReadInt("cancel")));
    InputManager->SetMenuKey(static_cast<SDLKey>(settings.ReadInt("menu")));
    InputManager->SetPauseKey(static_cast<SDLKey>(settings.ReadInt("pause")));
    settings.CloseTable();

    if(settings.IsErrorDetected()) {
        PRINT_ERROR << "SETTINGS LOAD ERROR: failure while trying to retrieve key map "
                    << "information from file: " << settings.GetFilename() << std::endl
                    << settings.GetErrorMessages() << std::endl;
        settings.CloseFile();
        return false;
    }

    settings.OpenTable("joystick_settings");
    // This is a hack to disable joystick input to fix a bug with "phantom" joysticks on certain systems.
    // TODO; It should call a method of the input engine to disable the joysticks.
    if(settings.DoesBoolExist("input_disabled") && settings.ReadBool("input_disabled") == true) {
        SDL_JoystickEventState(SDL_IGNORE);
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
    InputManager->SetJoyIndex(static_cast<int32>(settings.ReadInt("index")));
    InputManager->SetConfirmJoy(static_cast<uint8>(settings.ReadInt("confirm")));
    InputManager->SetCancelJoy(static_cast<uint8>(settings.ReadInt("cancel")));
    InputManager->SetMenuJoy(static_cast<uint8>(settings.ReadInt("menu")));
    InputManager->SetPauseJoy(static_cast<uint8>(settings.ReadInt("pause")));

    // WinterKnight: These are hidden settings. You can change them by editing settings.lua,
    // but they are not available in the options menu at this time.
    InputManager->SetQuitJoy(static_cast<uint8>(settings.ReadInt("quit")));
    if(settings.DoesIntExist("x_axis"))
        InputManager->SetXAxisJoy(static_cast<int8>(settings.ReadInt("x_axis")));
    if(settings.DoesIntExist("y_axis"))
        InputManager->SetYAxisJoy(static_cast<int8>(settings.ReadInt("y_axis")));
    if(settings.DoesIntExist("threshold"))
        InputManager->SetThresholdJoy(static_cast<uint16>(settings.ReadInt("threshold")));
    settings.CloseTable();

    if(settings.IsErrorDetected()) {
        PRINT_ERROR << "SETTINGS LOAD ERROR: an error occured while trying to retrieve joystick mapping information "
                    << "from file: " << settings.GetFilename() << std::endl
                    << settings.GetErrorMessages() << std::endl;
        settings.CloseFile();
        return false;
    }

    // Load video settings
    settings.OpenTable("video_settings");
    bool fullscreen = settings.ReadBool("full_screen");
    VideoManager->SetPixelArtSmoothed(settings.ReadBool("smooth_graphics"));
    int32 resx = static_cast<int32>(settings.ReadInt("screen_resx"));


    //Set Resolution  according to width if no width matches our predefined resoultion set to lowest resolution
    if(resx == 800) {
        _OnResolution800x600();
        _resolution_menu.SetSelection(1);
    } else if(resx == 1024) {
        _OnResolution1024x768();
        _resolution_menu.SetSelection(2);
    } else if(resx == 1280) {
        _OnResolution1280x1024();
        _resolution_menu.SetSelection(3);
    } else {
        _OnResolution640x480();
        _resolution_menu.SetSelection(0);
    }

    //set the fullscreen and update video options
    if(VideoManager->IsFullscreen() && fullscreen == false)
        _OnToggleFullscreen();
    else if(VideoManager->IsFullscreen() == false && fullscreen)
        _OnToggleFullscreen();

    settings.CloseTable();

    if(settings.IsErrorDetected()) {
        PRINT_ERROR << "SETTINGS LOAD ERROR: failure while trying to retrieve video settings "
                    << "information from file: " << settings.GetFilename() << std::endl
                    << settings.GetErrorMessages() << std::endl;
        settings.CloseFile();
        return false;
    }

    // Load Audio settings
    if(AUDIO_ENABLE) {
        settings.OpenTable("audio_settings");
        AudioManager->SetMusicVolume(static_cast<float>(settings.ReadFloat("music_vol")));
        AudioManager->SetSoundVolume(static_cast<float>(settings.ReadFloat("sound_vol")));
    }
    settings.CloseAllTables();

    if(settings.IsErrorDetected()) {
        PRINT_ERROR << "SETTINGS LOAD ERROR: failure while trying to retrieve audio settings "
                    << "information from file: " << settings.GetFilename() << std::endl
                    << settings.GetErrorMessages() << std::endl;
        settings.CloseFile();
        return false;
    }

    settings.CloseFile();

    IF_PRINT_WARNING(BOOT_DEBUG) << "Settings closed " << settings.GetFilename() << std::endl;

    return true;
} // bool BootMode::_LoadSettingsFile(const std::string& filename)



bool BootMode::_SaveSettingsFile(const std::string &filename)
{

    // No need to save the settings if we haven't edited anything!
    if(!_has_modified_settings)
        return false;

    std::string file = "";
    std::string fileTemp = "";

    // Load the settings file for reading in the original data
    fileTemp = GetUserDataPath(false) + "/settings.lua";


    if(filename == "")
        file = fileTemp;
    else
        file = GetUserDataPath(false) + "/" + filename;

    //copy the default file so we have an already set up lua file and then we can modify its settings
    if(!DoesFileExist(file))
        CopyFile(std::string("dat/config/settings.lua"), file);

    ModifyScriptDescriptor settings_lua;
    if(!settings_lua.OpenFile(file)) {
        PRINT_ERROR << "BOOT ERROR: failed to load the settings file!" << std::endl;
        return false;
    }

    // Write the current settings into the .lua file
    settings_lua.ModifyInt("settings.first_start", 0);

    //Save language settings
    settings_lua.ModifyString("settings.language", SystemManager->GetLanguage());

    // video
    settings_lua.OpenTable("settings");
    settings_lua.ModifyInt("video_settings.screen_resx", VideoManager->GetScreenWidth());
    settings_lua.ModifyInt("video_settings.screen_resy", VideoManager->GetScreenHeight());
    settings_lua.ModifyBool("video_settings.full_screen", VideoManager->IsFullscreen());
    settings_lua.ModifyBool("video_settings.smooth_graphics", VideoManager->ShouldSmoothPixelArt());
    //settings_lua.ModifyFloat("video_settings.brightness", VideoManager->GetGamma());

    // audio
    settings_lua.ModifyFloat("audio_settings.music_vol", AudioManager->GetMusicVolume());
    settings_lua.ModifyFloat("audio_settings.sound_vol", AudioManager->GetSoundVolume());

    // input
    settings_lua.ModifyInt("key_settings.up", InputManager->GetUpKey());
    settings_lua.ModifyInt("key_settings.down", InputManager->GetDownKey());
    settings_lua.ModifyInt("key_settings.left", InputManager->GetLeftKey());
    settings_lua.ModifyInt("key_settings.right", InputManager->GetRightKey());
    settings_lua.ModifyInt("key_settings.confirm", InputManager->GetConfirmKey());
    settings_lua.ModifyInt("key_settings.cancel", InputManager->GetCancelKey());
    settings_lua.ModifyInt("key_settings.menu", InputManager->GetMenuKey());
    settings_lua.ModifyInt("key_settings.pause", InputManager->GetPauseKey());
    settings_lua.ModifyInt("joystick_settings.x_axis", InputManager->GetXAxisJoy());
    settings_lua.ModifyInt("joystick_settings.y_axis", InputManager->GetYAxisJoy());
    settings_lua.ModifyInt("joystick_settings.confirm", InputManager->GetConfirmJoy());
    settings_lua.ModifyInt("joystick_settings.cancel", InputManager->GetCancelJoy());
    settings_lua.ModifyInt("joystick_settings.menu", InputManager->GetMenuJoy());
    settings_lua.ModifyInt("joystick_settings.pause", InputManager->GetPauseJoy());

    // and save it!
    settings_lua.CommitChanges();
    settings_lua.CloseFile();

    _has_modified_settings = false;

    return true;
} // bool BootMode::_SaveSettingsFile(const std::string& filename)

// ****************************************************************************
// ***** BootMode input configuration methods
// ****************************************************************************

SDLKey BootMode::_WaitKeyPress()
{
    SDL_Event event;
    while(SDL_WaitEvent(&event)) {
        if(event.type == SDL_KEYDOWN)
            break;
    }

    return event.key.keysym.sym;
}

uint8 BootMode::_WaitJoyPress()
{
    SDL_Event event;
    while(SDL_WaitEvent(&event)) {
        if(event.type == SDL_JOYBUTTONDOWN)
            break;
    }

    return event.jbutton.button;
}

void BootMode::_RedefineUpKey()
{
    _key_setting_function = &BootMode::_SetUpKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefineDownKey()
{
    _key_setting_function = &BootMode::_SetDownKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefineLeftKey()
{
    _key_setting_function = &BootMode::_SetLeftKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefineRightKey()
{
    _key_setting_function = &BootMode::_SetRightKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefineConfirmKey()
{
    _key_setting_function = &BootMode::_SetConfirmKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefineCancelKey()
{
    _key_setting_function = &BootMode::_SetCancelKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefineMenuKey()
{
    _key_setting_function = &BootMode::_SetMenuKey;
    _ShowMessageWindow(false);
}

void BootMode::_RedefinePauseKey()
{
    _key_setting_function = &BootMode::_SetPauseKey;
    _ShowMessageWindow(false);
}

void BootMode::_SetUpKey(const SDLKey &key)
{
    InputManager->SetUpKey(key);
}

void BootMode::_SetDownKey(const SDLKey &key)
{
    InputManager->SetDownKey(key);
}

void BootMode::_SetLeftKey(const SDLKey &key)
{
    InputManager->SetLeftKey(key);
}

void BootMode::_SetRightKey(const SDLKey &key)
{
    InputManager->SetRightKey(key);
}

void BootMode::_SetConfirmKey(const SDLKey &key)
{
    InputManager->SetConfirmKey(key);
}

void BootMode::_SetCancelKey(const SDLKey &key)
{
    InputManager->SetCancelKey(key);
}

void BootMode::_SetMenuKey(const SDLKey &key)
{
    InputManager->SetMenuKey(key);
}

void BootMode::_SetPauseKey(const SDLKey &key)
{
    InputManager->SetPauseKey(key);
}

void BootMode::_RedefineXAxisJoy()
{
    _joy_axis_setting_function = &BootMode::_SetXAxisJoy;
    _ShowMessageWindow(WAIT_JOY_AXIS);
    InputManager->ResetLastAxisMoved();
}

void BootMode::_RedefineYAxisJoy()
{
    _joy_axis_setting_function = &BootMode::_SetYAxisJoy;
    _ShowMessageWindow(WAIT_JOY_AXIS);
    InputManager->ResetLastAxisMoved();
}

void BootMode::_RedefineThresholdJoy()
{
    // TODO
}

void BootMode::_RedefineConfirmJoy()
{
    _joy_setting_function = &BootMode::_SetConfirmJoy;
    _ShowMessageWindow(true);
}

void BootMode::_RedefineCancelJoy()
{
    _joy_setting_function = &BootMode::_SetCancelJoy;
    _ShowMessageWindow(true);
}

void BootMode::_RedefineMenuJoy()
{
    _joy_setting_function = &BootMode::_SetMenuJoy;
    _ShowMessageWindow(true);
}

void BootMode::_RedefinePauseJoy()
{
    _joy_setting_function = &BootMode::_SetPauseJoy;
    _ShowMessageWindow(true);
}

void BootMode::_RedefineQuitJoy()
{
    // TODO
}

void BootMode::_SetXAxisJoy(int8 axis)
{
    InputManager->SetXAxisJoy(axis);
}

void BootMode::_SetYAxisJoy(int8 axis)
{
    InputManager->SetYAxisJoy(axis);
}

void BootMode::_SetConfirmJoy(uint8 button)
{
    InputManager->SetConfirmJoy(button);
}

void BootMode::_SetCancelJoy(uint8 button)
{
    InputManager->SetCancelJoy(button);
}

void BootMode::_SetMenuJoy(uint8 button)
{
    InputManager->SetMenuJoy(button);
}

void BootMode::_SetPauseJoy(uint8 button)
{
    InputManager->SetPauseJoy(button);
}

} // namespace hoa_boot
