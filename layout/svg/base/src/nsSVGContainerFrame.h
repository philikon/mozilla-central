/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef NS_SVGCONTAINERFRAME_H
#define NS_SVGCONTAINERFRAME_H

#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsContainerFrame.h"
#include "nsFrame.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsQueryFrame.h"
#include "nsRect.h"

class nsFrameList;
class nsIContent;
class nsIPresShell;
class nsRenderingContext;
class nsStyleContext;

struct nsPoint;

typedef nsContainerFrame nsSVGContainerFrameBase;

/**
 * Base class for SVG container frames. Frame sub-classes that do not
 * display their contents directly (such as the frames for <marker> or
 * <pattern>) just inherit this class. Frame sub-classes that do or can
 * display their contents directly (such as the frames for inner-<svg> or
 * <g>) inherit our nsDisplayContainerFrame sub-class.
 */
class nsSVGContainerFrame : public nsSVGContainerFrameBase
{
  friend nsIFrame* NS_NewSVGContainerFrame(nsIPresShell* aPresShell,
                                           nsStyleContext* aContext);
protected:
  nsSVGContainerFrame(nsStyleContext* aContext)
    : nsSVGContainerFrameBase(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT);
  }

public:
  NS_DECL_QUERYFRAME_TARGET(nsSVGContainerFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // Returns the transform to our gfxContext (to device pixels, not CSS px)
  virtual gfxMatrix GetCanvasTM() { return gfxMatrix(); }

  /**
   * Returns true if the frame's content has a transform that applies only to
   * its children, and not to the frame itself. For example, an implicit
   * transform introduced by a 'viewBox' attribute, or an explicit transform
   * due to a root-<svg> having its currentScale/currentTransform properties
   * set. If aTransform is non-null, then it will be set to the transform.
   */
  virtual bool HasChildrenOnlyTransform(gfxMatrix *aTransform) const {
    return false;
  }

  // nsIFrame:
  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList);
  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);
  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame);

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsSVGContainerFrameBase::IsFrameOfType(
            aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGContainer));
  }

  virtual bool UpdateOverflow();
};

/**
 * Frame class or base-class for SVG containers that can or do display their
 * contents directly.
 */
class nsSVGDisplayContainerFrame : public nsSVGContainerFrame,
                                   public nsISVGChildFrame
{
protected:
  nsSVGDisplayContainerFrame(nsStyleContext* aContext)
    : nsSVGContainerFrame(aContext)
  {
     AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
  }

public:
  NS_DECL_QUERYFRAME_TARGET(nsSVGDisplayContainerFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);
  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame);
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual bool IsSVGTransformed(gfxMatrix *aOwnTransform = nsnull,
                                gfxMatrix *aFromParentTransform = nsnull) const;

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);
  NS_IMETHOD_(nsRect) GetCoveredRegion();
  virtual void UpdateBounds();
  virtual void NotifySVGChanged(PRUint32 aFlags);
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags);
  NS_IMETHOD_(bool) IsDisplayContainer() { return true; }
};

#endif
