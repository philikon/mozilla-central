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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"

#include "nsCSSRendering.h"
#include "prprf.h"         // For PR_snprintf()

#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMElement.h"

#include "nsIDOMEventTarget.h"

#include "nsMathMLmactionFrame.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"

//
// <maction> -- bind actions to a subexpression - implementation
//

#define NS_MATHML_ACTION_TYPE_NONE         0
#define NS_MATHML_ACTION_TYPE_TOGGLE       1
#define NS_MATHML_ACTION_TYPE_STATUSLINE   2
#define NS_MATHML_ACTION_TYPE_TOOLTIP      3 // unsupported


// helper function to parse actiontype attribute
static PRInt32
GetActionType(nsIContent* aContent)
{
  nsAutoString value;

  if (aContent)
    aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::actiontype_, value);

  if (value.EqualsLiteral("toggle"))
    return NS_MATHML_ACTION_TYPE_TOGGLE;
  if (value.EqualsLiteral("statusline"))
    return NS_MATHML_ACTION_TYPE_STATUSLINE;
  if (value.EqualsLiteral("tooltip"))
    return NS_MATHML_ACTION_TYPE_TOOLTIP;

  return NS_MATHML_ACTION_TYPE_NONE;
}

nsIFrame*
NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmactionFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmactionFrame)

nsMathMLmactionFrame::~nsMathMLmactionFrame()
{
  // unregister us as a mouse event listener ...
  //  printf("maction:%p unregistering as mouse event listener ...\n", this);
  if (mListener) {
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("click"), mListener,
                                        false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mouseover"), mListener,
                                        false);
    mContent->RemoveSystemEventListener(NS_LITERAL_STRING("mouseout"), mListener,
                                        false);
  }
}

NS_IMETHODIMP
nsMathMLmactionFrame::Init(nsIContent*      aContent,
                           nsIFrame*        aParent,
                           nsIFrame*        aPrevInFlow)
{
  // Init our local attributes

  mChildCount = -1; // these will be updated in GetSelectedFrame()
  mSelection = 0;
  mSelectedFrame = nsnull;
  mActionType = GetActionType(aContent);

  // Let the base class do the rest
  return nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsMathMLmactionFrame::TransmitAutomaticData() {
  // The REC defines the following element to be space-like:
  // * an maction element whose selected sub-expression exists and is
  //   space-like;
  nsIMathMLFrame* mathMLFrame = do_QueryFrame(mSelectedFrame);
  if (mathMLFrame && mathMLFrame->IsSpaceLike()) {
    mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
  } else {
    mPresentationData.flags &= ~NS_MATHML_SPACE_LIKE;
  }

  // The REC defines the following element to be an embellished operator:
  // * an maction element whose selected sub-expression exists and is an
  //   embellished operator;
  mPresentationData.baseFrame = mSelectedFrame;
  GetEmbellishDataFrom(mSelectedFrame, mEmbellishData);

  return NS_OK;
}

nsresult
nsMathMLmactionFrame::ChildListChanged(PRInt32 aModType)
{
  // update cached values
  mChildCount = -1;
  mSelection = 0;
  mSelectedFrame = nsnull;
  GetSelectedFrame();

  return nsMathMLContainerFrame::ChildListChanged(aModType);
}

// return the frame whose number is given by the attribute selection="number"
nsIFrame* 
nsMathMLmactionFrame::GetSelectedFrame()
{
  nsAutoString value;
  PRInt32 selection; 

  // selection is applied only to toggle, return first child otherwise
  if (NS_MATHML_ACTION_TYPE_TOGGLE != mActionType) {
    // We don't touch mChildCount here. It's incorrect to assign it 1,
    // and it's inefficient to count the children. It's fine to leave
    // it be equal -1 because it's not used with other actiontypes.
    mSelection = 1;
    mSelectedFrame = mFrames.FirstChild();
    return mSelectedFrame;
  }

  GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::selection_,
               value);
  if (!value.IsEmpty()) {
    PRInt32 errorCode;
    selection = value.ToInteger(&errorCode);
    if (NS_FAILED(errorCode)) 
      selection = 1;
  }
  else selection = 1; // default is first frame

  if (-1 != mChildCount) { // we have been in this function before...
    // cater for invalid user-supplied selection
    if (selection > mChildCount || selection < 1) 
      selection = 1;
    // quick return if it is identical with our cache
    if (selection == mSelection) 
      return mSelectedFrame;
  }

  // get the selected child and cache new values...
  PRInt32 count = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!mSelectedFrame) 
      mSelectedFrame = childFrame; // default is first child
    if (++count == selection) 
      mSelectedFrame = childFrame;

    childFrame = childFrame->GetNextSibling();
  }
  // cater for invalid user-supplied selection
  if (selection > count || selection < 1) 
    selection = 1;

  mChildCount = count;
  mSelection = selection;
  TransmitAutomaticData();

  return mSelectedFrame;
}

