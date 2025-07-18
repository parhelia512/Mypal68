/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");
const {
  EVALUATE_EXPRESSION,
  SET_TERMINAL_INPUT,
  SET_TERMINAL_EAGER_RESULT,
} = require("devtools/client/webconsole/constants");
const { getAllPrefs } = require("devtools/client/webconsole/selectors/prefs");

loader.lazyServiceGetter(
  this,
  "clipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);
loader.lazyRequireGetter(
  this,
  "saveScreenshot",
  "devtools/shared/screenshot/save"
);
loader.lazyRequireGetter(
  this,
  "messagesActions",
  "devtools/client/webconsole/actions/messages"
);
loader.lazyRequireGetter(
  this,
  "historyActions",
  "devtools/client/webconsole/actions/history"
);
loader.lazyRequireGetter(
  this,
  "ConsoleCommand",
  "devtools/client/webconsole/types",
  true
);
const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

async function getMappedExpression(hud, expression) {
  let mapResult;
  try {
    mapResult = await hud.getMappedExpression(expression);
  } catch (e) {
    console.warn("Error when calling getMappedExpression", e);
  }

  let mapped = null;
  if (mapResult) {
    ({ expression, mapped } = mapResult);
  }
  return { expression, mapped };
}

function evaluateExpression(expression) {
  return async ({ dispatch, webConsoleUI, hud }) => {
    if (!expression) {
      expression = hud.getInputSelection() || hud.getInputValue();
    }
    if (!expression) {
      return null;
    }

    // We use the messages action as it's doing additional transformation on the message.
    dispatch(
      messagesActions.messagesAdd([
        new ConsoleCommand({
          messageText: expression,
          timeStamp: Date.now(),
        }),
      ])
    );
    dispatch({
      type: EVALUATE_EXPRESSION,
      expression,
    });

    WebConsoleUtils.usageCount++;

    let mapped;
    ({ expression, mapped } = await getMappedExpression(hud, expression));

    const frameActorId = await webConsoleUI.getFrameActor();
    const webConsoleFront = await webConsoleUI.getWebConsoleFront({
      frameActorId,
    });

    // Even if the evaluation fails,
    // we still need to pass the error response to onExpressionEvaluated.
    const onSettled = res => res;

    const response = await webConsoleFront
      .evaluateJSAsync(expression, {
        frameActor: frameActorId,
        selectedNodeActor: webConsoleUI.getSelectedNodeActor(),
        mapped,
      })
      .then(onSettled, onSettled);

    return dispatch(onExpressionEvaluated(response));
  };
}

/**
 * The JavaScript evaluation response handler.
 *
 * @private
 * @param {Object} response
 *        The message received from the server.
 */
function onExpressionEvaluated(response) {
  return async ({ dispatch, webConsoleUI }) => {
    if (response.error) {
      console.error(`Evaluation error`, response.error, ": ", response.message);
      return;
    }

    // If the evaluation was a top-level await expression that was rejected, there will
    // be an uncaught exception reported, so we don't need to do anything.
    if (response.topLevelAwaitRejected === true) {
      return;
    }

    if (!response.helperResult) {
      dispatch(messagesActions.messagesAdd([response]));
      return;
    }

    await dispatch(handleHelperResult(response));
  };
}

function handleHelperResult(response) {
  return async ({ dispatch, hud, webConsoleUI }) => {
    const result = response.result;
    const helperResult = response.helperResult;
    const helperHasRawOutput = !!(helperResult || {}).rawOutput;

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          dispatch(messagesActions.messagesClear());
          break;
        case "clearHistory":
          dispatch(historyActions.clearHistory());
          break;
        case "inspectObject": {
          const objectActor = helperResult.object;
          if (hud.toolbox && !helperResult.forceExpandInConsole) {
            hud.toolbox.inspectObjectActor(objectActor);
          } else {
            webConsoleUI.inspectObjectActor(objectActor);
          }
          break;
        }
        case "help":
          hud.openLink(HELP_URL);
          break;
        case "copyValueToClipboard":
          clipboardHelper.copyString(helperResult.value);
          break;
        case "screenshotOutput":
          const { args, value } = helperResult;
          const screenshotMessages = await saveScreenshot(
            webConsoleUI.getPanelWindow(),
            args,
            value
          );
          dispatch(
            messagesActions.messagesAdd(
              screenshotMessages.map(message => ({
                message,
                type: "logMessage",
              }))
            )
          );
          // early return as we already dispatched necessary messages.
          return;
      }
    }

    const hasErrorMessage =
      response.exceptionMessage ||
      (helperResult && helperResult.type === "error");

    // Hide undefined results coming from helper functions.
    const hasUndefinedResult =
      result && typeof result == "object" && result.type == "undefined";

    if (hasErrorMessage || helperHasRawOutput || !hasUndefinedResult) {
      dispatch(messagesActions.messagesAdd([response]));
    }
  };
}

function focusInput() {
  return ({ hud }) => {
    return hud.focusInput();
  };
}

function setInputValue(value) {
  return ({ hud }) => {
    return hud.setInputValue(value);
  };
}

function terminalInputChanged(expression) {
  return async ({ dispatch, webConsoleUI, hud, client, getState }) => {
    const prefs = getAllPrefs(getState());
    if (!prefs.eagerEvaluation) {
      return;
    }

    const { terminalInput = "" } = getState().history;
    // Only re-evaluate if the expression did change.
    if (
      (!terminalInput && !expression) ||
      (typeof terminalInput === "string" &&
        typeof expression === "string" &&
        expression.trim() === terminalInput.trim())
    ) {
      return;
    }

    dispatch({
      type: SET_TERMINAL_INPUT,
      expression: expression.trim(),
    });

    // There's no need to evaluate an empty string.
    if (!expression.trim()) {
      return;
    }

    let mapped;
    ({ expression, mapped } = await getMappedExpression(hud, expression));

    const frameActorId = await webConsoleUI.getFrameActor();
    const webConsoleFront = await webConsoleUI.getWebConsoleFront({
      frameActorId,
    });

    const response = await client.evaluateJSAsync(expression, {
      frameActor: frameActorId,
      selectedNodeFront: webConsoleUI.getSelectedNodeFront(),
      webConsoleFront,
      mapped,
      eager: true,
    });

    // eslint-disable-next-line consistent-return
    return dispatch({
      type: SET_TERMINAL_EAGER_RESULT,
      result: getEagerEvaluationResult(response),
    });
  };
}

function getEagerEvaluationResult(response) {
  const result = response.exception || response.result;
  // Don't show syntax errors results to the user.
  if (
    (result && result.isSyntaxError) ||
    (result && result.type == "undefined")
  ) {
    return null;
  }

  return result;
}

module.exports = {
  evaluateExpression,
  focusInput,
  setInputValue,
  terminalInputChanged,
};
