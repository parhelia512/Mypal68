/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  getAllFilters,
} = require("devtools/client/webconsole/selectors/filters");
const {
  getFilteredMessagesCount,
} = require("devtools/client/webconsole/selectors/messages");
const { getAllPrefs } = require("devtools/client/webconsole/selectors/prefs");
const { getAllUi } = require("devtools/client/webconsole/selectors/ui");
const actions = require("devtools/client/webconsole/actions/index");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const { PluralForm } = require("devtools/shared/plural-form");
const {
  FILTERS,
  FILTERBAR_DISPLAY_MODES,
} = require("devtools/client/webconsole/constants");

const FilterButton = require("devtools/client/webconsole/components/FilterBar/FilterButton");
const FilterCheckbox = require("devtools/client/webconsole/components/FilterBar/FilterCheckbox");
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);

const disabledCssFilterButtonTitle = l10n.getStr(
  "webconsole.cssFilterButton.inactive.tooltip"
);

class FilterBar extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      filter: PropTypes.object.isRequired,
      persistLogs: PropTypes.bool.isRequired,
      hidePersistLogsCheckbox: PropTypes.bool.isRequired,
      eagerEvaluation: PropTypes.bool.isRequired,
      showContentMessages: PropTypes.bool.isRequired,
      hideShowContentMessagesCheckbox: PropTypes.bool.isRequired,
      filteredMessagesCount: PropTypes.object.isRequired,
      closeButtonVisible: PropTypes.bool,
      closeSplitConsole: PropTypes.func,
      displayMode: PropTypes.oneOf([...Object.values(FILTERBAR_DISPLAY_MODES)])
        .isRequired,
      autocomplete: PropTypes.bool.isRequired,
    };
  }

  static get defaultProps() {
    return {
      hidePersistLogsCheckbox: false,
      hideShowContentMessagesCheckbox: true,
    };
  }

  constructor(props) {
    super(props);

    this.onClickMessagesClear = this.onClickMessagesClear.bind(this);
    this.onSearchBoxChange = this.onSearchBoxChange.bind(this);
    this.onChangePersistToggle = this.onChangePersistToggle.bind(this);
    this.onChangeShowContent = this.onChangeShowContent.bind(this);
    this.renderFiltersConfigBar = this.renderFiltersConfigBar.bind(this);

    this.maybeUpdateLayout = this.maybeUpdateLayout.bind(this);
    this.resizeObserver = new ResizeObserver(this.maybeUpdateLayout);
  }

  componentDidMount() {
    this.filterInputMinWidth = 150;
    try {
      const filterInput = this.wrapperNode.querySelector(".devtools-searchbox");
      this.filterInputMinWidth = Number(
        window.getComputedStyle(filterInput)["min-width"].replace("px", "")
      );
    } catch (e) {
      // If the min-width of the filter input isn't set, or is set in a different unit
      // than px.
      console.error("min-width of the filter input couldn't be retrieved.", e);
    }

    this.maybeUpdateLayout();
    this.resizeObserver.observe(this.wrapperNode);
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      filter,
      persistLogs,
      showContentMessages,
      filteredMessagesCount,
      closeButtonVisible,
      displayMode,
      eagerEvaluation,
      autocomplete,
    } = this.props;

    if (
      nextProps.closeButtonVisible !== closeButtonVisible ||
      nextProps.displayMode !== displayMode ||
      nextProps.filter !== filter ||
      nextProps.persistLogs !== persistLogs ||
      nextProps.showContentMessages !== showContentMessages ||
      nextProps.eagerEvaluation !== eagerEvaluation ||
      nextProps.autocomplete !== autocomplete
    ) {
      return true;
    }

    if (
      JSON.stringify(nextProps.filteredMessagesCount) !==
      JSON.stringify(filteredMessagesCount)
    ) {
      return true;
    }

    return false;
  }

  /**
   * Update the boolean state that informs where the filter buttons should be rendered.
   * If the filter buttons are rendered inline with the filter input and the filter
   * input width is reduced below a threshold, the filter buttons are rendered on a new
   * row. When the filter buttons are on a separate row and the filter input grows
   * wide enough to display the filter buttons without dropping below the threshold,
   * the filter buttons are rendered inline.
   */
  maybeUpdateLayout() {
    const { dispatch, displayMode } = this.props;

    // If we don't have the wrapperNode reference, or if the wrapperNode isn't connected
    // anymore, we disconnect the resize observer (componentWillUnmount is never called
    // on this component, so we have to do it here).
    if (!this.wrapperNode || !this.wrapperNode.isConnected) {
      this.resizeObserver.disconnect();
      return;
    }

    const filterInput = this.wrapperNode.querySelector(".devtools-searchbox");
    const { width: filterInputWidth } = filterInput.getBoundingClientRect();

    if (displayMode === FILTERBAR_DISPLAY_MODES.WIDE) {
      if (filterInputWidth <= this.filterInputMinWidth) {
        dispatch(
          actions.filterBarDisplayModeSet(FILTERBAR_DISPLAY_MODES.NARROW)
        );
      }

      return;
    }

    if (displayMode === FILTERBAR_DISPLAY_MODES.NARROW) {
      const filterButtonsToolbar = this.wrapperNode.querySelector(
        ".webconsole-filterbar-secondary"
      );

      const buttonMargin = 5;
      const filterButtonsToolbarWidth = Array.from(
        filterButtonsToolbar.children
      ).reduce(
        (width, el) => width + el.getBoundingClientRect().width + buttonMargin,
        0
      );

      if (
        filterInputWidth - this.filterInputMinWidth >
        filterButtonsToolbarWidth
      ) {
        dispatch(actions.filterBarDisplayModeSet(FILTERBAR_DISPLAY_MODES.WIDE));
      }
    }
  }

  onClickMessagesClear() {
    this.props.dispatch(actions.messagesClear());
  }

  onSearchBoxChange(text) {
    this.props.dispatch(actions.filterTextSet(text));
  }

  onChangePersistToggle() {
    this.props.dispatch(actions.persistToggle());
  }

  onChangeShowContent() {
    this.props.dispatch(actions.contentMessagesToggle());
  }

  renderFiltersConfigBar() {
    const { dispatch, filter, filteredMessagesCount } = this.props;

    const getLabel = (baseLabel, filterKey) => {
      const count = filteredMessagesCount[filterKey];
      if (filter[filterKey] || count === 0) {
        return baseLabel;
      }
      return `${baseLabel} (${count})`;
    };

    return dom.div(
      {
        className: "devtools-toolbar webconsole-filterbar-secondary",
        key: "config-bar",
      },
      FilterButton({
        active: filter[FILTERS.ERROR],
        label: getLabel(
          l10n.getStr("webconsole.errorsFilterButton.label"),
          FILTERS.ERROR
        ),
        filterKey: FILTERS.ERROR,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.WARN],
        label: getLabel(
          l10n.getStr("webconsole.warningsFilterButton.label"),
          FILTERS.WARN
        ),
        filterKey: FILTERS.WARN,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.LOG],
        label: getLabel(
          l10n.getStr("webconsole.logsFilterButton.label"),
          FILTERS.LOG
        ),
        filterKey: FILTERS.LOG,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.INFO],
        label: getLabel(
          l10n.getStr("webconsole.infoFilterButton.label"),
          FILTERS.INFO
        ),
        filterKey: FILTERS.INFO,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.DEBUG],
        label: getLabel(
          l10n.getStr("webconsole.debugFilterButton.label"),
          FILTERS.DEBUG
        ),
        filterKey: FILTERS.DEBUG,
        dispatch,
      }),
      dom.div({
        className: "devtools-separator",
      }),
      FilterButton({
        active: filter[FILTERS.CSS],
        title: filter[FILTERS.CSS] ? undefined : disabledCssFilterButtonTitle,
        label: l10n.getStr("webconsole.cssFilterButton.label"),
        filterKey: FILTERS.CSS,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.NETXHR],
        label: l10n.getStr("webconsole.xhrFilterButton.label"),
        filterKey: FILTERS.NETXHR,
        dispatch,
      }),
      FilterButton({
        active: filter[FILTERS.NET],
        label: l10n.getStr("webconsole.requestsFilterButton.label"),
        filterKey: FILTERS.NET,
        dispatch,
      })
    );
  }

  render() {
    const {
      persistLogs,
      hidePersistLogsCheckbox,
      hideShowContentMessagesCheckbox,
      closeSplitConsole,
      displayMode,
      showContentMessages,
      filteredMessagesCount,
    } = this.props;

    const isNarrow = displayMode === FILTERBAR_DISPLAY_MODES.NARROW;
    const isWide = displayMode === FILTERBAR_DISPLAY_MODES.WIDE;

    let searchBoxSummary;
    let searchBoxSummaryTooltip;
    if (filteredMessagesCount.text > 0) {
      searchBoxSummary = l10n.getStr("webconsole.filteredMessagesByText.label");
      searchBoxSummary = PluralForm.get(
        filteredMessagesCount.text,
        searchBoxSummary
      ).replace("#1", filteredMessagesCount.text);

      searchBoxSummaryTooltip = l10n.getStr(
        "webconsole.filteredMessagesByText.tooltip"
      );
      searchBoxSummaryTooltip = PluralForm.get(
        filteredMessagesCount.text,
        searchBoxSummaryTooltip
      ).replace("#1", filteredMessagesCount.text);
    }

    const children = [
      dom.div(
        {
          className:
            "devtools-toolbar devtools-input-toolbar webconsole-filterbar-primary",
          key: "primary-bar",
        },
        dom.button({
          className: "devtools-button devtools-clear-icon",
          title: l10n.getStr("webconsole.clearButton.tooltip"),
          onClick: this.onClickMessagesClear,
        }),
        dom.div({
          className: "devtools-separator",
        }),
        SearchBox({
          type: "filter",
          placeholder: l10n.getStr("webconsole.filterInput.placeholder"),
          keyShortcut: l10n.getStr("webconsole.find.key"),
          onChange: this.onSearchBoxChange,
          summary: searchBoxSummary,
          summaryTooltip: searchBoxSummaryTooltip,
        }),
        isWide &&
          dom.div({
            className: "devtools-separator",
          }),
        isWide && this.renderFiltersConfigBar(),
        !(hidePersistLogsCheckbox && hideShowContentMessagesCheckbox)
          ? dom.div({
              className: "devtools-separator",
            })
          : null,
        !hidePersistLogsCheckbox &&
          FilterCheckbox({
            label: l10n.getStr("webconsole.enablePersistentLogs.label"),
            title: l10n.getStr("webconsole.enablePersistentLogs.tooltip"),
            onChange: this.onChangePersistToggle,
            checked: persistLogs,
          }),
        !hideShowContentMessagesCheckbox &&
          FilterCheckbox({
            label: l10n.getStr("browserconsole.contentMessagesCheckbox.label"),
            title: l10n.getStr(
              "browserconsole.contentMessagesCheckbox.tooltip"
            ),
            onChange: this.onChangeShowContent,
            checked: showContentMessages,
          })
      ),
    ];

    if (this.props.closeButtonVisible) {
      children.push(
        dom.div(
          {
            className: "devtools-toolbar split-console-close-button-wrapper",
            key: "wrapper",
          },
          dom.button({
            id: "split-console-close-button",
            key: "split-console-close-button",
            className: "devtools-button",
            title: l10n.getStr("webconsole.closeSplitConsoleButton.tooltip"),
            onClick: () => {
              closeSplitConsole();
            },
          })
        )
      );
    }

    if (isNarrow) {
      children.push(this.renderFiltersConfigBar());
    }

    return dom.div(
      {
        className: `webconsole-filteringbar-wrapper ${displayMode}`,
        "aria-live": "off",
        ref: node => {
          this.wrapperNode = node;
        },
      },
      children
    );
  }
}

function mapStateToProps(state) {
  const uiState = getAllUi(state);
  const prefsState = getAllPrefs(state);
  return {
    filter: getAllFilters(state),
    persistLogs: uiState.persistLogs,
    eagerEvaluation: prefsState.eagerEvaluation,
    showContentMessages: uiState.showContentMessages,
    filteredMessagesCount: getFilteredMessagesCount(state),
    closeButtonVisible: uiState.closeButtonVisible,
    autocomplete: prefsState.autocomplete,
  };
}

module.exports = connect(mapStateToProps)(FilterBar);
