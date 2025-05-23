// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

var EXPORTED_SYMBOLS = ["FindBarContent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

/* Please keep in sync with toolkit/content/widgets/findbar.js */
const FIND_NORMAL = 0;
const FIND_TYPEAHEAD = 1;
const FIND_LINKS = 2;

class FindBarContent {
  constructor(mm) {
    this.mm = mm;

    this.findMode = 0;
    this.inQuickFind = false;

    this.mm.addMessageListener("Findbar:UpdateState", this);

    Services.els.addSystemEventListener(this.mm, "mouseup", this, false);
  }

  start(event) {
    this.inPassThrough = true;
  }

  startQuickFind(event, autostart = false) {
    let mode = FIND_TYPEAHEAD;
    if (
      event.charCode == "'".charAt(0) ||
      (autostart && FindBarContent.typeAheadLinksOnly)
    ) {
      mode = FIND_LINKS;
    }

    // Set findMode immediately (without waiting for child->parent->child roundtrip)
    // to ensure we pass any further keypresses, too.
    this.findMode = mode;
    this.passKeyToParent(event);
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "Findbar:UpdateState":
        this.findMode = msg.data.findMode;
        this.inQuickFind = msg.data.hasQuickFindTimeout;
        if (msg.data.isOpenAndFocused) {
          this.inPassThrough = false;
        }
        break;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "keypress":
        this.onKeypress(event);
        break;
      case "mouseup":
        this.onMouseup(event);
        break;
    }
  }

  onKeypress(event) {
    if (this.inPassThrough) {
      this.passKeyToParent(event);
    } else if (
      this.findMode != FIND_NORMAL &&
      this.inQuickFind &&
      event.charCode
    ) {
      this.passKeyToParent(event);
    }
  }

  passKeyToParent(event) {
    event.preventDefault();
    // These are the properties required to dispatch another 'real' event
    // to the findbar in the parent in _dispatchKeypressEvent in findbar.xml .
    // If you make changes here, verify that that method can still do its job.
    const kRequiredProps = [
      "type",
      "bubbles",
      "cancelable",
      "ctrlKey",
      "altKey",
      "shiftKey",
      "metaKey",
      "keyCode",
      "charCode",
    ];
    let fakeEvent = {};
    for (let prop of kRequiredProps) {
      fakeEvent[prop] = event[prop];
    }
    this.mm.sendAsyncMessage("Findbar:Keypress", fakeEvent);
  }

  onMouseup(event) {
    if (this.findMode != FIND_NORMAL) {
      this.mm.sendAsyncMessage("Findbar:Mouseup");
    }
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  FindBarContent,
  "typeAheadLinksOnly",
  "accessibility.typeaheadfind.linksonly"
);
