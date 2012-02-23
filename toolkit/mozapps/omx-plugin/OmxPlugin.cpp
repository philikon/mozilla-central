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
 *  Sotaro Ikeda <ikeda.sohtaroh@sharp.co.jp>
 *  Anonymous Silicon Vendor contributor
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Ver+sion 2.1 or later (the "LGPL"),
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

#include <DataSource.h>
#include <MediaErrors.h>
#include <MediaExtractor.h>
#include <MediaSource.h>
#include <MetaData.h>
#include <OMXCodec.h>
#include <OMX.h>

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Index.h>
#include <OMX_IVCommon.h>
#include <OMX_Component.h>

#include "mozilla/Types.h"
#include "MPAPI.h"

#include "android/log.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "OmxPlugin" , ## args)

using namespace MPAPI;

namespace android {

// MediaStreamSource is a DataSource that reads from a MPAPI media stream.

class MediaStreamSource : public DataSource {
  PluginHost *mPluginHost;
public:
  MediaStreamSource(PluginHost *aPluginHost, Decoder *aDecoder);

  virtual status_t initCheck() const;
  virtual ssize_t readAt(off_t offset, void *data, size_t size);
  virtual status_t getSize(off_t *size);
  virtual uint32_t flags() {
    return kWantsPrefetching;
  }

  virtual ~MediaStreamSource();

private:
  Decoder *mDecoder;

  MediaStreamSource(const MediaStreamSource &);
  MediaStreamSource &operator=(const MediaStreamSource &);
};

MediaStreamSource::MediaStreamSource(PluginHost *aPluginHost, Decoder *aDecoder) :
  mPluginHost(aPluginHost)
{
  mDecoder = aDecoder;
}

MediaStreamSource::~MediaStreamSource()
{
}

status_t MediaStreamSource::initCheck() const
{
  return OK;
}

ssize_t MediaStreamSource::readAt(off_t offset, void *data, size_t size)
{
  char *ptr = reinterpret_cast<char *>(data);
  size_t todo = size;
  while (todo > 0) {
    uint32_t bytesRead;
    if (!mPluginHost->Read(mDecoder, ptr, offset, todo, &bytesRead)) {
      return ERROR_IO;
    }
    offset += bytesRead;
    todo -= bytesRead;
    ptr += bytesRead;
  }
  return size;
}

status_t MediaStreamSource::getSize(off_t *size)
{
  uint64_t length = mPluginHost->GetLength(mDecoder);
  if (length == static_cast<uint64_t>(-1))
    return ERROR_UNSUPPORTED;

  *size = length;

  return OK;
}

}  // namespace android                                                                                                                               

using namespace android;

class OmxDecoder {
  PluginHost *mPluginHost;
  Decoder *mDecoder;
  sp<MediaSource> mVideoTrack;
  sp<MediaSource> mVideoSource;
  sp<MediaSource> mAudioTrack;
  sp<MediaSource> mAudioSource;
  const char *mVideoDecoderComponent;
  int32_t mVideoWidth;
  int32_t mVideoHeight;
  int32_t mVideoColorFormat;
  int32_t mVideoStride;
  int32_t mVideoSliceHeight;
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;
  int64_t mDurationUs;
  MediaBuffer* mVideoBuffer;
  VideoFrame mVideoFrame;
  MediaBuffer* mAudioBuffer;
  AudioFrame mAudioFrame;

  void ReleaseVideoBuffer();
  void ReleaseAudioBuffer();

  void PlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable, void *aBufferId);
  void CbYCrYFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable, void *aBufferId);
  void SemiPlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable, void *aBufferId);
  void SemiPlanarYVU420Frame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable, void *aBufferId);
  bool ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs, void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable, void *aBufferId);
  bool ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize, int32_t aAudioChannels, int32_t aAudioSampleRate);
public:
  OmxDecoder(PluginHost *aPluginHost, Decoder *aDecoder);
  ~OmxDecoder();

  bool Init();

  void GetDuration(int64_t *durationUs) {
    *durationUs = mDurationUs;
  }

  void GetVideoParameters(int32_t *width, int32_t *height) {
    *width = mVideoWidth;
    *height = mVideoHeight;
  }

  void GetAudioParameters(int32_t *numChannels, int32_t *sampleRate) {
    *numChannels = mAudioChannels;
    *sampleRate = mAudioSampleRate;
  }

  bool HasVideo() {
    return mVideoSource != NULL;
  }

  bool HasAudio() {
    return mAudioSource != NULL;
  }

  bool ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs);
  bool ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs);
};

