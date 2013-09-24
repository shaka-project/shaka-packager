// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides data access interface for dump file profiler
 * @constructor
 */
var Profiler = function(jsonData) {
  this._jsonData = jsonData;
};

/**
 * Get units of a snapshot in a world.
 * Exception will be thrown when no world of given name exists.
 * @param  {string} worldName
 * @param  {number} snapshotIndex
 * @return {Object.<string, number>}
 */
Profiler.prototype.getUnits = function(worldName, snapshotIndex) {
  var snapshot = this._jsonData.snapshots[snapshotIndex];
  if (!snapshot.worlds[worldName])
    throw 'no world ' + worldName + ' in snapshot ' + index;

  // Return units.
  var world = snapshot.worlds[worldName];
  var units = {};
  for (var unitName in world.units)
    units[unitName] = world.units[unitName][0];
  return units;
};

/**
 * Get first-level breakdowns of a snapshot in a world.
 * Exception will be thrown when no world of given name exists.
 * @param  {string} worldName
 * @param  {number} snapshotIndex
 * @return {Object.<string, Object>}
 */
Profiler.prototype.getBreakdowns = function(worldName, snapshotIndex) {
  var snapshot = this._jsonData.snapshots[snapshotIndex];
  if (!snapshot.worlds[worldName])
    throw 'no world ' + worldName + ' in snapshot ' + index;

  // Return breakdowns.
  // TODO(junjianx): handle breakdown with arbitrary-level structure.
  return snapshot.worlds[worldName].breakdown;
};

/**
 * Get categories from fixed hard-coded worlds and breakdowns temporarily.
 * TODO(junjianx): remove the hard-code and support general cases.
 * @return {Array.<Object>}
 */
Profiler.prototype.getCategories = function() {
  var categories = [];
  var snapshotNum = this._jsonData.snapshots.length;

  for (var snapshotIndex = 0; snapshotIndex < snapshotNum; ++snapshotIndex) {
    // Initial categories object for one snapshot.
    categories.push({});

    // Handle breakdowns in malloc world.
    var mallocBreakdown = this.getBreakdowns('malloc', snapshotIndex);
    var mallocUnits = this.getUnits('malloc', snapshotIndex);
    if (!mallocBreakdown['component'])
      throw 'no breakdown ' + 'component' + ' in snapshot ' + snapshotIndex;

    var componentBreakdown = mallocBreakdown['component'];
    var componentMemory = 0;
    Object.keys(componentBreakdown).forEach(function(breakdownName) {
      var breakdown = componentBreakdown[breakdownName];
      var memory = breakdown.units.reduce(function(previous, current) {
        return previous + mallocUnits[current];
      }, 0);
      componentMemory += memory;

      if (componentBreakdown['hidden'] === true)
        return;
      else
        categories[snapshotIndex][breakdownName] = memory;
    });

    // Handle breakdowns in vm world.
    var vmBreakdown = this.getBreakdowns('vm', snapshotIndex);
    var vmUnits = this.getUnits('vm', snapshotIndex);
    if (!vmBreakdown['map'])
      throw 'no breakdown ' + 'map' + ' in snapshot ' + snapshotIndex;

    var mapBreakdown = vmBreakdown['map'];

    Object.keys(mapBreakdown).forEach(function(breakdownName) {
      var breakdown = mapBreakdown[breakdownName];
      var memory = breakdown.units.reduce(function(previous, current) {
        return previous + vmUnits[current];
      }, 0);

      if (vmBreakdown['hidden'] === true)
        return;
      else if (breakdownName === 'mmap-tcmalloc')
        categories[snapshotIndex]['tc-unused'] = memory - componentMemory;
      else
        categories[snapshotIndex][breakdownName] = memory;
    });
  }

  return categories;
};

/**
 * Generate lines for flot plotting.
 * @param  {Array.<Object>} categories
 * @return {Array.<Object>}
 */
var generateLines = function(categories) {
  var lines = {};
  var snapshotNum = categories.length;

  // Initialize lines with all zero.
  categories.forEach(function(categories) {
    Object.keys(categories).forEach(function(breakdownName) {
      if (lines[breakdownName])
        return;
      lines[breakdownName] = [];
      for (var i = 0; i < snapshotNum; ++i)
        lines[breakdownName].push([i, 0]);
    });
  });

  // Assignment lines with values of categories.
  categories.forEach(function(categories, index) {
    Object.keys(categories).forEach(function(breakdownName) {
      lines[breakdownName][index] = [index, categories[breakdownName]];
    });
  });

  return Object.keys(lines).map(function(breakdownName) {
    return {
      label: breakdownName,
      data: lines[breakdownName]
    };
  });
};

$(function() {
  // Read original data and plot.
  $.getJSON('data/result.json', function(jsonData) {
    var profiler = new Profiler(jsonData);
    var categories = profiler.getCategories();
    var lines = generateLines(categories);

    // Plot stack graph.
    $.plot('#plot', lines, {
      series: {
        stack: true,
        lines: { show: true, fill: true }
      }
    });
  });
});