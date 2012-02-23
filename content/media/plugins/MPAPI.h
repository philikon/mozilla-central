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
#if !defined(MPAPI_h_)
#define MPAPI_h_

#include <stdint.h>

namespace MPAPI {

struct VideoPlane {
  void *mData;
  int32_t mStride;
  int32_t mWidth;
  int32_t mHeight;
  int32_t mOffset;
  int32_t mSkip;
};

struct VideoFrame {
  int64_t mTimeUs;
  bool mKeyFrame;
  bool mUnreadable;
  void *mBufferId;
  VideoPlane Y;
  VideoPlane Cb;
  VideoPlane Cr;

  void Set(int64_t aTimeUs, bool aKeyFrame, bool aUnreadable, void *aBufferId,
           void *aYData, int32_t aYStride, int32_t aYWidth, int32_t aYHeight, int32_t aYOffset, int32_t aYSkip,
           void *aCbData, int32_t aCbStride, int32_t aCbWidth, int32_t aCbHeight, int32_t aCbOffset, int32_t aCbSkip,
           void *aCrData, int32_t aCrStride, int32_t aCrWidth, int32_t aCrHeight, int32_t aCrOffset, int32_t aCrSkip)
  {
    mTimeUs = aTimeUs;
    mKeyFrame = aKeyFrame;
    mUnreadable = aUnreadable;
    mBufferId = aBufferId;
    Y.mData = aYData;
    Y.mStride = aYStride;
    Y.mWidth = aYWidth;
    Y.mHeight = aYHeight;
    Y.mOffset = aYOffset;
    Y.mSkip = aYSkip;
    Cb.mData = aCbData;
    Cb.mStride = aCbStride;
    Cb.mWidth = aCbWidth;
    Cb.mHeight = aCbHeight;
    Cb.mOffset = aCbOffset;
    Cb.mSkip = aCbSkip;
    Cr.mData = aCrData;
    Cr.mStride = aCrStride;
    Cr.mWidth = aCrWidth;
    Cr.mHeight = aCrHeight;
    Cr.mOffset = aCrOffset;
    Cr.mSkip = aCrSkip;
  }
};

struct AudioFrame {
  int64_t mTimeUs;
  void *mData; // 16PCM interleaved
  size_t mSize; // Size of mData in bytes
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;

  void Set(int64_t aTimeUs,
           void *aData, size_t aSize,
           int32_t aAudioChannels, int32_t aAudioSampleRate)
  {
    mTimeUs = aTimeUs;
    mData = aData;
    mSize = aSize;
    mAudioChannels = aAudioChannels;
    mAudioSampleRate = aAudioSampleRate;
  }
};

struct Decoder;

struct PluginHost {
  bool (*Read)(Decoder *aDecoder, char* aBuffer, int64_t aOffset, uint32_t aCount, uint32_t* aBytes);
  uint64_t (*GetLength)(Decoder *aDecoder);
  void (*SetMetaDataReadMode)(Decoder *aDecoder);
  void (*SetPlaybackReadMode)(Decoder *aDecoder);
};

struct Decoder {
  void *mResource;
  void *mPrivate;

  Decoder();

  void (*GetDuration)(Decoder *aDecoder, int64_t *durationUs);
  void (*GetVideoParameters)(Decoder *aDecoder, int32_t *width, int32_t *height);
  void (*GetAudioParameters)(Decoder *aDecoder, int32_t *numChannels, int32_t *sampleRate);
  bool (*HasVideo)(Decoder *aDecoder);
  bool (*HasAudio)(Decoder *aDecoder);
  bool (*ReadVideo)(Decoder *aDecoder, VideoFrame *aFrame, int64_t aSeekTimeUs);
  bool (*ReadAudio)(Decoder *aDecoder, AudioFrame *aFrame, int64_t aSeekTimeUs);
  void (*DestroyDecoder)(Decoder *);
};

struct Manifest {
  bool (*CanDecode)(const char *aMimeChars, size_t aMimeLen, const char* const**aCodecs);
  bool (*CreateDecoder)(PluginHost *aPluginHost, Decoder *aDecoder,
                        const char *aMimeChars, size_t aMimeLen);
};

}

#endif
