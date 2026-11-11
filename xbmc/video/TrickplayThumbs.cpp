/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "TrickplayThumbs.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "filesystem/Directory.h"
#include "guilib/Texture.h"
#include "pictures/Picture.h"
#include "jobs/JobManager.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <tuple>

using namespace XFILE;

namespace
{
constexpr int MAX_WANTED_DISTANCE = 30; // tiles from the chosen one
}

CTrickplayThumbs& CTrickplayThumbs::GetInstance()
{
  static CTrickplayThumbs instance;
  return instance;
}

void CTrickplayThumbs::StartLocating(const std::string& mediaPath)
{
  m_media = mediaPath;
  m_tileDir.clear();
  m_width = 0;
  m_shownPath.clear();
  m_ready.clear();
  m_wanted.clear();
  m_chosen = 0;
  m_sheetNumber = -1;
  m_sheet.reset();

  const std::string oldOutDir = m_outDir;
  m_generation++;
  m_outDir = StringUtils::Format("special://temp/trickplay/{}/", m_generation);

  const unsigned int generation = m_generation;
  CServiceBroker::GetJobManager()->Submit([this, generation, mediaPath, oldOutDir]()
                                          { Locate(generation, mediaPath, oldOutDir); });
}

void CTrickplayThumbs::Locate(unsigned int generation,
                              const std::string& mediaPath,
                              const std::string& oldOutDir)
{
  if (!oldOutDir.empty())
    CDirectory::RemoveRecursive(oldOutDir);

  if (URIUtils::IsInternetStream(mediaPath))
    return;

  std::string base = mediaPath;
  URIUtils::RemoveExtension(base);
  const std::string root = URIUtils::AddFileToFolder(base + ".trickplay", "");

  CFileItemList items;
  if (!CDirectory::GetDirectory(root, items, "", DIR_FLAG_NO_FILE_DIRS | DIR_FLAG_BYPASS_CACHE))
    return;

  std::string tileDir;
  int tileWidth = 0;
  int tileCols = 0;
  int tileRows = 0;

  // Folder names are "<width> - <cols>x<rows>"; take the widest.
  for (const auto& item : items)
  {
    if (!item->IsFolder())
      continue;
    int width = 0;
    int cols = 0;
    int rows = 0;
    if (sscanf(item->GetLabel().c_str(), "%d - %dx%d", &width, &cols, &rows) == 3 &&
        width > tileWidth && cols > 0 && rows > 0)
    {
      tileDir = URIUtils::AddFileToFolder(item->GetPath(), "");
      tileWidth = width;
      tileCols = cols;
      tileRows = rows;
    }
  }

  if (tileDir.empty())
    return;

  std::unique_lock lock(m_lock);
  if (generation != m_generation)
    return;

  CDirectory::Create(m_outDir);
  m_tileDir = tileDir;
  m_width = tileWidth;
  m_cols = tileCols;
  m_rows = tileRows;
  CLog::Log(LOGINFO, "CTrickplayThumbs: using {} ({}x{} tiles of width {})",
            CURL::GetRedacted(m_tileDir), m_cols, m_rows, m_width);
}

std::string CTrickplayThumbs::GetThumb(const std::string& mediaPath,
                                       double seconds,
                                       float videoAspect,
                                       bool keepPrevious,
                                       bool isChosen)
{
  std::unique_lock lock(m_lock);

  if (mediaPath != m_media)
    StartLocating(mediaPath);
  if (m_tileDir.empty())
    return {};

  const int index = std::max(0, static_cast<int>(seconds / INTERVAL_SECONDS));
  if (isChosen)
    m_chosen = index;

  const auto ready = m_ready.find(index);
  if (ready != m_ready.end())
  {
    if (isChosen)
      m_shownPath = ready->second;
    return ready->second;
  }

  // Jellyfin's tile height: width / aspect, rounded to an even number.
  m_tileHeight = videoAspect > 0 ? static_cast<int>(std::lround(m_width / videoAspect)) : 0;
  m_tileHeight += m_tileHeight % 2;
  m_wanted.insert(index);
  if (!m_extracting)
  {
    m_extracting = true;
    const unsigned int generation = m_generation;
    CServiceBroker::GetJobManager()->Submit([this, generation]() { ExtractWanted(generation); });
  }
  return keepPrevious ? m_shownPath : std::string();
}

