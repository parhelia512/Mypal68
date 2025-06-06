/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutReader"];

const { ReaderMode } = ChromeUtils.import(
  "resource://gre/modules/ReaderMode.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AsyncPrefs",
  "resource://gre/modules/AsyncPrefs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NarrateControls",
  "resource://gre/modules/narrate/NarrateControls.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);

var gStrings = Services.strings.createBundle(
  "chrome://global/locale/aboutReader.properties"
);

const zoomOnCtrl =
  Services.prefs.getIntPref("mousewheel.with_control.action", 3) == 3;
const zoomOnMeta =
  Services.prefs.getIntPref("mousewheel.with_meta.action", 1) == 3;

var AboutReader = function(mm, win, articlePromise) {
  let url = this._getOriginalUrl(win);
  if (
    !(
      url.startsWith("http://") ||
      url.startsWith("https://") ||
      url.startsWith("file://")
    )
  ) {
    let errorMsg =
      "Only http://, https:// and file:// URLs can be loaded in about:reader.";
    if (Services.prefs.getBoolPref("reader.errors.includeURLs")) {
      errorMsg += " Tried to load: " + url + ".";
    }
    Cu.reportError(errorMsg);
    win.location.href = "about:blank";
    return;
  }

  let doc = win.document;

  this._mm = mm;
  this._mm.addMessageListener("Reader:CloseDropdown", this);
  this._mm.addMessageListener("Reader:AddButton", this);
  this._mm.addMessageListener("Reader:RemoveButton", this);
  this._mm.addMessageListener("Reader:GetStoredArticleData", this);
  this._mm.addMessageListener("Reader:ZoomIn", this);
  this._mm.addMessageListener("Reader:ZoomOut", this);
  this._mm.addMessageListener("Reader:ResetZoom", this);

  this._docRef = Cu.getWeakReference(doc);
  this._winRef = Cu.getWeakReference(win);
  this._innerWindowId = win.windowUtils.currentInnerWindowID;

  this._article = null;
  this._languagePromise = new Promise(resolve => {
    this._foundLanguage = resolve;
  });

  if (articlePromise) {
    this._articlePromise = articlePromise;
  }

  this._headerElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-header")
  );
  this._domainElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-domain")
  );
  this._titleElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-title")
  );
  this._readTimeElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-estimated-time")
  );
  this._creditsElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-credits")
  );
  this._contentElementRef = Cu.getWeakReference(
    doc.querySelector(".moz-reader-content")
  );
  this._toolbarElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-toolbar")
  );
  this._messageElementRef = Cu.getWeakReference(
    doc.querySelector(".reader-message")
  );
  this._containerElementRef = Cu.getWeakReference(
    doc.querySelector(".container")
  );

  this._scrollOffset = win.pageYOffset;

  doc.addEventListener("mousedown", this);
  doc.addEventListener("click", this);
  doc.addEventListener("touchstart", this);

  win.addEventListener("pagehide", this);
  win.addEventListener("mozvisualscroll", this, { mozSystemGroup: true });
  win.addEventListener("resize", this);
  win.addEventListener("wheel", this, { passive: false });

  Services.obs.addObserver(this, "inner-window-destroyed");

  this._setupStyleDropdown();
  this._setupButton(
    "close-button",
    this._onReaderClose.bind(this),
    "aboutReader.toolbar.close"
  );

  // we're ready for any external setup, send a signal for that.
  this._mm.sendAsyncMessage("Reader:OnSetup");

  let colorSchemeValues = JSON.parse(
    Services.prefs.getCharPref("reader.color_scheme.values")
  );
  let colorSchemeOptions = colorSchemeValues.map(value => {
    return {
      name: gStrings.GetStringFromName("aboutReader.colorScheme." + value),
      value,
      itemClass: value + "-button",
    };
  });

  let colorScheme = Services.prefs.getCharPref("reader.color_scheme");
  this._setupSegmentedButton(
    "color-scheme-buttons",
    colorSchemeOptions,
    colorScheme,
    this._setColorSchemePref.bind(this)
  );
  this._setColorSchemePref(colorScheme);

  let fontTypeSample = gStrings.GetStringFromName("aboutReader.fontTypeSample");
  let fontTypeOptions = [
    {
      name: fontTypeSample,
      description: gStrings.GetStringFromName(
        "aboutReader.fontType.sans-serif"
      ),
      value: "sans-serif",
      itemClass: "sans-serif-button",
    },
    {
      name: fontTypeSample,
      description: gStrings.GetStringFromName("aboutReader.fontType.serif"),
      value: "serif",
      itemClass: "serif-button",
    },
  ];

  let fontType = Services.prefs.getCharPref("reader.font_type");
  this._setupSegmentedButton(
    "font-type-buttons",
    fontTypeOptions,
    fontType,
    this._setFontType.bind(this)
  );
  this._setFontType(fontType);

  this._setupFontSizeButtons();

  this._setupContentWidthButtons();

  this._setupLineHeightButtons();

  if (win.speechSynthesis && Services.prefs.getBoolPref("narrate.enabled")) {
    new NarrateControls(mm, win, this._languagePromise);
  }

  this._loadArticle();

  let dropdown = this._toolbarElement;

  let elemL10nMap = {
    ".minus-button": "minus",
    ".plus-button": "plus",
    ".content-width-minus-button": "contentwidthminus",
    ".content-width-plus-button": "contentwidthplus",
    ".line-height-minus-button": "lineheightminus",
    ".line-height-plus-button": "lineheightplus",
    ".light-button": "colorschemelight",
    ".dark-button": "colorschemedark",
    ".sepia-button": "colorschemesepia",
  };

  for (let [selector, stringID] of Object.entries(elemL10nMap)) {
    dropdown
      .querySelector(selector)
      .setAttribute(
        "title",
        gStrings.GetStringFromName("aboutReader.toolbar." + stringID)
      );
  }
};

