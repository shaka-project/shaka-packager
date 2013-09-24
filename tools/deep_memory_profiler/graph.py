#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys
from string import Template


_HTML_TEMPLATE = """
<html>
  <head>
    <script type='text/javascript' src='https://www.google.com/jsapi'></script>
    <script type='text/javascript'>
      google.load('visualization', '1', {packages:['corechart', 'table']});
      google.setOnLoadCallback(drawVisualization);
      function drawVisualization() {
        var data = google.visualization.arrayToDataTable(
          $JSON_ARRAY
        );

        var charOptions = {
          title: 'DMP Graph',
          vAxis: {title: 'Timestamp',  titleTextStyle: {color: 'red'}},
          isStacked : true
        };

        var chart = new google.visualization.BarChart(
            document.getElementById('chart_div'));
        chart.draw(data, charOptions);

        var table = new google.visualization.Table(
            document.getElementById('table_div'));
        table.draw(data);
      }
    </script>
  </head>
  <body>
    <div id='chart_div' style="width: 1024px; height: 800px;"></div>
    <div id='table_div' style="width: 1024px; height: 640px;"></div>
  </body>
</html>
"""

def _GenerateGraph(json_data, policy):
  legends = list(json_data['policies'][policy]['legends'])
  legends = ['second'] + legends[legends.index('FROM_HERE_FOR_TOTAL') + 1:
                                 legends.index('UNTIL_HERE_FOR_TOTAL')]
  data = []
  for snapshot in json_data['policies'][policy]['snapshots']:
    data.append([0] * len(legends))
    for k, v in snapshot.iteritems():
      if k in legends:
        data[-1][legends.index(k)] = v
  print Template(_HTML_TEMPLATE).safe_substitute(
      {'JSON_ARRAY': json.dumps([legends] + data)})


def main(argv):
  _GenerateGraph(json.load(file(argv[1], 'r')), argv[2])


if __name__ == '__main__':
  sys.exit(main(sys.argv))