OmxDecoder::OmxDecoder(PluginHost *aPluginHost, Decoder *aDecoder) :
  mPluginHost(aPluginHost), mDecoder(aDecoder)
{
  mVideoWidth = mVideoHeight = mVideoColorFormat = mVideoStride = mVideoSliceHeight = 0;
  mVideoDecoderComponent = NULL;
  mVideoBuffer = NULL;
  mAudioBuffer = NULL;
}

OmxDecoder::~OmxDecoder()
{
  ReleaseVideoBuffer();
  ReleaseAudioBuffer();
}

class AutoStopMediaSource {
  sp<MediaSource> mMediaSource;
public:
  AutoStopMediaSource(sp<MediaSource> aMediaSource) : mMediaSource(aMediaSource) {
  }

  ~AutoStopMediaSource() {
    mMediaSource->stop();
  }
};

static sp<IOMX> sOMX = NULL;
static sp<IOMX> GetOMX() {
  if(sOMX.get() == NULL) {
    sOMX = new OMX;
    }
  return sOMX;
}

bool OmxDecoder::Init() {
  //register sniffers, if they are not registered in this process.
  DataSource::RegisterDefaultSniffers();

  sp<DataSource> dataSource = new MediaStreamSource(mPluginHost, mDecoder);
  if (dataSource->initCheck()) {
    return false;
  }

  mPluginHost->SetMetaDataReadMode(mDecoder);

  sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
  if (extractor == NULL) {
    return false;
  }

  sp<MediaSource> videoTrack;
  sp<MediaSource> audioTrack;
  const char *audioMime = NULL;
  int32_t audioChannels = 0, audioSampleRate = 0;

  for (size_t i = 0; i < extractor->countTracks(); ++i) {
    sp<MetaData> meta = extractor->getTrackMetaData(i);

    int32_t bitRate;
    if (!meta->findInt32(kKeyBitRate, &bitRate))
      bitRate = 0;
    size_t bytesPerSecond = (bitRate+7)/8;

    const char *mime;
    if (!meta->findCString(kKeyMIMEType, &mime)) {
      continue;
    }

    if (videoTrack == NULL && !strncasecmp(mime, "video/", 6)) {
      videoTrack = extractor->getTrack(i);
    } else if (audioTrack == NULL && !strncasecmp(mime, "audio/", 6)) {
      audioTrack = extractor->getTrack(i);
      audioMime = mime;
      if (!meta->findInt32(kKeyChannelCount, &audioChannels) ||
          !meta->findInt32(kKeySampleRate, &audioSampleRate)) {
        return false;
      }
    }
  }

  if (videoTrack == NULL && audioTrack == NULL) {
    return false;
  }

  mPluginHost->SetPlaybackReadMode(mDecoder);

  int64_t totalDurationUs = 0;

  sp<MediaSource> videoSource;
  int32_t videoWidth = 0, videoHeight = 0;
  const char *videoDecoderComponent = NULL;
  int32_t videoColorFormat = 0, videoStride = 0, videoSliceHeight = 0;
  if (videoTrack != NULL) {
    videoSource = OMXCodec::Create(GetOMX(),
                                   videoTrack->getFormat(),
                                   false, // decoder
                                   videoTrack,
                                   NULL,
                                   0); // flags (prefer hw codecs)
    if (videoSource == NULL) {
      return false;
    }

    if (videoSource->start() != OK) {
      return false;
    }

    int64_t durationUs;
    if (videoTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }

    if (!videoSource->getFormat()->findInt32(kKeyWidth, &videoWidth) ||
        !videoSource->getFormat()->findInt32(kKeyHeight, &videoHeight) ||
        !videoSource->getFormat()->findCString(kKeyDecoderComponent, &videoDecoderComponent) ||
        !videoSource->getFormat()->findInt32(kKeyColorFormat, &videoColorFormat) ) {
      return false;
    }

    LOG("width: %d height: %d component: %s format: %d\n",
        videoWidth, videoHeight, videoDecoderComponent, videoColorFormat);

    if (!videoSource->getFormat()->findInt32(kKeyStride, &videoStride) ) {
      videoStride = videoWidth;
      LOG("stride not available, assuming width");
    }

    if (!videoSource->getFormat()->findInt32(kKeySliceHeight, &videoSliceHeight) ) {
      videoSliceHeight = videoHeight;
      LOG("slice height not available, assuming height");
    }

    LOG("stride: %d slice height: %d",
        videoStride, videoSliceHeight);
  }

  sp<MediaSource> audioSource;
  if (audioTrack != NULL) {
    if (!strcasecmp(audioMime, "audio/raw")) {
      audioSource = audioTrack;
    } else {
      audioSource = OMXCodec::Create(GetOMX(),
                                     audioTrack->getFormat(),
                                     false, // decoder
                                     audioTrack);
    }
    if (audioSource == NULL) {
      return false;
    }
    if (audioSource->start() != OK) {
      return false;
    }

    int64_t durationUs;
    if (audioTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
      if (durationUs > totalDurationUs)
        totalDurationUs = durationUs;
    }
  }

  // set decoder state
  mVideoTrack = videoTrack;
  mVideoSource = videoSource;
  mAudioTrack = audioTrack;
  mAudioSource = audioSource;
  mVideoWidth = videoWidth;
  mVideoHeight = videoHeight;
  mVideoDecoderComponent = videoDecoderComponent;
  mVideoColorFormat = videoColorFormat;
  mVideoStride = videoStride;
  mVideoSliceHeight = videoSliceHeight;
  mAudioChannels = audioChannels;
  mAudioSampleRate = audioSampleRate;
  mDurationUs = totalDurationUs;

  return true;
}