int CTrickplayThumbs::NextWanted(int loadedSheet)
{
  // Holding a button moves past tiles faster than they can be made; forget ones left far behind.
  std::erase_if(m_wanted, [this](int index) { return std::abs(index - m_chosen) > MAX_WANTED_DISTANCE; });

  // The chosen tile first, then ones from the sheet already loaded, then the nearest.
  const int perSheet = m_cols * m_rows;
  int best = -1;
  auto bestRank = std::make_tuple(true, true, 0);
  for (const int index : m_wanted)
  {
    const auto rank =
        std::make_tuple(index != m_chosen, index / perSheet != loadedSheet, std::abs(index - m_chosen));
    if (best < 0 || rank < bestRank)
    {
      best = index;
      bestRank = rank;
    }
  }
  if (best >= 0)
    m_wanted.erase(best);
  return best;
}

void CTrickplayThumbs::ExtractWanted(unsigned int generation)
{
  std::unique_ptr<CTexture> sheet;
  int loadedSheet = -1;
  {
    std::unique_lock lock(m_lock);
    if (generation == m_generation)
    {
      sheet = std::move(m_sheet);
      loadedSheet = m_sheetNumber;
    }
  }

  while (true)
  {
    int index;
    int sheetNumber;
    std::string sheetPath;
    std::string outPath;
    int cols;
    int rows;
    int width;
    int tileHeight;
    {
      std::unique_lock lock(m_lock);
      if (generation != m_generation)
      {
        m_extracting = false;
        return;
      }
      index = NextWanted(loadedSheet);
      if (index < 0)
      {
        m_sheet = std::move(sheet);
        m_sheetNumber = loadedSheet;
        m_extracting = false;
        return;
      }
      sheetNumber = index / (m_cols * m_rows);
      sheetPath = URIUtils::AddFileToFolder(m_tileDir, std::to_string(sheetNumber) + ".jpg");
      outPath = URIUtils::AddFileToFolder(m_outDir, std::to_string(index) + ".jpg");
      cols = m_cols;
      rows = m_rows;
      width = m_width;
      tileHeight = m_tileHeight;
    }

    if (sheetNumber != loadedSheet)
    {
      sheet = CTexture::LoadFromFile(sheetPath);
      loadedSheet = sheetNumber;
    }

    bool ok = false;
    if (sheet && sheet->GetPixels())
    {
      const int sheetWidth = static_cast<int>(sheet->GetWidth());
      const int sheetHeight = static_cast<int>(sheet->GetHeight());
      // Sheets may be stored scaled; work in the sheet's own pixels.
      const float scale = static_cast<float>(sheetWidth) / (cols * width);
      const int inSheet = index % (cols * rows);
      const int w = sheetWidth / cols;
      // Jellyfin doesn't always round a tile's height the way the aspect predicts (1280x536
      // gives 132, not 134), and two pixels a row is enough to show a strip of the next row
      // and to push the last row off the sheet. So the aspect only says how many rows this
      // sheet holds, and the height comes from the sheet itself.
      int sheetRows = rows;
      if (tileHeight > 0)
        sheetRows = std::clamp(
            static_cast<int>(std::lround(sheetHeight / (tileHeight * scale))), 1, rows);
      const int h = sheetHeight / sheetRows;
      const int x = (inSheet % cols) * w;
      const int y = (inSheet / cols) * h;
      if (w > 0 && h > 0 && x + w <= sheetWidth && y + h <= sheetHeight)
      {
        uint32_t outWidth = w;
        uint32_t outHeight = h;
        uint8_t* pixels = sheet->GetPixels() + y * sheet->GetPitch() + x * 4;
        ok = CPicture::CacheTexture(pixels, w, h, sheet->GetPitch(), 0, outWidth, outHeight, outPath);
      }
    }

    std::unique_lock lock(m_lock);
    if (generation == m_generation)
      m_ready[index] = ok ? outPath : std::string(); // a failed tile shows nothing, without retrying
  }
}
