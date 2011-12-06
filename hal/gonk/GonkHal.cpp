/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *   Michael Wu <mwu@mozilla.com>
 *   Justin Lebar <justin.lebar@gmail.com>
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

#include "GonkGlue.h"
#include "Hal.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/FileUtils.h"
#include "nsAlgorithm.h"
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>

using mozilla::hal::WindowIdentifier;

namespace mozilla {
namespace hal_impl {

void
Vibrate(const nsTArray<uint32>& pattern, const WindowIdentifier &)
{}

void
CancelVibrate(const WindowIdentifier &)
{}

void
EnableBatteryNotifications()
{
    mozilla::CheckBattery(true);
}

void
DisableBatteryNotifications()
{
    mozilla::CheckBattery(false);
}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  FILE *capacityFile = fopen("/sys/class/power_supply/battery/capacity", "r");
  double capacity = dom::battery::kDefaultLevel * 100;
  if (capacityFile)
    fscanf(capacityFile, "%lf", &capacity);
  fclose(capacityFile);

  FILE *chargingFile = fopen("/sys/class/power_supply/battery/charging_source", "r");
  int chargingSrc = 1;
  if (chargingFile)
    fscanf(chargingFile, "%d", &chargingSrc);
  fclose(chargingFile);

  aBatteryInfo->level() = capacity / 100;
  aBatteryInfo->charging() = chargingSrc == 1;
  aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
}

namespace {

/**
 * RAII class to help us remember to close file descriptors.
 */
const char *screenEnabledFilename = "/sys/power/state";
const char *screenBrightnessFilename = "/sys/class/backlight/pwm-backlight/brightness";

template<ssize_t n>
bool ReadFromFile(const char *filename, char (&buf)[n])
{
  int fd = open(filename, O_RDONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return false;
  }

  ssize_t numRead = read(fd, buf, n);
  if (numRead < 0) {
    HAL_LOG(("Error reading from file %s.", filename));
    return false;
  }

  buf[PR_MIN(numRead, n - 1)] = '\0';
  return true;
}

void WriteToFile(const char *filename, const char *toWrite)
{
  int fd = open(filename, O_WRONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return;
  }

  if (write(fd, toWrite, strlen(toWrite)) < 0) {
    HAL_LOG(("Unable to write to file %s.", filename));
    return;
  }
}

// We can write to screenEnabledFilename to enable/disable the screen, but when
// we read, we always get "mem"!  So we have to keep track ourselves whether
// the screen is on or not.
bool sScreenEnabled = true;

} // anonymous namespace

bool
GetScreenEnabled()
{
  return sScreenEnabled;
}

void
SetScreenEnabled(bool enabled)
{
  WriteToFile(screenEnabledFilename, enabled ? "on" : "mem");
  sScreenEnabled = enabled;
}

double
GetScreenBrightness()
{
  char buf[32];
  ReadFromFile(screenBrightnessFilename, buf);

  errno = 0;
  unsigned long val = strtoul(buf, NULL, 10);
  if (errno) {
    HAL_LOG(("Cannot parse contents of %s; expected an unsigned "
             "int, but contains \"%s\".",
             screenBrightnessFilename, buf));
    return 1;
  }

  if (val > 255) {
    HAL_LOG(("Got out-of-range brightness %d, truncating to 1.0", val));
    val = 255;
  }

  return val / 255.0;
}

void
SetScreenBrightness(double brightness)
{
  // Don't use De Morgan's law to push the ! into this expression; we want to
  // catch NaN too.
  if (!(0 <= brightness && brightness <= 1)) {
    HAL_LOG(("SetScreenBrightness: Dropping illegal brightness %f.",
             brightness));
    return;
  }

  // Convert the value in [0, 1] to an int between 0 and 255, then write to a
  // string.
  int val = static_cast<int>(round(brightness * 255));
  char str[4];
  DebugOnly<int> numChars = snprintf(str, sizeof(str), "%d", val);
  MOZ_ASSERT(numChars < static_cast<int>(sizeof(str)));

  WriteToFile(screenBrightnessFilename, str);
}

} // hal_impl
} // mozilla