void OmxDecoder::ReleaseVideoBuffer() {
  if (mVideoBuffer)
    mVideoBuffer->release();
  mVideoBuffer = NULL;
}

void OmxDecoder::ReleaseAudioBuffer() {
  if (mAudioBuffer)
    mAudioBuffer->release();
  mAudioBuffer = NULL;
}

void OmxDecoder::PlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs,void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable,
                                   void *aBufferId) {
  void *y = aData;
  void *u = static_cast<uint8_t *>(y) + mVideoStride * mVideoSliceHeight;
  void *v = static_cast<uint8_t *>(u) + mVideoStride/2 * mVideoSliceHeight/2;

  aFrame->Set(aTimeUs, aKeyFrame, aUnreadable, aBufferId,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              u, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0,
              v, mVideoStride/2, mVideoWidth/2, mVideoHeight/2, 0, 0);
}

void OmxDecoder::CbYCrYFrame(VideoFrame *aFrame, int64_t aTimeUs,void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable,
                             void *aBufferId) {
  aFrame->Set(aTimeUs, aKeyFrame, aUnreadable, aBufferId,
              aData, mVideoStride, mVideoWidth, mVideoHeight, 1, 1,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 3,
              aData, mVideoStride, mVideoWidth/2, mVideoHeight/2, 2, 3);
}

void OmxDecoder::SemiPlanarYUV420Frame(VideoFrame *aFrame, int64_t aTimeUs,void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable,
                                       void *aBufferId) {
  void *y = aData;
  void *uv = static_cast<uint8_t *>(y) + (mVideoStride * mVideoSliceHeight);

  aFrame->Set(aTimeUs, aKeyFrame, aUnreadable, aBufferId,
              y, mVideoStride, mVideoWidth, mVideoHeight, 0, 0,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 0, 1,
              uv, mVideoStride, mVideoWidth/2, mVideoHeight/2, 1, 1);
}

void OmxDecoder::SemiPlanarYVU420Frame(VideoFrame *aFrame, int64_t aTimeUs,void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable,
                                       void *aBufferId) {
  SemiPlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame, aUnreadable, aBufferId);
  aFrame->Cb.mOffset = 1;
  aFrame->Cr.mOffset = 0;
}

