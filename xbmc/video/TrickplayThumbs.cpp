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
#include <mutex>

using namespace XFILE;

namespace
{
constexpr int INTERVAL_MS = 10000; // Jellyfin's default trickplay interval
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
  m_shown = -1;
  m_shownPath.clear();
  m_sheetPath.clear();
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
                                       bool keepPrevious)
{
  std::unique_lock lock(m_lock);

  if (mediaPath != m_media)
    StartLocating(mediaPath);
  if (m_tileDir.empty())
    return {};

  const int index = std::max(0, static_cast<int>(seconds * 1000 / INTERVAL_MS));
  if (index != m_shown && m_pending < 0)
  {
    // Jellyfin's tile height: width / aspect, rounded to an even number.
    int tileHeight = videoAspect > 0 ? static_cast<int>(std::lround(m_width / videoAspect)) : 0;
    tileHeight += tileHeight % 2;
    m_pending = index;
    const unsigned int generation = m_generation;
    CServiceBroker::GetJobManager()->Submit([this, generation, index, tileHeight]()
                                            { Extract(generation, index, tileHeight); });
  }
  return keepPrevious || index == m_shown ? m_shownPath : std::string();
}

void CTrickplayThumbs::Extract(unsigned int generation, int index, int tileHeight)
{
  std::string sheetPath;
  std::string loadedPath;
  std::unique_ptr<CTexture> sheet;
  std::string outPath;
  int cols;
  int rows;
  int width;
  {
    std::unique_lock lock(m_lock);
    if (generation != m_generation)
    {
      m_pending = -1;
      return;
    }
    const int perSheet = m_cols * m_rows;
    sheetPath = URIUtils::AddFileToFolder(m_tileDir, std::to_string(index / perSheet) + ".jpg");
    outPath = URIUtils::AddFileToFolder(m_outDir, std::to_string(index) + ".jpg");
    cols = m_cols;
    rows = m_rows;
    width = m_width;
    // Only one job runs at a time; it keeps the sheet for its next tile if the file hasn't changed.
    sheet = std::move(m_sheet);
    loadedPath = m_sheetPath;
  }

  bool ok = false;
  if (sheetPath != loadedPath)
  {
    sheet = CTexture::LoadFromFile(sheetPath);
    loadedPath = sheetPath;
  }
  if (sheet && sheet->GetPixels())
  {
    const int sheetWidth = static_cast<int>(sheet->GetWidth());
    const int sheetHeight = static_cast<int>(sheet->GetHeight());
    // Sheets may be stored scaled; work in the sheet's own pixels.
    const float scale = static_cast<float>(sheetWidth) / (cols * width);
    const int inSheet = index % (cols * rows);
    const int w = sheetWidth / cols;
    const int h = tileHeight > 0 ? static_cast<int>(std::lround(tileHeight * scale))
                                 : sheetHeight / rows;
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
  m_pending = -1;
  if (generation == m_generation)
  {
    m_sheet = std::move(sheet);
    m_sheetPath = loadedPath;
    // On failure show nothing for this tile rather than retrying every frame.
    m_shown = index;
    m_shownPath = ok ? outPath : std::string();
  }
}