NS_IMETHODIMP
nsMathMLmactionFrame::SetInitialChildList(ChildListID     aListID,
                                          nsFrameList&    aChildList)
{
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);

  // This very first call to GetSelectedFrame() will cause us to be marked as an
  // embellished operator if the selected child is an embellished operator
  if (!GetSelectedFrame()) {
    mActionType = NS_MATHML_ACTION_TYPE_NONE;
  }
  else {
    // create mouse event listener and register it
    mListener = new nsMathMLmactionFrame::MouseListener(this);
    // printf("maction:%p registering as mouse event listener ...\n", this);
    mContent->AddSystemEventListener(NS_LITERAL_STRING("click"), mListener,
                                     false, false);
    mContent->AddSystemEventListener(NS_LITERAL_STRING("mouseover"), mListener,
                                     false, false);
    mContent->AddSystemEventListener(NS_LITERAL_STRING("mouseout"), mListener,
                                     false, false);
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLmactionFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                       nsIAtom* aAttribute,
                                       PRInt32  aModType)
{
  bool needsReflow = false;

  if (aAttribute == nsGkAtoms::actiontype_) {
    // updating mActionType ...
    PRInt32 oldActionType = mActionType;
    mActionType = GetActionType(mContent);

    // We have to initiate a reflow only when changing actiontype
    // from toggle or to toggle.
    if (oldActionType == NS_MATHML_ACTION_TYPE_TOGGLE || 
          mActionType == NS_MATHML_ACTION_TYPE_TOGGLE) {
      needsReflow = true;
    }
  } else if (aAttribute == nsGkAtoms::selection_) {
    // When the selection attribute is changed we have to initiate a reflow
    // only when actiontype is toggle.
    if (NS_MATHML_ACTION_TYPE_TOGGLE == mActionType) {
      needsReflow = true;
    }
  } else {
    // let the base class handle other attribute changes
    return 
      nsMathMLContainerFrame::AttributeChanged(aNameSpaceID, 
                                               aAttribute, aModType);
  }

  if (needsReflow) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange, NS_FRAME_IS_DIRTY);
  }

  return NS_OK;
}

//  Only paint the selected child...
NS_IMETHODIMP
nsMathMLmactionFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    // Put the child's background directly onto the content list
    nsDisplayListSet set(aLists, aLists.Content());
    // The children should be in content order
    rv = BuildDisplayListForChild(aBuilder, childFrame, aDirtyRect, set);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  // visual debug
  rv = DisplayBoundingMetrics(aBuilder, this, mReference, mBoundingMetrics, aLists);
#endif
  return rv;
}

// Only reflow the selected child ...
NS_IMETHODIMP
nsMathMLmactionFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  mBoundingMetrics = nsBoundingMetrics();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, aDesiredSize,
                     childReflowState, aStatus);
    SaveReflowAndBoundingMetricsFor(childFrame, aDesiredSize,
                                    aDesiredSize.mBoundingMetrics);
    mBoundingMetrics = aDesiredSize.mBoundingMetrics;
  }
  FinalizeReflow(*aReflowState.rendContext, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

// Only place the selected child ...
/* virtual */ nsresult
nsMathMLmactionFrame::Place(nsRenderingContext& aRenderingContext,
                            bool                 aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  mBoundingMetrics = nsBoundingMetrics();
  nsIFrame* childFrame = GetSelectedFrame();
  if (childFrame) {
    GetReflowAndBoundingMetricsFor(childFrame, aDesiredSize, mBoundingMetrics);
    if (aPlaceOrigin) {
      FinishReflowChild(childFrame, PresContext(), nsnull, aDesiredSize, 0, 0, 0);
    }
    mReference.x = 0;
    mReference.y = aDesiredSize.ascent;
  }
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}

// ################################################################
// Event handlers 
// ################################################################

NS_IMPL_ISUPPORTS1(nsMathMLmactionFrame::MouseListener,
                   nsIDOMEventListener)


// helper to show a msg on the status bar
// curled from nsObjectFrame.cpp ...
void
ShowStatus(nsPresContext* aPresContext, nsString& aStatusMsg)
{
  nsCOMPtr<nsISupports> cont = aPresContext->GetContainer();
  if (cont) {
    nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont));
    if (docShellItem) {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
      if (treeOwner) {
        nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
        if (browserChrome) {
          browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_LINK, aStatusMsg.get());
        }
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLmactionFrame::MouseListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("mouseover")) {
    mOwner->MouseOver();
  }
  else if (eventType.EqualsLiteral("click")) {
    mOwner->MouseClick();
  }
  else if (eventType.EqualsLiteral("mouseout")) {
    mOwner->MouseOut();
  }
  else {
    NS_ABORT();
  }

  return NS_OK;
}

void
nsMathMLmactionFrame::MouseOver()
{
  // see if we should display a status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    // retrieve content from a second child if it exists
    nsIFrame* childFrame = mFrames.FrameAt(1);
    if (!childFrame) return;

    nsIContent* content = childFrame->GetContent();
    if (!content) return;

    // check whether the content is mtext or not
    if (content->GetNameSpaceID() == kNameSpaceID_MathML &&
        content->Tag() == nsGkAtoms::mtext_) {
      // get the text to be displayed
      content = content->GetFirstChild();
      if (!content) return;

      const nsTextFragment* textFrg = content->GetText();
      if (!textFrg) return;

      nsAutoString text;
      textFrg->AppendTo(text);
      // collapse whitespaces as listed in REC, section 3.2.6.1
      text.CompressWhitespace();
      ShowStatus(PresContext(), text);
    }
  }
}

void
nsMathMLmactionFrame::MouseOut()
{
  // see if we should remove the status message
  if (NS_MATHML_ACTION_TYPE_STATUSLINE == mActionType) {
    nsAutoString value;
    value.SetLength(0);
    ShowStatus(PresContext(), value);
  }
}

void
nsMathMLmactionFrame::MouseClick()
{
  if (NS_MATHML_ACTION_TYPE_TOGGLE == mActionType) {
    if (mChildCount > 1) {
      PRInt32 selection = (mSelection == mChildCount)? 1 : mSelection + 1;
      nsAutoString value;
      char cbuf[10];
      PR_snprintf(cbuf, sizeof(cbuf), "%d", selection);
      value.AssignASCII(cbuf);
      bool notify = false; // don't yet notify the document
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::selection_, value, notify);

      // Now trigger a content-changed reflow...
      PresContext()->PresShell()->
        FrameNeedsReflow(mSelectedFrame, nsIPresShell::eTreeChange,
                         NS_FRAME_IS_DIRTY);
    }
  }
}