bool OmxDecoder::ToVideoFrame(VideoFrame *aFrame, int64_t aTimeUs,void *aData, size_t aSize, bool aKeyFrame, bool aUnreadable,
                              void *aBufferId) {
  const int OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00;
  const int QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03;

  switch (mVideoColorFormat) {
  case OMX_COLOR_FormatYUV420Planar:
    PlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame, aUnreadable, aBufferId);
    break;
  case OMX_COLOR_FormatCbYCrY:
    CbYCrYFrame(aFrame, aTimeUs, aData, aSize, aKeyFrame, aUnreadable, aBufferId);
    break;
  case OMX_COLOR_FormatYUV420SemiPlanar:
    SemiPlanarYUV420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame, aUnreadable, aBufferId);
    break;
  case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
    SemiPlanarYVU420Frame(aFrame, aTimeUs, aData, aSize, aKeyFrame, aUnreadable, aBufferId);
    break;
  default:
    return false;
  }
  return true;
}

bool OmxDecoder::ToAudioFrame(AudioFrame *aFrame, int64_t aTimeUs, void *aData, size_t aDataOffset, size_t aSize, int32_t aAudioChannels, int32_t aAudioSampleRate)
{
  aFrame->Set(aTimeUs, reinterpret_cast<char *>(aData) + aDataOffset, aSize, aAudioChannels, aAudioSampleRate);
  return true;
}

bool OmxDecoder::ReadVideo(VideoFrame *aFrame, int64_t aSeekTimeUs)
{
  for (;;) {
    ReleaseVideoBuffer();

    status_t err;

    if (aSeekTimeUs != -1) {
      MediaSource::ReadOptions options;
      options.setSeekTo(aSeekTimeUs);
      err = mVideoSource->read(&mVideoBuffer, &options);
    } else {
      err = mVideoSource->read(&mVideoBuffer);
    }

    aSeekTimeUs = -1;

    if (err == OK) {
      if (mVideoBuffer->range_length() == 0) // If we get a spurious empty buffer, keep going
        continue;

      int64_t timeUs;
      int32_t unreadable;
      int32_t keyFrame;
      void *bufferId;

      if (!mVideoBuffer->meta_data()->findInt32(kKeyIsSyncFrame, &keyFrame)) {
        keyFrame = 0;
      }

      if (!mVideoBuffer->meta_data()->findInt32(kKeyIsUnreadable, &unreadable)) {
        unreadable = 0;
      }

      if (!mVideoBuffer->meta_data()->findPointer(kKeyBufferID, &bufferId)) {
        bufferId = 0;
      }

      if (!mVideoBuffer->meta_data()->findInt64(kKeyTime, &timeUs) ) {
        LOG("no key time");
        return false;
      }

      return ToVideoFrame(aFrame, timeUs, mVideoBuffer->data() + mVideoBuffer->range_offset(), mVideoBuffer->range_length(),
                          keyFrame, unreadable, bufferId);
    }

    if (err == INFO_FORMAT_CHANGED) {
      // If the format changed, update our cached info.
      if (!mVideoSource->getFormat()->findInt32(kKeyWidth, &mVideoWidth) ||
          !mVideoSource->getFormat()->findInt32(kKeyHeight, &mVideoHeight) ||
          !mVideoSource->getFormat()->findInt32(kKeyColorFormat, &mVideoColorFormat) ) {
        LOG("bad format");
        return false;
      }

      LOG("format changed -- width: %d height: %d format: %d\n",
          mVideoWidth, mVideoHeight, mVideoColorFormat);

      if (!mVideoSource->getFormat()->findInt32(kKeyStride, &mVideoStride) ) {
        LOG("stride not available, assuming width");
        mVideoStride = mVideoWidth;
      }
  
      if (!mVideoSource->getFormat()->findInt32(kKeySliceHeight, &mVideoSliceHeight) ) {
        LOG("slice height not available, assuming height");
        mVideoSliceHeight = mVideoHeight;
      }

      LOG("stride: %d slice height: %d",
          mVideoStride, mVideoSliceHeight);

      // Ok, try to read a buffer again.
      continue;
    }

    /* err == ERROR_END_OF_STREAM */
    LOG("err = %d", err);
    break;
  }

  return false;
}

