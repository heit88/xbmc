/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/CriticalSection.h"

#include <map>
#include <memory>
#include <set>
#include <string>

class CTexture;

/*!
 \brief Seek preview thumbnails from Jellyfin trickplay folders.

 Jellyfin ("save trickplay images with media") writes, next to <name>.mkv, a
 folder <name>.trickplay/<width> - <cols>x<rows>/ of tile sheets 0.jpg,
 1.jpg, ... holding one thumbnail per 10 s in reading order. Tiles are
 cropped out on a background job and kept until the file changes, so the GUI
 thread never waits on the network.
 */
class CTrickplayThumbs
{
public:
  static constexpr int INTERVAL_SECONDS = 10; // Jellyfin's default trickplay interval

  static CTrickplayThumbs& GetInstance();

  /*! \brief Path of the thumbnail for a time in the given file, or empty if
   the file has no trickplay folder. videoAspect is the video's width / height.
   With keepPrevious, the last chosen thumbnail stands in until this one is ready.
   isChosen marks the one being looked at, which is prepared first.
   */
  std::string GetThumb(const std::string& mediaPath,
                       double seconds,
                       float videoAspect,
                       bool keepPrevious,
                       bool isChosen);

private:
  CTrickplayThumbs() = default;

  void StartLocating(const std::string& mediaPath);
  void Locate(unsigned int generation, const std::string& mediaPath, const std::string& oldOutDir);
  void ExtractWanted(unsigned int generation);
  int NextWanted(int loadedSheet);

  CCriticalSection m_lock;

  std::string m_media;
  std::string m_tileDir; // empty: m_media has no trickplay folder
  std::string m_outDir;
  int m_width = 0;
  int m_cols = 0;
  int m_rows = 0;
  unsigned int m_generation = 0;

  int m_tileHeight = 0;
  int m_chosen = 0;
  std::string m_shownPath;
  std::map<int, std::string> m_ready; // an empty path means the tile couldn't be made
  std::set<int> m_wanted;
  bool m_extracting = false;

  int m_sheetNumber = -1;
  std::unique_ptr<CTexture> m_sheet;
};
