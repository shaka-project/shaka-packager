/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
'use strict';

(function() {
  /**
   * Main results viewer component.
   */
  var Viewer = ui.define('div');
  (function() {
    //"Private" functions for Viewer
    /**
     * Determines the appropriate parser for a given column, using it's first
     * data element.
     * @param {String} firstElement The first (non-header) element of a given
     * column.
     * @return {String} The YUI parser needed for the column.
     */
    function getColumnParser(firstElement) {
      if (isNumeric(firstElement)) {
        return 'number';
      } else if (isDate(firstElement)) {
        return 'date';
      } else {
        return 'string';
      }
    }

    /**
     * Determines whether or not the given element is a date.
     * @param {String} str The string representation of a potential date.
     * @return {boolean} true/false whether or not str can be parsed to
     * a date.
     */
    function isDate(str) {
      var timestamp = Date.parse(str);
      return !isNaN(timestamp);
    }

    /**
     * Generates the YUI column definition for the given dataset.
     * @param {String[][]} dataSet the dataset that will be displayed.
     * @return {JSON[]} The array containing the column definitions.
     */
    function createYUIColumnDefinitions(dataSet) {
      var header = dataSet[0];
      var firstRow = dataSet[1];
      var columnDefinitions = [];
      header.forEach(function (element, index, array) {
        columnDefinitions.push({
          'key' : index.toString(),
          'label':element.toString(),
          'maxAutoWidth':95,
          'sortable':true,
          'parser':getColumnParser(firstRow[index])});
      });
      return columnDefinitions;
    }

    /**
     * Generates the YUI data source for the given dataset.
     * @param {String[][]} dataSet the dataset that will be displayed.
     * @return {YAHOO.util.FunctionDataSource} The YUI data source
     * derived from the dataset.
     */
    function createYUIDataSource(dataSet) {
      var dataSource = [];
      //Starts from the first non-header line.
      for (var i = 1; i < dataSet.length; i++) {
        var dataSourceLine = {};
        dataSet[i].forEach(function (element, index, array) {
          if (isNumeric(element)) {
            dataSourceLine[index.toString()] = parseFloat(element);
          } else {
            dataSourceLine[index.toString()] = element
          }
        });
        dataSource.push(dataSourceLine);
      }
      return new YAHOO.util.FunctionDataSource(function() {
                                               return dataSource});
    }

    /**
     * Un-selects all the columns from the given data table.
     * @param {YAHOO.widget.DataTable} dataTable The data table that
     * contains the results.
     */
    function unselectAllColumns(dataTable) {
      var selectedColumns = dataTable.getSelectedColumns();
      for (var i = 0; i < selectedColumns.length; i++) {
        dataTable.unselectColumn(selectedColumns[i]);
      }
    }

    /**
     * Generates an array that contains the indices of the selected
     * columns in the data table.
     * @param {YAHOO.widget.DataTable} dataTable
     * @return {int[]} An array with the indices of the selected columns.
     */
    function getSelectedColumnIndices(dataTable) {
      var selectedColumnIndices = [];
      var selectedColumns = dataTable.getSelectedColumns();
      for (var i = 0; i < selectedColumns.length; i++) {
        selectedColumnIndices.push(selectedColumns[i].key);
      }
      return selectedColumnIndices;
    }

    Viewer.prototype = {
      __proto__: HTMLDivElement.prototype,
      decorate:function() {
        /**
         * The id for the element that contains the barchart (Optional).
         * @type {String}
         */
        this.barChartElementId_ = undefined;
        /**
         * The rectangular array that contains the contents of the cvs file.
         * @type {String[][]}
         */
        this.dataSet_ = undefined;
      },
      set barChartElementId(e) {
        this.barChartElementId_ = e;
      },
      get barChartElementId() {
        return this.barChartElementId_;
      },
      set dataSet(ds) {
        this.dataSet_ = ds;
      },
      get dataSet() {
        return this.dataSet_;
      },
      /**
       * Renders the Viewer component.
       * @expose
       */
      render: function() {
        document.body.appendChild(this);
        var previousBarChart = this.barChartElementId_ != null ?
                               $(this.barChartElementId_) : null;
        if (previousBarChart != null) {
          document.body.removeChild(previousBarChart);
          window.location.hash = this.id;
        }

        var columnDefinitions = createYUIColumnDefinitions(this.dataSet_);
        var dataSource = createYUIDataSource(this.dataSet_);
        var dataTable = new YAHOO.widget.DataTable(this.id, columnDefinitions,
            dataSource, {caption:'Results'});
        var firstRow = this.dataSet_[1];
        var currentViewer = this;

        dataTable.subscribe('cellClickEvent', function (oArgs) {
          var selectedColumn = dataTable.getColumn(oArgs.target);
          var selectedColumnIndex = parseInt(selectedColumn.key);

          if (selectedColumnIndex == 0) {
            unselectAllColumns(dataTable);
            return;
          }

          if (isNumeric(firstRow[selectedColumnIndex])) {
            dataTable.selectColumn(selectedColumn);
            if (currentViewer.barChartElementId_ != null) {
              var viewerBarChart_ =
                      new ViewerBarChart({ownerDocument:window.document});
              viewerBarChart_.id = currentViewer.barChartElementId_;
              viewerBarChart_.dataSet = currentViewer.dataSet_;
              viewerBarChart_.selectedColumnIndices
                  = getSelectedColumnIndices(dataTable);
              viewerBarChart_.render();
            }
          }
        });
      }
    };
  }());

  /**
   * BarChart component for the results viewer.
   */
  var ViewerBarChart = ui.define('div');
  (function () {
    //"Private" functions for ViewerBarChart
    /**
     * Generates a new array that contains only the first column, and all
     * other selected columns.
     * @param {(string|number)[][]} dataset Array with the csv contents.
     * @param {int[]} selectedColumnIndices Indices for all the selected
     * columns.
     * @return {String[][]} A new array containing the first column
     * and all selected columns.
     */
    function extractColumnsToPlot(dataset, selectedColumnIndices) {
      var lines = [];
      var line = [];
      for (var i = 0; i < dataset.length; ++i) {
        line.push(dataset[i][0]);
        for (var j = 0; j < selectedColumnIndices.length; j++) {
          var elementValue = dataset[i][selectedColumnIndices[j]];
          line.push(isNumeric(elementValue) ? parseFloat(elementValue) :
                        elementValue);
        }
        lines.push(line);
        line = [];
      }
      return lines;
    }

    ViewerBarChart.prototype = {
      __proto__:HTMLDivElement.prototype,
      decorate: function() {
        /**
         * Percetage of the window width that will be used for the chart
         * @const
         * @type {float}
         */
        ViewerBarChart.PERCENTAGE_OF_WINDOW_WIDTH_FOR_CHART = 0.75;
        /**
         * Approximate number of pixels that will be used per line
         * @const
         * @type {int}
         */
        ViewerBarChart.HEIGHT_PER_LINE_IN_PIXELS = 28;

        /**
         * Raw dataset, which contains the csv file contents.
         * @type {(String|number)[][]}
         */
        this.dataSet_ = undefined;
        /**
         * Array that contains the selected indices from the table view.
         * @type {number[]}
         */
        this.selectedColumnIndices_ = undefined;
      },
      /**
       * Renders the ViewerBarChart component.
       * @expose
       */
      render : function() {
        var existingBarChart = $(this.id);
        if (existingBarChart != null) {
          //Remove the previous bar chart
          document.body.removeChild(existingBarChart);
        }
        //Attach this component to the document
        document.body.appendChild(this);
        var lines = extractColumnsToPlot(this.dataSet_,
            this.selectedColumnIndices_);
        var data = google.visualization.arrayToDataTable(lines);

        var barCharWidth = window.width *
                           ViewerBarChart.PERCENTAGE_OF_WINDOW_WIDTH_FOR_CHART;
        var barCharHeight = this.dataSet_.length *
                            ViewerBarChart.HEIGHT_PER_LINE_IN_PIXELS;
        var options = {
          'width': barCharWidth,
          'height':barCharHeight,
          'fontSize':15
        };
        new google.visualization.BarChart(this).draw(data, options);
        window.location.hash = this.id;
      },
      set dataSet(ds) {
        this.dataSet_ = ds;
      },
      set selectedColumnIndices(sci) {
        this.selectedColumnIndices_ = sci;
      }
    };
  }());

  /**
   * Determines whether or not a string can be parsed to a number.
   * @param {String} element String representation of the potential number.
   * @return {boolean} True or false depending on whether the element is
   * numeric or not.
   */
  function isNumeric(element) {
    return !isNaN(parseFloat(element)) && isFinite(element);
  }

  window.Viewer = Viewer;
  window.ViewerBarChart = ViewerBarChart;
})();