bool OmxDecoder::ReadAudio(AudioFrame *aFrame, int64_t aSeekTimeUs)
{
  do {
    ReleaseAudioBuffer();

    status_t err;

    if (aSeekTimeUs != -1) {
      MediaSource::ReadOptions options;
      options.setSeekTo(aSeekTimeUs);
      err = mAudioSource->read(&mAudioBuffer, &options);
    } else {
      err = mAudioSource->read(&mAudioBuffer);
    }

    aSeekTimeUs = -1;

    if (err == OK) {
      if (mAudioBuffer->range_length() == 0) // If we get a spurious empty buffer, keep going
        continue;

      int64_t timeUs;
      if (!mAudioBuffer->meta_data()->findInt64(kKeyTime, &timeUs))
        return false;
      return ToAudioFrame(aFrame, timeUs,
                          mAudioBuffer->data(),
                          mAudioBuffer->range_offset(),
                          mAudioBuffer->range_length(),
                          mAudioChannels, mAudioSampleRate);
    }

    if (err == INFO_FORMAT_CHANGED) {
      // If the format changed, update our cached info.
      if (!mAudioSource->getFormat()->findInt32(kKeyChannelCount, &mAudioChannels) ||
          !mAudioSource->getFormat()->findInt32(kKeySampleRate, &mAudioSampleRate)) {
        return false;
      }

      // Ok, try to read a buffer again.
      continue;
    }

    /* err == ERROR_END_OF_STREAM */
    break;
  } while (0);

  return false;
}

static OmxDecoder *cast(Decoder *decoder) {
  return reinterpret_cast<OmxDecoder *>(decoder->mPrivate);
}

static void GetDuration(Decoder *aDecoder, int64_t *durationUs) {
  cast(aDecoder)->GetDuration(durationUs);
}

static void GetVideoParameters(Decoder *aDecoder, int32_t *width, int32_t *height) {
  cast(aDecoder)->GetVideoParameters(width, height);
}

static void GetAudioParameters(Decoder *aDecoder, int32_t *numChannels, int32_t *sampleRate) {
  cast(aDecoder)->GetAudioParameters(numChannels, sampleRate);
}

static bool HasVideo(Decoder *aDecoder) {
  return cast(aDecoder)->HasVideo();
}

static bool HasAudio(Decoder *aDecoder) {
  return cast(aDecoder)->HasAudio();
}

static bool ReadVideo(Decoder *aDecoder, VideoFrame *aFrame, int64_t aSeekTimeUs)
{
  return cast(aDecoder)->ReadVideo(aFrame, aSeekTimeUs);
}

static bool ReadAudio(Decoder *aDecoder, AudioFrame *aFrame, int64_t aSeekTimeUs)
{
  return cast(aDecoder)->ReadAudio(aFrame, aSeekTimeUs);
}

static void DestroyDecoder(Decoder *aDecoder)
{
  if (aDecoder->mPrivate)
    delete reinterpret_cast<OmxDecoder *>(aDecoder->mPrivate);
}

static bool Match(const char *aMimeChars, size_t aMimeLen, const char *aNeedle)
{
  return !strncmp(aMimeChars, aNeedle, aMimeLen);
}

static const char* const gCodecs[] = {
  "avc",
  "mp4v",
  "mp4a",
  NULL
};

static bool CanDecode(const char *aMimeChars, size_t aMimeLen, const char* const**aCodecs)
{
  if (!Match(aMimeChars, aMimeLen, "video/mp4") &&
      !Match(aMimeChars, aMimeLen, "audio/mp4") &&
      !Match(aMimeChars, aMimeLen, "application/octet-stream")) { // file urls
    return false;
  }
  *aCodecs = gCodecs;

  return true;
}

static bool CreateDecoder(PluginHost *aPluginHost, Decoder *aDecoder, const char *aMimeChars, size_t aMimeLen)
{
  OmxDecoder *omx = new OmxDecoder(aPluginHost, aDecoder);
  if (!omx || !omx->Init())
    return false;

  aDecoder->mPrivate = omx;
  aDecoder->GetDuration = GetDuration;
  aDecoder->GetVideoParameters = GetVideoParameters;
  aDecoder->GetAudioParameters = GetAudioParameters;
  aDecoder->HasVideo = HasVideo;
  aDecoder->HasAudio = HasAudio;
  aDecoder->ReadVideo = ReadVideo;
  aDecoder->ReadAudio = ReadAudio;
  aDecoder->DestroyDecoder = DestroyDecoder;

  return true;
}

// Export the manifest so MPAPI can find our entry points.
Manifest MOZ_EXPORT_DATA(MPAPI_MANIFEST) {
  CanDecode,
  CreateDecoder
};
