/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

/**
 * Main entry point for the rendering of the Viewer component.
 * @param {Event} evt File select event.
 */
function handleFileSelectEvent(evt) {
  var file = evt.target.files[0];
  var fileReader = new FileReader();

  fileReader.onload = (function(theFile) {
    return function(e) {
      var dataSet = createViewerDataset(e.target.result);
      var viewer = new Viewer({ownerDocument:window.document});
      viewer.id = 'csvContentsDataTable';
      viewer.dataSet = dataSet;
      viewer.barChartElementId = 'barChart';
      viewer.render();
    };
  })(file);
  fileReader.readAsText(file);
}

/**
 * Splits the csv file contents, and creates a square array of strings for
 * each token in the line.
 * @param fileContents
 * @return {String[][]} The data set with the contents of the csv file.
 */
function createViewerDataset(fileContents) {
  var rows = fileContents.split('\n');
  var dataset = [];
  rows.forEach(function (row, index, array) {
    if (row.trim().length == 0) {
      //Ignore empty lines
      return;
    }
    var newRow = [];
    var columns = row.split(',');
    columns.forEach(function (element, index, array) {
      newRow.push(element);
    });
    dataset.push(newRow);
  });
  return dataset;
}