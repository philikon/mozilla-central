/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/TimeStamp.h"
#include "nsTimeRanges.h"
#include "MediaResource.h"
#include "nsHTMLMediaElement.h"
#include "nsMediaPluginHost.h"
#include "nsXPCOMStrings.h"
#include "nsISeekableStream.h"
#include "pratom.h"
#include "nsMediaPluginReader.h"

#include "android/log.h"
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "MediaPluginHost" , ## args)

#include "MPAPI.h"

using namespace MPAPI;
using namespace mozilla;

static MediaResource *GetResource(Decoder *aDecoder)
{
  return reinterpret_cast<MediaResource *>(aDecoder->mResource);
}

static bool Read(Decoder *aDecoder, char *aBuffer, int64_t aOffset, uint32_t aCount, uint32_t* aBytes)
{
  MediaResource *resource = GetResource(aDecoder);
  if (aOffset != resource->Tell()) {
    nsresult rv = resource->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
    if (NS_FAILED(rv)) {
      LOG("seek failed");
      return false;
    }
  }
  nsresult rv = resource->Read(aBuffer, aCount, aBytes);
  if (NS_FAILED(rv)) {
    LOG("read failed");
    return false;
  }
  return true;
}

static uint64_t GetLength(Decoder *aDecoder)
{
  return GetResource(aDecoder)->GetLength();
}

static void SetMetaDataReadMode(Decoder *aDecoder)
{
  GetResource(aDecoder)->SetReadMode(nsMediaCacheStream::MODE_METADATA);
}

static void SetPlaybackReadMode(Decoder *aDecoder)
{
  GetResource(aDecoder)->SetReadMode(nsMediaCacheStream::MODE_PLAYBACK);
}

static PluginHost sPluginHost = {
  Read,
  GetLength,
  SetMetaDataReadMode,
  SetPlaybackReadMode
};

void nsMediaPluginHost::TryLoad(const char *name)
{
  PRLibrary *lib = PR_LoadLibrary("libomxplugin.so");
  if (lib) {
    Manifest *manifest = static_cast<Manifest *>(PR_FindSymbol(lib, "MPAPI_MANIFEST"));
    if (manifest)
      mPlugins.AppendElement(manifest);
  }
}

nsMediaPluginHost::nsMediaPluginHost() {
  TryLoad("libomxplugin.so");
}

bool nsMediaPluginHost::FindDecoder(const nsACString& aMimeType, const char* const** aCodecs)
{
  const char *chars;
  size_t len = NS_CStringGetData(aMimeType, &chars, nsnull);
  for (size_t n = 0; n < mPlugins.Length(); ++n) {
    Manifest *plugin = mPlugins[n];
    const char* const *codecs;
    if (plugin->CanDecode(chars, len, &codecs)) {
      if (aCodecs)
        *aCodecs = codecs;
      return true;
    }
  }
  return false;
}

Decoder::Decoder() :
  mResource(NULL), mPrivate(NULL)
{
}

MPAPI::Decoder *nsMediaPluginHost::CreateDecoder(MediaResource *aResource, const nsACString& aMimeType)
{
  const char *chars;
  size_t len = NS_CStringGetData(aMimeType, &chars, nsnull);

  Decoder *decoder = new Decoder();
  if (!decoder) {
    return nsnull;
  }
  decoder->mResource = aResource;

  for (size_t n = 0; n < mPlugins.Length(); ++n) {
    Manifest *plugin = mPlugins[n];
    const char* const *codecs;
    if (!plugin->CanDecode(chars, len, &codecs)) {
      continue;
    }
    if (plugin->CreateDecoder(&sPluginHost, decoder, chars, len)) {
      return decoder;
    }
  }

  return nsnull;
}

void nsMediaPluginHost::DestroyDecoder(Decoder *aDecoder)
{
  aDecoder->DestroyDecoder(aDecoder);
  delete aDecoder;
}

nsMediaPluginHost *sMediaPluginHost;
nsMediaPluginHost *GetMediaPluginHost()
{
  if (!sMediaPluginHost) {
    sMediaPluginHost = new nsMediaPluginHost();
  }
  return sMediaPluginHost;
}
