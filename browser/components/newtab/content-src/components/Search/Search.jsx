/* globals ContentSearchUIController */
"use strict";

import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {connect} from "react-redux";
import {IS_NEWTAB} from "content-src/lib/constants";
import React from "react";

export class _Search extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onSearchClick = this.onSearchClick.bind(this);
    this.onSearchHandoffClick = this.onSearchHandoffClick.bind(this);
    this.onSearchHandoffPaste = this.onSearchHandoffPaste.bind(this);
    this.onSearchHandoffDrop = this.onSearchHandoffDrop.bind(this);
    this.onInputMount = this.onInputMount.bind(this);
    this.onSearchHandoffButtonMount = this.onSearchHandoffButtonMount.bind(this);
  }

  onSearchClick(event) {
    window.gContentSearchController.search(event);
  }

  doSearchHandoff(text) {
    this.props.dispatch(ac.OnlyToMain({type: at.HANDOFF_SEARCH_TO_AWESOMEBAR, data: {text}}));
    this.props.dispatch({type: at.FAKE_FOCUS_SEARCH});
    this.props.dispatch(ac.UserEvent({event: "SEARCH_HANDOFF"}));
    if (text) {
      this.props.dispatch({type: at.HIDE_SEARCH});
    }
  }

  onSearchHandoffClick(event) {
    // When search hand-off is enabled, we render a big button that is styled to
    // look like a search textbox. If the button is clicked, we style
    // the button as if it was a focused search box and show a fake cursor but
    // really focus the awesomebar without the focus styles ("hidden focus").
    event.preventDefault();
    this.doSearchHandoff();
  }

  onSearchHandoffPaste(event) {
    event.preventDefault();
    this.doSearchHandoff(event.clipboardData.getData("Text"));
  }

  onSearchHandoffDrop(event) {
    event.preventDefault();
    let text = event.dataTransfer.getData("text");
    if (text) {
      this.doSearchHandoff(text);
    }
  }

  componentWillUnmount() {
    delete window.gContentSearchController;
  }

  onInputMount(input) {
    if (input) {
      // The "healthReportKey" and needs to be "newtab" or "abouthome" so that
      // BrowserUsageTelemetry.jsm knows to handle events with this name, and
      // can add the appropriate telemetry probes for search. Without the correct
      // name, certain tests like browser_UsageTelemetry_content.js will fail
      // (See github ticket #2348 for more details)
      const healthReportKey = IS_NEWTAB ? "newtab" : "abouthome";

      // The "searchSource" needs to be "newtab" or "homepage" and is sent with
      // the search data and acts as context for the search request (See
      // nsISearchEngine.getSubmission). It is necessary so that search engine
      // plugins can correctly atribute referrals. (See github ticket #3321 for
      // more details)
      const searchSource = IS_NEWTAB ? "newtab" : "homepage";

      // gContentSearchController needs to exist as a global so that tests for
      // the existing about:home can find it; and so it allows these tests to pass.
      // In the future, when activity stream is default about:home, this can be renamed
      window.gContentSearchController = new ContentSearchUIController(input, input.parentNode,
        healthReportKey, searchSource);
      addEventListener("ContentSearchClient", this);
    } else {
      window.gContentSearchController = null;
      removeEventListener("ContentSearchClient", this);
    }
  }

  onSearchHandoffButtonMount(button) {
    // Keep a reference to the button for use during "paste" event handling.
    this._searchHandoffButton = button;
  }

  /*
   * Do not change the ID on the input field, as legacy newtab code
   * specifically looks for the id 'newtab-search-text' on input fields
   * in order to execute searches in various tests
   */
  render() {
    const wrapperClassName = [
      "search-wrapper",
      this.props.hide && "search-hidden",
      this.props.fakeFocus && "fake-focus",
    ].filter(v => v).join(" ");

    return (<div className={wrapperClassName}>
      {this.props.showLogo &&
        <div className="logo-and-wordmark">
          <div className="logo" />
          <div className="wordmark" />
        </div>
      }
      {!this.props.handoffEnabled &&
      <div className="search-inner-wrapper">
        <input
          id="newtab-search-text"
          data-l10n-id="newtab-search-box-search-the-web-input"
          maxLength="256"
          ref={this.onInputMount}
          type="search" />
        <button
          id="searchSubmit"
          className="search-button"
          data-l10n-id="newtab-search-box-search-button"
          onClick={this.onSearchClick} />
      </div>
      }
      {this.props.handoffEnabled &&
        <div className="search-inner-wrapper">
          <button
            className="search-handoff-button"
            data-l10n-id="newtab-search-box-search-the-web-input"
            ref={this.onSearchHandoffButtonMount}
            onClick={this.onSearchHandoffClick}
            tabIndex="-1">
            <div className="fake-textbox" data-l10n-id="newtab-search-box-search-the-web-text" />
            <input type="search" className="fake-editable" tabIndex="-1" aria-hidden="true" onDrop={this.onSearchHandoffDrop} onPaste={this.onSearchHandoffPaste} />
            <div className="fake-caret" />
          </button>
          {/*
            This dummy and hidden input below is so we can load ContentSearchUIController.
            Why? It sets --newtab-search-icon for us and it isn't trivial to port over.
          */}
          <input
            type="search"
            style={{display: "none"}}
            ref={this.onInputMount} />
        </div>
      }
    </div>);
  }
}

export const Search = connect()(_Search);
