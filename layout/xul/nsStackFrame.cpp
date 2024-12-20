/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsStackFrame.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/PresShell.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsCSSRendering.h"
#include "nsBoxLayoutState.h"
#include "nsStackLayout.h"
#include "nsDisplayList.h"

using namespace mozilla;

nsIFrame* NS_NewStackFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsStackFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsStackFrame)

nsStackFrame::nsStackFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsBoxFrame(aStyle, aPresContext, kClassID) {
  nsCOMPtr<nsBoxLayout> layout;
  NS_NewStackLayout(layout);
  SetXULLayoutManager(layout);
}

// REVIEW: The old code put everything in the background layer. To be more
// consistent with the way other frames work, I'm putting everything in the
// Content() (i.e., foreground) layer (see nsIFrame::BuildDisplayListForChild,
// the case for stacking context but non-positioned, non-floating frames).
// This could easily be changed back by hacking
// nsBoxFrame::BuildDisplayListInternal a bit more.
void nsStackFrame::BuildDisplayListForChildren(nsDisplayListBuilder* aBuilder,
                                               const nsDisplayListSet& aLists) {
  // BuildDisplayListForChild puts stacking contexts into the
  // PositionedDescendants list. So we need to map that list to
  // aLists.Content(). This is an easy way to do that.
  nsDisplayList* content = aLists.Content();
  nsDisplayListSet kidLists(content, content, content, content, content,
                            content);
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    // Force each child into its own true stacking context.
    BuildDisplayListForChild(aBuilder, kid, kidLists,
                             DisplayChildFlag::ForceStackingContext);
    kid = kid->GetNextSibling();
  }
}
