/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#include "SVGTransform.h"
#include "nsContentUtils.h"
#include "nsTextFormatter.h"

namespace {
  const double radPerDegree = 2.0*3.1415926535 / 360.0;
}

namespace mozilla {

void
SVGTransform::GetValueAsString(nsAString& aValue) const
{
  PRUnichar buf[256];

  switch (mType) {
    case nsIDOMSVGTransform::SVG_TRANSFORM_TRANSLATE:
      // The spec say that if Y is not provided, it is assumed to be zero.
      if (mMatrix.y0 != 0)
        nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
            NS_LITERAL_STRING("translate(%g, %g)").get(),
            mMatrix.x0, mMatrix.y0);
      else
        nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
            NS_LITERAL_STRING("translate(%g)").get(),
            mMatrix.x0);
      break;
    case nsIDOMSVGTransform::SVG_TRANSFORM_ROTATE:
      if (mOriginX != 0.0f || mOriginY != 0.0f)
        nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
            NS_LITERAL_STRING("rotate(%g, %g, %g)").get(),
            mAngle, mOriginX, mOriginY);
      else
        nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
            NS_LITERAL_STRING("rotate(%g)").get(), mAngle);
      break;
    case nsIDOMSVGTransform::SVG_TRANSFORM_SCALE:
      if (mMatrix.xx != mMatrix.yy)
        nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
            NS_LITERAL_STRING("scale(%g, %g)").get(), mMatrix.xx, mMatrix.yy);
      else
        nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
            NS_LITERAL_STRING("scale(%g)").get(), mMatrix.xx);
      break;
    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWX:
      nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                                NS_LITERAL_STRING("skewX(%g)").get(), mAngle);
      break;
    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY:
      nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                                NS_LITERAL_STRING("skewY(%g)").get(), mAngle);
      break;
    case nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX:
      nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
          NS_LITERAL_STRING("matrix(%g, %g, %g, %g, %g, %g)").get(),
                            mMatrix.xx, mMatrix.yx,
                            mMatrix.xy, mMatrix.yy,
                            mMatrix.x0, mMatrix.y0);
      break;
    default:
      buf[0] = '\0';
      NS_ERROR("unknown transformation type");
      break;
  }

  aValue.Assign(buf);
}

void
SVGTransform::SetMatrix(const gfxMatrix& aMatrix)
{
  mType    = nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX;
  mMatrix  = aMatrix;
  // We set the other members here too, since operator== requires it and
  // the DOM requires it for mAngle.
  mAngle   = 0.f;
  mOriginX = 0.f;
  mOriginY = 0.f;
}

void
SVGTransform::SetTranslate(float aTx, float aTy)
{
  mType    = nsIDOMSVGTransform::SVG_TRANSFORM_TRANSLATE;
  mMatrix.Reset();
  mMatrix.x0 = aTx;
  mMatrix.y0 = aTy;
  mAngle   = 0.f;
  mOriginX = 0.f;
  mOriginY = 0.f;
}

void
SVGTransform::SetScale(float aSx, float aSy)
{
  mType    = nsIDOMSVGTransform::SVG_TRANSFORM_SCALE;
  mMatrix.Reset();
  mMatrix.xx = aSx;
  mMatrix.yy = aSy;
  mAngle   = 0.f;
  mOriginX = 0.f;
  mOriginY = 0.f;
}

void
SVGTransform::SetRotate(float aAngle, float aCx, float aCy)
{
  mType    = nsIDOMSVGTransform::SVG_TRANSFORM_ROTATE;
  mMatrix.Reset();
  mMatrix.Translate(gfxPoint(aCx, aCy));
  mMatrix.Rotate(aAngle*radPerDegree);
  mMatrix.Translate(gfxPoint(-aCx, -aCy));
  mAngle   = aAngle;
  mOriginX = aCx;
  mOriginY = aCy;
}

nsresult
SVGTransform::SetSkewX(float aAngle)
{
  double ta = tan(aAngle*radPerDegree);
  NS_ENSURE_FINITE(ta, NS_ERROR_DOM_SVG_INVALID_VALUE_ERR);

  mType    = nsIDOMSVGTransform::SVG_TRANSFORM_SKEWX;
  mMatrix.Reset();
  mMatrix.xy = ta;
  mAngle   = aAngle;
  mOriginX = 0.f;
  mOriginY = 0.f;
  return NS_OK;
}

nsresult
SVGTransform::SetSkewY(float aAngle)
{
  double ta = tan(aAngle*radPerDegree);
  NS_ENSURE_FINITE(ta, NS_ERROR_DOM_SVG_INVALID_VALUE_ERR);

  mType    = nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY;
  mMatrix.Reset();
  mMatrix.yx = ta;
  mAngle   = aAngle;
  mOriginX = 0.f;
  mOriginY = 0.f;
  return NS_OK;
}

SVGTransformSMILData::SVGTransformSMILData(const SVGTransform& aTransform)
  : mTransformType(aTransform.Type())
{
  NS_ABORT_IF_FALSE(
    mTransformType >= nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX &&
    mTransformType <= nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY,
    "Unexpected transform type");

  for (PRUint32 i = 0; i < NUM_STORED_PARAMS; ++i) {
    mParams[i] = 0.f;
  }

  switch (mTransformType) {
    case nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX: {
      const gfxMatrix& mx = aTransform.Matrix();
      mParams[0] = static_cast<float>(mx.xx);
      mParams[1] = static_cast<float>(mx.yx);
      mParams[2] = static_cast<float>(mx.xy);
      mParams[3] = static_cast<float>(mx.yy);
      mParams[4] = static_cast<float>(mx.x0);
      mParams[5] = static_cast<float>(mx.y0);
      break;
    }
    case nsIDOMSVGTransform::SVG_TRANSFORM_TRANSLATE: {
      const gfxMatrix& mx = aTransform.Matrix();
      mParams[0] = static_cast<float>(mx.x0);
      mParams[1] = static_cast<float>(mx.y0);
      break;
    }
    case nsIDOMSVGTransform::SVG_TRANSFORM_SCALE: {
      const gfxMatrix& mx = aTransform.Matrix();
      mParams[0] = static_cast<float>(mx.xx);
      mParams[1] = static_cast<float>(mx.yy);
      break;
    }
    case nsIDOMSVGTransform::SVG_TRANSFORM_ROTATE:
      mParams[0] = aTransform.Angle();
      aTransform.GetRotationOrigin(mParams[1], mParams[2]);
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWX:
    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY:
      mParams[0] = aTransform.Angle();
      break;

    default:
      NS_NOTREACHED("Unexpected transform type");
      break;
  }
}

SVGTransform
SVGTransformSMILData::ToSVGTransform() const
{
  SVGTransform result;

  switch (mTransformType) {
    case nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX:
      result.SetMatrix(gfxMatrix(mParams[0], mParams[1],
                                 mParams[2], mParams[3],
                                 mParams[4], mParams[5]));
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_TRANSLATE:
      result.SetTranslate(mParams[0], mParams[1]);
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SCALE:
      result.SetScale(mParams[0], mParams[1]);
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_ROTATE:
      result.SetRotate(mParams[0], mParams[1], mParams[2]);
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWX:
      result.SetSkewX(mParams[0]);
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY:
      result.SetSkewY(mParams[0]);
      break;

    default:
      NS_NOTREACHED("Unexpected transform type");
      break;
  }
  return result;
}

} // namespace mozilla
