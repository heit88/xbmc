/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GUIComponent.h"
#include "cores/AudioEngine/Interfaces/AESound.h"
#include "settings/lib/ISettingCallback.h"
#include "threads/CriticalSection.h"

#include <future>
#include <map>
#include <memory>
#include <string>

// forward definitions
class CAction;
class CSettings;
class TiXmlNode;
class IAESound;

enum WINDOW_SOUND { SOUND_INIT = 0, SOUND_DEINIT };

class CGUIAudioManager : public ISettingCallback
{
  class CWindowSounds
  {
  public:
    std::shared_ptr<IAESound> initSound;
    std::shared_ptr<IAESound> deInitSound;
  };

  struct IAESoundDeleter
  {
    void operator()(IAESound* s);
  };

public:
  CGUIAudioManager();
  ~CGUIAudioManager() override;

  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;
  bool OnSettingUpdate(const std::shared_ptr<CSetting>& setting,
                       const char* oldSettingId,
                       const TiXmlNode* oldSettingNode) override;

  void Initialize();
  void DeInitialize();

  bool Load();
  void UnLoad();


  void PlayActionSound(const CAction& action);
  void PlayWindowSound(int id, WINDOW_SOUND event);
  void PlayPythonSound(const std::string& strFileName, bool useCached = true);

  /*!
   * \brief Loop a file in the background of the audio engine, without using a player.
   *
   * The file is loaded on another thread, so it starts a moment later. Replaces any theme that is
   * already playing. It doesn't play over a stream unless it was started first, and never over
   * passthrough, so stop it before playback opens.
   *
   * \param file The file to play
   * \param volume From 0 to 1, relative to Kodi's own volume
   */
  void PlayTheme(const std::string& file, float volume);
  void SetThemeVolume(float volume);
  void StopTheme();
  bool IsThemePlaying();

  void Enable(bool bEnable);
  void SetVolume(float level);
  void Stop();

private:
  // Construction parameters
  std::shared_ptr<CSettings> m_settings;

  typedef std::map<const std::string, std::weak_ptr<IAESound>> soundCache;
  typedef std::map<int, std::shared_ptr<IAESound>> actionSoundMap;
  typedef std::map<int, CWindowSounds> windowSoundMap;
  typedef std::map<const std::string, std::shared_ptr<IAESound>> pythonSoundsMap;

  soundCache          m_soundCache;
  actionSoundMap      m_actionSoundMap;
  windowSoundMap      m_windowSoundMap;
  pythonSoundsMap     m_pythonSounds;

  std::string          m_strMediaDir;
  bool                m_bEnabled;

  CCriticalSection    m_cs;

  std::shared_ptr<IAESound> m_theme;
  float m_themeVolume{1.0f};
  unsigned int m_themeRequest{0}; //!< a load that finishes with an older number is discarded
  std::future<void> m_themeLoad;
  CCriticalSection m_themeLoadLock;

  std::shared_ptr<IAESound> LoadSound(const std::string& filename);
  std::shared_ptr<IAESound> LoadWindowSound(TiXmlNode* pWindowNode,
                                            const std::string& strIdentifier);
};

