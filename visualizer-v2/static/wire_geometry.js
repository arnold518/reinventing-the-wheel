"use strict";

(function exportWireGeometry(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.CircuitWireGeometry = api;
}(typeof globalThis !== "undefined" ? globalThis : this, () => {
  const EPSILON = 1e-9;

  function samePoint(left, right) {
    return Math.abs(left.x - right.x) <= EPSILON
      && Math.abs(left.y - right.y) <= EPSILON;
  }

  function dedupePath(path) {
    const result = [];
    for (const point of path) {
      if (result.length === 0 || !samePoint(point, result[result.length - 1])) {
        result.push({ x: point.x, y: point.y });
      }
    }
    return result;
  }

  function horizontalDirection(pinPoint, boundaryPoint) {
    const dx = boundaryPoint.x - pinPoint.x;
    if (Math.abs(dx) <= EPSILON) return 0;
    return dx > 0 ? 1 : -1;
  }

  function routeForward(start, end) {
    const dx = end.x - start.x;
    if (dx > 10) return dedupePath([start, { x: end.x, y: start.y }, end]);
    const midX = (start.x + end.x) / 2;
    return dedupePath([start, { x: midX, y: start.y }, { x: midX, y: end.y }, end]);
  }

  function turnaroundChannelY(start, end, clearance, bounds) {
    if (Math.abs(start.y - end.y) > EPSILON) return (start.y + end.y) / 2;

    let direction = -1;
    if (bounds && Number.isFinite(bounds.y) && Number.isFinite(bounds.h)) {
      const roomAbove = start.y - bounds.y;
      const roomBelow = bounds.y + bounds.h - start.y;
      direction = roomBelow > roomAbove ? 1 : -1;
    }
    return start.y + direction * clearance;
  }

  // The pin-to-boundary stubs have a physical direction. A connection that
  // must travel back across that direction first continues outward, turns 90
  // degrees, and only then crosses to the other endpoint. This prevents the
  // route from drawing back over its own source or sink stub.
  function routeBetweenHorizontalStubs({
    sourcePinPoint,
    sourceBoundaryPoint,
    sinkPinPoint,
    sinkBoundaryPoint,
    bounds = null,
  }) {
    const sourceDirection = horizontalDirection(sourcePinPoint, sourceBoundaryPoint);
    const sinkDirection = horizontalDirection(sinkPinPoint, sinkBoundaryPoint);
    const dx = sinkBoundaryPoint.x - sourceBoundaryPoint.x;
    const flowDirection = Math.abs(dx) <= EPSILON ? 0 : (dx > 0 ? 1 : -1);

    if (sourceDirection === 0 || sinkDirection === 0 || flowDirection === 0) {
      return routeForward(sourceBoundaryPoint, sinkBoundaryPoint);
    }

    const sourceNeedsTurn = flowDirection !== sourceDirection;
    const sinkNeedsTurn = flowDirection === sinkDirection;
    if (!sourceNeedsTurn && !sinkNeedsTurn) {
      return routeForward(sourceBoundaryPoint, sinkBoundaryPoint);
    }

    const sourceStubLength = Math.abs(sourceBoundaryPoint.x - sourcePinPoint.x);
    const sinkStubLength = Math.abs(sinkBoundaryPoint.x - sinkPinPoint.x);
    const clearance = Math.max(sourceStubLength, sinkStubLength, EPSILON) * 1.5;
    const sourceEscape = sourceNeedsTurn
      ? {
        x: sourceBoundaryPoint.x + sourceDirection * clearance,
        y: sourceBoundaryPoint.y,
      }
      : sourceBoundaryPoint;
    const sinkEscape = sinkNeedsTurn
      ? {
        x: sinkBoundaryPoint.x + sinkDirection * clearance,
        y: sinkBoundaryPoint.y,
      }
      : sinkBoundaryPoint;
    const channelY = turnaroundChannelY(sourceEscape, sinkEscape, clearance, bounds);

    return dedupePath([
      sourceBoundaryPoint,
      sourceEscape,
      { x: sourceEscape.x, y: channelY },
      { x: sinkEscape.x, y: channelY },
      sinkEscape,
      sinkBoundaryPoint,
    ]);
  }

  return {
    dedupePath,
    routeBetweenHorizontalStubs,
  };
}));