AboutReader.prototype = {
  _BLOCK_IMAGES_SELECTOR:
    ".content p > img:only-child, " +
    ".content p > a:only-child > img:only-child, " +
    ".content .wp-caption img, " +
    ".content figure img",

  FONT_SIZE_MIN: 1,

  FONT_SIZE_LEGACY_MAX: 9,

  FONT_SIZE_MAX: 15,

  FONT_SIZE_EXTENDED_VALUES: [32, 40, 56, 72, 96, 128],

  get _doc() {
    return this._docRef.get();
  },

  get _win() {
    return this._winRef.get();
  },

  get _headerElement() {
    return this._headerElementRef.get();
  },

  get _domainElement() {
    return this._domainElementRef.get();
  },

  get _titleElement() {
    return this._titleElementRef.get();
  },

  get _readTimeElement() {
    return this._readTimeElementRef.get();
  },

  get _creditsElement() {
    return this._creditsElementRef.get();
  },

  get _contentElement() {
    return this._contentElementRef.get();
  },

  get _toolbarElement() {
    return this._toolbarElementRef.get();
  },

  get _messageElement() {
    return this._messageElementRef.get();
  },

  get _containerElement() {
    return this._containerElementRef.get();
  },

  get _isToolbarVertical() {
    if (this._toolbarVertical !== undefined) {
      return this._toolbarVertical;
    }
    return (this._toolbarVertical = Services.prefs.getBoolPref(
      "reader.toolbar.vertical"
    ));
  },

  // Provides unique view Id.
  get viewId() {
    let _viewId = Cc["@mozilla.org/uuid-generator;1"]
      .getService(Ci.nsIUUIDGenerator)
      .generateUUID()
      .toString();
    Object.defineProperty(this, "viewId", { value: _viewId });

    return _viewId;
  },

  receiveMessage(message) {
    switch (message.name) {
      // Triggered by Android user pressing BACK while the banner font-dropdown is open.
      case "Reader:CloseDropdown": {
        // Just close it.
        this._closeDropdowns();
        break;
      }

      case "Reader:AddButton": {
        if (
          message.data.id &&
          message.data.image &&
          !this._doc.getElementsByClassName(message.data.id)[0]
        ) {
          let btn = this._doc.createElement("button");
          btn.dataset.buttonid = message.data.id;
          btn.className = "button " + message.data.id;
          btn.style.backgroundImage = "url('" + message.data.image + "')";
          if (message.data.title) {
            btn.title = message.data.title;
          }
          if (message.data.text) {
            btn.textContent = message.data.text;
          }
          if (message.data.width && message.data.height) {
            btn.style.backgroundSize = `${message.data.width}px ${
              message.data.height
            }px`;
          }
          let tb = this._toolbarElement;
          tb.appendChild(btn);
          this._setupButton(message.data.id, button => {
            this._mm.sendAsyncMessage(
              "Reader:Clicked-" + button.dataset.buttonid,
              { article: this._article }
            );
          });
        }
        break;
      }
      case "Reader:RemoveButton": {
        if (message.data.id) {
          let btn = this._doc.getElementsByClassName(message.data.id)[0];
          if (btn) {
            btn.remove();
          }
        }
        break;
      }
      case "Reader:GetStoredArticleData": {
        this._mm.sendAsyncMessage("Reader:StoredArticleData", {
          article: this._article,
        });
        break;
      }
      case "Reader:ZoomIn": {
        this._changeFontSize(+1);
        break;
      }
      case "Reader:ZoomOut": {
        this._changeFontSize(-1);
        break;
      }
      case "Reader:ResetZoom": {
        this._resetFontSize();
        break;
      }
    }
  },

  handleEvent(aEvent) {
    if (!aEvent.isTrusted) {
      return;
    }

    let target = aEvent.target;
    switch (aEvent.type) {
      case "touchstart":
      /* fall through */
      case "mousedown":
        if (!target.closest(".dropdown-popup")) {
          this._closeDropdowns();
        }
        break;
      case "click":
        if (target.classList.contains("dropdown-toggle")) {
          this._toggleDropdownClicked(aEvent);
        }
        break;
      case "mozvisualscroll":
        const vv = aEvent.originalTarget; // VisualViewport

        if (gIsFirefoxDesktop) {
          this._closeDropdowns(true);
        } else if (this._scrollOffset != vv.pageTop) {
          // hide the system UI and the "reader-toolbar" only if the dropdown is not opened
          let selector = ".dropdown.open";
          let openDropdowns = this._doc.querySelectorAll(selector);
          if (openDropdowns.length) {
            break;
          }

          let isScrollingUp = this._scrollOffset > vv.pageTop;
          this._setSystemUIVisibility(isScrollingUp);
          this._setToolbarVisibility(isScrollingUp);
        }

        this._scrollOffset = vv.pageTop;
        break;
      case "resize":
        this._updateImageMargins();
        if (this._isToolbarVertical) {
          this._win.setTimeout(() => {
            for (let dropdown of this._doc.querySelectorAll(".dropdown.open")) {
              this._updatePopupPosition(dropdown);
            }
          }, 0);
        }
        break;

      case "wheel":
        let doZoom =
          (aEvent.ctrlKey && zoomOnCtrl) || (aEvent.metaKey && zoomOnMeta);
        if (!doZoom) {
          return;
        }
        aEvent.preventDefault();

        // Throttle events to once per 150ms. This avoids excessively fast zooming.
        if (aEvent.timeStamp <= this._zoomBackoffTime) {
          return;
        }
        this._zoomBackoffTime = aEvent.timeStamp + 150;

        // Determine the direction of the delta (we don't care about its size);
        // This code is adapted from normalizeWheelEventDelta in
        // toolkt/components/pdfjs/content/web/viewer.js
        let delta = Math.abs(aEvent.deltaX) + Math.abs(aEvent.deltaY);
        let angle = Math.atan2(aEvent.deltaY, aEvent.deltaX);
        if (-0.25 * Math.PI < angle && angle < 0.75 * Math.PI) {
          delta = -delta;
        }

        if (delta > 0) {
          this._changeFontSize(+1);
        } else if (delta < 0) {
          this._changeFontSize(-1);
        }
        break;

      case "pagehide":
        // Close the Banners Font-dropdown, cleanup Android BackPressListener.
        this._closeDropdowns();

        this._mm.removeMessageListener("Reader:CloseDropdown", this);
        this._mm.removeMessageListener("Reader:AddButton", this);
        this._mm.removeMessageListener("Reader:RemoveButton", this);
        this._mm.removeMessageListener("Reader:GetStoredArticleData", this);
        this._mm.removeMessageListener("Reader:ZoomIn", this);
        this._mm.removeMessageListener("Reader:ZoomOut", this);
        this._mm.removeMessageListener("Reader:ResetZoom", this);
        this._windowUnloaded = true;
        break;
    }
  },

  observe(subject, topic, data) {
    if (
      subject.QueryInterface(Ci.nsISupportsPRUint64).data != this._innerWindowId
    ) {
      return;
    }

    Services.obs.removeObserver(this, "inner-window-destroyed");

    this._mm.removeMessageListener("Reader:CloseDropdown", this);
    this._mm.removeMessageListener("Reader:AddButton", this);
    this._mm.removeMessageListener("Reader:RemoveButton", this);
    this._windowUnloaded = true;
  },

  _onReaderClose() {
    ReaderMode.leaveReaderMode(this._mm.docShell, this._win);
  },

  async _resetFontSize() {
    await AsyncPrefs.reset("reader.font_size");
    let currentSize = Services.prefs.getIntPref("reader.font_size");
    this._setFontSize(currentSize);
  },

  _setFontSize(newFontSize) {
    this._fontSize = Math.min(
      this.FONT_SIZE_MAX,
      Math.max(this.FONT_SIZE_MIN, newFontSize)
    );
    let size;
    if (this._fontSize > this.FONT_SIZE_LEGACY_MAX) {
      // -1 because we're indexing into a 0-indexed array, so the first value
      // over the legacy max should be 0, the next 1, etc.
      let index = this._fontSize - this.FONT_SIZE_LEGACY_MAX - 1;
      size = this.FONT_SIZE_EXTENDED_VALUES[index];
    } else {
      size = 10 + 2 * this._fontSize;
    }

    this._containerElement.style.setProperty("--font-size", size + "px");
    return AsyncPrefs.set("reader.font_size", this._fontSize);
  },

  _setupFontSizeButtons() {
    let plusButton = this._doc.querySelector(".plus-button");
    let minusButton = this._doc.querySelector(".minus-button");

    let currentSize = Services.prefs.getIntPref("reader.font_size");
    this._setFontSize(currentSize);
    this._updateFontSizeButtonControls();

    plusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();
        this._changeFontSize(+1);
      },
      true
    );

    minusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();
        this._changeFontSize(-1);
      },
      true
    );
  },

  _updateFontSizeButtonControls() {
    let plusButton = this._doc.querySelector(".plus-button");
    let minusButton = this._doc.querySelector(".minus-button");

    let currentSize = Services.prefs.getIntPref("reader.font_size");

    if (currentSize === this.FONT_SIZE_MIN) {
      minusButton.setAttribute("disabled", true);
    } else {
      minusButton.removeAttribute("disabled");
    }
    if (currentSize === this.FONT_SIZE_MAX) {
      plusButton.setAttribute("disabled", true);
    } else {
      plusButton.removeAttribute("disabled");
    }
  },

  _changeFontSize(changeAmount) {
    let currentSize =
      Services.prefs.getIntPref("reader.font_size") + changeAmount;
    this._setFontSize(currentSize);
    this._updateFontSizeButtonControls();
  },

  _setContentWidth(newContentWidth) {
    let containerClasses = this._containerElement.classList;

    if (this._contentWidth > 0) {
      containerClasses.remove("content-width" + this._contentWidth);
    }

    this._contentWidth = newContentWidth;
    containerClasses.add("content-width" + this._contentWidth);
    return AsyncPrefs.set("reader.content_width", this._contentWidth);
  },

  _setupContentWidthButtons() {
    const CONTENT_WIDTH_MIN = 1;
    const CONTENT_WIDTH_MAX = 9;

    let currentContentWidth = Services.prefs.getIntPref("reader.content_width");
    currentContentWidth = Math.max(
      CONTENT_WIDTH_MIN,
      Math.min(CONTENT_WIDTH_MAX, currentContentWidth)
    );

    let plusButton = this._doc.querySelector(".content-width-plus-button");
    let minusButton = this._doc.querySelector(".content-width-minus-button");

    function updateControls() {
      if (currentContentWidth === CONTENT_WIDTH_MIN) {
        minusButton.setAttribute("disabled", true);
      } else {
        minusButton.removeAttribute("disabled");
      }
      if (currentContentWidth === CONTENT_WIDTH_MAX) {
        plusButton.setAttribute("disabled", true);
      } else {
        plusButton.removeAttribute("disabled");
      }
    }

    updateControls();
    this._setContentWidth(currentContentWidth);

    plusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentContentWidth >= CONTENT_WIDTH_MAX) {
          return;
        }

        currentContentWidth++;
        updateControls();
        this._setContentWidth(currentContentWidth);
      },
      true
    );

    minusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentContentWidth <= CONTENT_WIDTH_MIN) {
          return;
        }

        currentContentWidth--;
        updateControls();
        this._setContentWidth(currentContentWidth);
      },
      true
    );
  },

  _setLineHeight(newLineHeight) {
    let contentClasses = this._contentElement.classList;

    if (this._lineHeight > 0) {
      contentClasses.remove("line-height" + this._lineHeight);
    }

    this._lineHeight = newLineHeight;
    contentClasses.add("line-height" + this._lineHeight);
    return AsyncPrefs.set("reader.line_height", this._lineHeight);
  },

  _setupLineHeightButtons() {
    const LINE_HEIGHT_MIN = 1;
    const LINE_HEIGHT_MAX = 9;

    let currentLineHeight = Services.prefs.getIntPref("reader.line_height");
    currentLineHeight = Math.max(
      LINE_HEIGHT_MIN,
      Math.min(LINE_HEIGHT_MAX, currentLineHeight)
    );

    let plusButton = this._doc.querySelector(".line-height-plus-button");
    let minusButton = this._doc.querySelector(".line-height-minus-button");

    function updateControls() {
      if (currentLineHeight === LINE_HEIGHT_MIN) {
        minusButton.setAttribute("disabled", true);
      } else {
        minusButton.removeAttribute("disabled");
      }
      if (currentLineHeight === LINE_HEIGHT_MAX) {
        plusButton.setAttribute("disabled", true);
      } else {
        plusButton.removeAttribute("disabled");
      }
    }

    updateControls();
    this._setLineHeight(currentLineHeight);

    plusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentLineHeight >= LINE_HEIGHT_MAX) {
          return;
        }

        currentLineHeight++;
        updateControls();
        this._setLineHeight(currentLineHeight);
      },
      true
    );

    minusButton.addEventListener(
      "click",
      event => {
        if (!event.isTrusted) {
          return;
        }
        event.stopPropagation();

        if (currentLineHeight <= LINE_HEIGHT_MIN) {
          return;
        }

        currentLineHeight--;
        updateControls();
        this._setLineHeight(currentLineHeight);
      },
      true
    );
  },

  _setColorScheme(newColorScheme) {
    // "auto" is not a real color scheme
    if (this._colorScheme === newColorScheme || newColorScheme === "auto") {
      return;
    }

    let bodyClasses = this._doc.body.classList;

    if (this._colorScheme) {
      bodyClasses.remove(this._colorScheme);
    }

    this._colorScheme = newColorScheme;
    bodyClasses.add(this._colorScheme);
  },

  // Pref values include "dark", "light", and "sepia"
  _setColorSchemePref(colorSchemePref) {
    this._setColorScheme(colorSchemePref);

    AsyncPrefs.set("reader.color_scheme", colorSchemePref);
  },

  _setFontType(newFontType) {
    if (this._fontType === newFontType) {
      return;
    }

    let bodyClasses = this._doc.body.classList;

    if (this._fontType) {
      bodyClasses.remove(this._fontType);
    }

    this._fontType = newFontType;
    bodyClasses.add(this._fontType);

    AsyncPrefs.set("reader.font_type", this._fontType);
  },

  _setSystemUIVisibility(visible) {
    this._mm.sendAsyncMessage("Reader:SystemUIVisibility", { visible });
  },

  _setToolbarVisibility(visible) {
    let tb = this._toolbarElement;

    if (visible) {
      if (tb.style.opacity != "1") {
        tb.removeAttribute("hidden");
        tb.style.opacity = "1";
      }
    } else if (tb.style.opacity != "0") {
      tb.addEventListener(
        "transitionend",
        evt => {
          if (tb.style.opacity == "0") {
            tb.setAttribute("hidden", "");
          }
        },
        { once: true }
      );
      tb.style.opacity = "0";
    }
  },

  async _loadArticle() {
    let url = this._getOriginalUrl();
    this._showProgressDelayed();

    let article;
    if (this._articlePromise) {
      article = await this._articlePromise;
    } else {
      try {
        article = await this._getArticle(url);
      } catch (e) {
        if (e && e.newURL) {
          let readerURL = "about:reader?url=" + encodeURIComponent(e.newURL);
          this._win.location.replace(readerURL);
          return;
        }
      }
    }

    if (this._windowUnloaded) {
      return;
    }

    // Replace the loading message with an error message if there's a failure.
    // Users are supposed to navigate away by themselves (because we cannot
    // remove ourselves from session history.)
    if (!article) {
      this._showError();
      return;
    }

    this._showContent(article);
  },

  _getArticle(url) {
    if (this.PLATFORM_HAS_CACHE) {
      return new Promise((resolve, reject) => {
        let listener = message => {
          this._mm.removeMessageListener("Reader:ArticleData", listener);
          if (message.data.newURL) {
            reject({ newURL: message.data.newURL });
            return;
          }
          resolve(message.data.article);
        };
        this._mm.addMessageListener("Reader:ArticleData", listener);
        this._mm.sendAsyncMessage("Reader:ArticleGet", { url });
      });
    }
    return ReaderMode.downloadAndParseDocument(url);
  },

  _requestFavicon() {
    let handleFaviconReturn = message => {
      this._mm.removeMessageListener(
        "Reader:FaviconReturn",
        handleFaviconReturn
      );
      this._loadFavicon(message.data.url, message.data.faviconUrl);
    };

    this._mm.addMessageListener("Reader:FaviconReturn", handleFaviconReturn);
    this._mm.sendAsyncMessage("Reader:FaviconRequest", {
      url: this._article.url,
      preferredWidth: 16 * this._win.devicePixelRatio,
    });
  },

  _loadFavicon(url, faviconUrl) {
    if (this._article.url !== url) {
      return;
    }

    let doc = this._doc;

    let link = doc.createElement("link");
    link.rel = "shortcut icon";
    link.href = faviconUrl;

    doc.getElementsByTagName("head")[0].appendChild(link);
  },

  _updateImageMargins() {
    let windowWidth = this._win.innerWidth;
    let bodyWidth = this._doc.body.clientWidth;

    let setImageMargins = function(img) {
      // If the image is at least as wide as the window, make it fill edge-to-edge on mobile.
      if (img.naturalWidth >= windowWidth) {
        img.setAttribute("moz-reader-full-width", true);
      } else {
        img.removeAttribute("moz-reader-full-width");
      }

      // If the image is at least half as wide as the body, center it on desktop.
      if (img.naturalWidth >= bodyWidth / 2) {
        img.setAttribute("moz-reader-center", true);
      } else {
        img.removeAttribute("moz-reader-center");
      }
    };

    let imgs = this._doc.querySelectorAll(this._BLOCK_IMAGES_SELECTOR);
    for (let i = imgs.length; --i >= 0; ) {
      let img = imgs[i];

      if (img.naturalWidth > 0) {
        setImageMargins(img);
      } else {
        img.onload = function() {
          setImageMargins(img);
        };
      }
    }
  },

  _maybeSetTextDirection: function Read_maybeSetTextDirection(article) {
    if (article.dir) {
      // Set "dir" attribute on content
      this._contentElement.setAttribute("dir", article.dir);
      this._headerElement.setAttribute("dir", article.dir);

      // The native locale could be set differently than the article's text direction.
      var localeDirection = Services.locale.isAppLocaleRTL ? "rtl" : "ltr";
      this._readTimeElement.setAttribute("dir", localeDirection);
      this._readTimeElement.style.textAlign =
        article.dir == "rtl" ? "right" : "left";
    }
  },

  _formatReadTime(slowEstimate, fastEstimate) {
    let displayStringKey = "aboutReader.estimatedReadTimeRange1";

    // only show one reading estimate when they are the same value
    if (slowEstimate == fastEstimate) {
      displayStringKey = "aboutReader.estimatedReadTimeValue1";
    }

    return PluralForm.get(
      slowEstimate,
      gStrings.GetStringFromName(displayStringKey)
    )
      .replace("#1", fastEstimate)
      .replace("#2", slowEstimate);
  },

  _showError() {
    this._headerElement.classList.remove("reader-show-element");
    this._contentElement.classList.remove("reader-show-element");

    let errorMessage = gStrings.GetStringFromName("aboutReader.loadError");
    this._messageElement.textContent = errorMessage;
    this._messageElement.style.display = "block";

    this._doc.title = errorMessage;

    this._doc.documentElement.dataset.isError = true;

    this._error = true;

    this._doc.dispatchEvent(
      new this._win.CustomEvent("AboutReaderContentError", {
        bubbles: true,
        cancelable: false,
      })
    );
  },

  // This function is the JS version of Java's StringUtils.stripCommonSubdomains.
  _stripHost(host) {
    if (!host) {
      return host;
    }

    let start = 0;

    if (host.startsWith("www.")) {
      start = 4;
    } else if (host.startsWith("m.")) {
      start = 2;
    } else if (host.startsWith("mobile.")) {
      start = 7;
    }

    return host.substring(start);
  },

  _showContent(article) {
    this._messageElement.classList.remove("reader-show-element");

    this._article = article;

    this._domainElement.href = article.url;
    let articleUri = Services.io.newURI(article.url);
    this._domainElement.textContent = this._stripHost(articleUri.host);
    this._creditsElement.textContent = article.byline;

    this._titleElement.textContent = article.title;
    this._readTimeElement.textContent = this._formatReadTime(
      article.readingTimeMinsSlow,
      article.readingTimeMinsFast
    );
    this._doc.title = article.title;

    this._headerElement.classList.add("reader-show-element");

    let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(
      Ci.nsIParserUtils
    );
    let contentFragment = parserUtils.parseFragment(
      article.content,
      Ci.nsIParserUtils.SanitizerDropForms |
        Ci.nsIParserUtils.SanitizerAllowStyle,
      false,
      articleUri,
      this._contentElement
    );
    this._contentElement.innerHTML = "";
    this._contentElement.appendChild(contentFragment);
    this._maybeSetTextDirection(article);
    this._foundLanguage(article.language);

    this._contentElement.classList.add("reader-show-element");
    this._updateImageMargins();

    this._requestFavicon();
    this._doc.body.classList.add("loaded");

    this._goToReference(articleUri.ref);

    Services.obs.notifyObservers(this._win, "AboutReader:Ready");

    this._doc.dispatchEvent(
      new this._win.CustomEvent("AboutReaderContentReady", {
        bubbles: true,
        cancelable: false,
      })
    );
  },

  _hideContent() {
    this._headerElement.classList.remove("reader-show-element");
    this._contentElement.classList.remove("reader-show-element");
  },

  _showProgressDelayed() {
    this._win.setTimeout(() => {
      // No need to show progress if the article has been loaded,
      // if the window has been unloaded, or if there was an error
      // trying to load the article.
      if (this._article || this._windowUnloaded || this._error) {
        return;
      }

      this._headerElement.classList.remove("reader-show-element");
      this._contentElement.classList.remove("reader-show-element");

      this._messageElement.textContent = gStrings.GetStringFromName(
        "aboutReader.loading2"
      );
      this._messageElement.classList.add("reader-show-element");
    }, 300);
  },

  /**
   * Returns the original article URL for this about:reader view.
   */
  _getOriginalUrl(win) {
    let url = win ? win.location.href : this._win.location.href;
    return ReaderMode.getOriginalUrl(url) || url;
  },

  _setupSegmentedButton(id, options, initialValue, callback) {
    let doc = this._doc;
    let segmentedButton = doc.getElementsByClassName(id)[0];

    for (let i = 0; i < options.length; i++) {
      let option = options[i];

      let item = doc.createElement("button");

      // Put the name in a div so that Android can hide it.
      let div = doc.createElement("div");
      div.textContent = option.name;
      div.classList.add("name");
      item.appendChild(div);

      if (option.itemClass !== undefined) {
        item.classList.add(option.itemClass);
      }

      if (option.description !== undefined) {
        let description = doc.createElement("div");
        description.textContent = option.description;
        description.classList.add("description");
        item.appendChild(description);
      }

      segmentedButton.appendChild(item);

      item.addEventListener(
        "click",
        function(aEvent) {
          if (!aEvent.isTrusted) {
            return;
          }

          aEvent.stopPropagation();

          let items = segmentedButton.children;
          for (let j = items.length - 1; j >= 0; j--) {
            items[j].classList.remove("selected");
          }

          item.classList.add("selected");
          callback(option.value);
        },
        true
      );

      if (option.value === initialValue) {
        item.classList.add("selected");
      }
    }
  },

  _setupButton(id, callback, titleEntity, textEntity) {
    if (titleEntity) {
      this._setButtonTip(id, titleEntity);
    }

    let button = this._doc.getElementsByClassName(id)[0];
    if (textEntity) {
      button.textContent = gStrings.GetStringFromName(textEntity);
    }
    button.removeAttribute("hidden");
    button.addEventListener(
      "click",
      function(aEvent) {
        if (!aEvent.isTrusted) {
          return;
        }

        let btn = aEvent.target;
        callback(btn);
      },
      true
    );
  },

  /**
   * Sets a toolTip for a button. Performed at initial button setup
   * and dynamically as button state changes.
   * @param   Localizable string providing UI element usage tip.
   */
  _setButtonTip(id, titleEntity) {
    let button = this._doc.getElementsByClassName(id)[0];
    button.setAttribute("title", gStrings.GetStringFromName(titleEntity));
  },

  _setupStyleDropdown() {
    let dropdownToggle = this._doc.querySelector(
      ".style-dropdown .dropdown-toggle"
    );
    dropdownToggle.setAttribute(
      "title",
      gStrings.GetStringFromName("aboutReader.toolbar.typeControls")
    );
  },

  _updatePopupPosition(dropdown) {
    let dropdownToggle = dropdown.querySelector(".dropdown-toggle");
    let dropdownPopup = dropdown.querySelector(".dropdown-popup");

    let toggleHeight = dropdownToggle.offsetHeight;
    let toggleTop = dropdownToggle.offsetTop;
    let popupTop = toggleTop - toggleHeight / 2;

    dropdownPopup.style.top = popupTop + "px";
  },

  _toggleDropdownClicked(event) {
    let dropdown = event.target.closest(".dropdown");

    if (!dropdown) {
      return;
    }

    event.stopPropagation();

    if (dropdown.classList.contains("open")) {
      this._closeDropdowns();
    } else {
      this._openDropdown(dropdown);
      if (this._isToolbarVertical) {
        this._updatePopupPosition(dropdown);
      }
    }
  },

  /*
   * If the ReaderView banner font-dropdown is closed, open it.
   */
  _openDropdown(dropdown) {
    if (dropdown.classList.contains("open")) {
      return;
    }

    this._closeDropdowns();

    // Trigger BackPressListener initialization in Android.
    dropdown.classList.add("open");
    this._mm.sendAsyncMessage("Reader:DropdownOpened", this.viewId);
  },

  /*
   * If the ReaderView has open dropdowns, close them. If we are closing the
   * dropdowns because the page is scrolling, allow popups to stay open with
   * the keep-open class.
   */
  _closeDropdowns(scrolling) {
    let selector = ".dropdown.open";
    if (scrolling) {
      selector += ":not(.keep-open)";
    }

    let openDropdowns = this._doc.querySelectorAll(selector);
    for (let dropdown of openDropdowns) {
      dropdown.classList.remove("open");
    }

    // Trigger BackPressListener cleanup in Android.
    if (openDropdowns.length) {
      this._mm.sendAsyncMessage("Reader:DropdownClosed", this.viewId);
    }
  },

  /*
   * Scroll reader view to a reference
   */
  _goToReference(ref) {
    if (ref) {
      this._win.location.hash = ref;
    }
  },
};
