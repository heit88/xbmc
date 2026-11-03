/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/CriticalSection.h"

#include <memory>
#include <string>

class CTexture;

/*!
 \brief Seek preview thumbnails from Jellyfin trickplay folders.

 Jellyfin ("save trickplay images with media") writes, next to <name>.mkv, a
 folder <name>.trickplay/<width> - <cols>x<rows>/ of tile sheets 0.jpg,
 1.jpg, ... holding one thumbnail per 10 s in reading order. Tiles are
 cropped out on a background job; until one is ready the previous path is
 returned, so the GUI thread never waits on the network.
 */
class CTrickplayThumbs
{
public:
  static CTrickplayThumbs& GetInstance();

  /*! \brief Path of the thumbnail for a time in the given file, or empty if
   the file has no trickplay folder. videoAspect is the video's width / height.
   */
  std::string GetThumb(const std::string& mediaPath, double seconds, float videoAspect);

private:
  CTrickplayThumbs() = default;

  void Locate(const std::string& mediaPath);
  void Extract(unsigned int generation, int index, int tileHeight);

  CCriticalSection m_lock;

  std::string m_media;
  std::string m_tileDir; // empty: m_media has no trickplay folder
  std::string m_outDir;
  int m_width = 0;
  int m_cols = 0;
  int m_rows = 0;
  unsigned int m_generation = 0;

  int m_shown = -1;
  std::string m_shownPath;
  int m_pending = -1;

  std::string m_sheetPath;
  std::unique_ptr<CTexture> m_sheet;
};
