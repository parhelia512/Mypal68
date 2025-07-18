/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  hasInScopeLines,
  getSourceWithContent,
  getVisibleSelectedFrame,
} from "../../selectors";

import { getSourceLineCount } from "../../utils/source";

import { range, flatMap, uniq, without } from "lodash";
import { isFulfilled } from "../../utils/async-value";

import type { AstLocation } from "../../workers/parser";
import type { ThunkArgs } from "../types";
import type { Context, SourceLocation } from "../../types";

function getOutOfScopeLines(outOfScopeLocations: ?(AstLocation[])) {
  if (!outOfScopeLocations) {
    return null;
  }

  return uniq(
    flatMap(outOfScopeLocations, location =>
      range(location.start.line, location.end.line)
    )
  );
}

async function getInScopeLines(
  cx: Context,
  location: SourceLocation,
  { dispatch, getState, parser }: ThunkArgs
) {
  const source = getSourceWithContent(getState(), location.sourceId);

  let locations = null;
  if (location.line && source && !source.isWasm) {
    locations = await parser.findOutOfScopeLocations(
      source.id,
      ((location: any): parser.AstPosition)
    );
  }

  const linesOutOfScope = getOutOfScopeLines(locations);
  const sourceNumLines =
    !source.content || !isFulfilled(source.content)
      ? 0
      : getSourceLineCount(source.content.value);

  const sourceLines = range(1, sourceNumLines + 1);

  return !linesOutOfScope
    ? sourceLines
    : without(sourceLines, ...linesOutOfScope);
}

export function setInScopeLines(cx: Context) {
  return async (thunkArgs: ThunkArgs) => {
    const { getState, dispatch } = thunkArgs;
    const visibleFrame = getVisibleSelectedFrame(getState());

    if (!visibleFrame) {
      return;
    }

    const { location } = visibleFrame;
    if (hasInScopeLines(getState(), location)) {
      return;
    }

    const lines = await getInScopeLines(cx, location, thunkArgs);

    dispatch({
      type: "IN_SCOPE_LINES",
      cx,
      location,
      lines,
    });
  };
}
