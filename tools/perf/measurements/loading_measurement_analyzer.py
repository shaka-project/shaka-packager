#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses CSV output from the loading_measurement and outputs interesting stats.

Example usage:
$ tools/perf/run_measurement --browser=release \
    --output-format=csv --output=/path/to/loading_measurement_output.csv \
    loading_measurement tools/perf/page_sets/top_1m.json
$ tools/perf/measurements/loading_measurement_analyzer.py \
    --num-slowest-urls=100 --rank-csv-file=/path/to/top-1m.csv \
    /path/to/loading_measurement_output.csv
"""

import collections
import csv
import heapq
import optparse
import os
import re
import sys


class LoadingMeasurementAnalyzer(object):

  def __init__(self, input_file, options):
    self.ranks = {}
    self.totals = collections.defaultdict(list)
    self.maxes = collections.defaultdict(list)
    self.avgs = collections.defaultdict(list)
    self.load_times = []
    self.cpu_times = []
    self.network_percents = []
    self.num_rows_parsed = 0
    self.num_slowest_urls = options.num_slowest_urls
    if options.rank_csv_file:
      self._ParseRankCsvFile(os.path.expanduser(options.rank_csv_file))
    self._ParseInputFile(input_file, options)
    self._display_zeros = options.display_zeros

  def _ParseInputFile(self, input_file, options):
    with open(input_file, 'r') as csvfile:
      row_dict = csv.DictReader(csvfile)
      for row in row_dict:
        if (options.rank_limit and
            self._GetRank(row['url']) > options.rank_limit):
          continue
        cpu_time = 0
        load_time = float(row['load_time (ms)'])
        if load_time < 0:
          print 'Skipping %s due to negative load time' % row['url']
          continue
        for key, value in row.iteritems():
          if key in ('url', 'load_time (ms)', 'dom_content_loaded_time (ms)'):
            continue
          if not value or value == '-':
            continue
          value = float(value)
          if not value:
            continue
          if '_avg' in key:
            self.avgs[key].append((value, row['url']))
          elif '_max' in key:
            self.maxes[key].append((value, row['url']))
          else:
            self.totals[key].append((value, row['url']))
            cpu_time += value
        self.load_times.append((load_time, row['url']))
        self.cpu_times.append((cpu_time, row['url']))
        if options.show_network:
          network_time = load_time - cpu_time
          self.totals['Network (ms)'].append((network_time, row['url']))
          self.network_percents.append((network_time / load_time, row['url']))
        self.num_rows_parsed += 1
        if options.max_rows and self.num_rows_parsed == int(options.max_rows):
          break

  def _ParseRankCsvFile(self, input_file):
    with open(input_file, 'r') as csvfile:
      for row in csv.reader(csvfile):
        assert len(row) == 2
        self.ranks[row[1]] = int(row[0])

  def _GetRank(self, url):
    url = url.replace('http://', '')
    if url in self.ranks:
      return self.ranks[url]
    return len(self.ranks)

  def PrintSummary(self, stdout):
    sum_totals = {}
    units = None
    for key, values in self.totals.iteritems():
      m = re.match('.* [(](.*)[)]', key)
      assert m, 'All keys should have units.'
      assert not units or units == m.group(1), 'All units should be the same.'
      units = m.group(1)
      sum_totals[key] = sum([v[0] for v in values])
    total_cpu_time = sum([v[0] for v in self.cpu_times])
    total_page_load_time = sum([v[0] for v in self.load_times])

    print >> stdout
    print >> stdout, 'Total URLs:', self.num_rows_parsed
    print >> stdout, 'Total page load time: %ds' % int(round(
        total_page_load_time / 1000))
    print >> stdout, 'Average page load time: %dms' % int(round(
        total_page_load_time / self.num_rows_parsed))
    if units == 'ms':
      print >> stdout, 'Total CPU time: %ds' % int(round(total_cpu_time / 1000))
      print >> stdout, 'Average CPU time: %dms' % int(round(
          total_cpu_time / self.num_rows_parsed))
    print >> stdout
    for key, value in sorted(sum_totals.iteritems(), reverse=True,
                             key=lambda i: i[1]):
      if not self._display_zeros and not int(value / 100.):
        break
      output_key = '%60s: ' % re.sub(' [(].*[)]', '', key)
      if units == 'ms':
        output_value = '%10ds ' % (value / 1000)
        output_percent = '%.1f%%' % (100 * value / total_page_load_time)
      else:
        output_value = '%10d%s ' % (value, units)
        output_percent = '%.1f%%' % (100 * value / total_cpu_time)
      print >> stdout, output_key, output_value, output_percent

    if not self.num_slowest_urls:
      return

    for key, values in sorted(self.totals.iteritems(), reverse=True,
                              key=lambda i: sum_totals[i[0]]):
      if not self._display_zeros and not int(sum_totals[key] / 100.):
        break
      print >> stdout
      print >> stdout, 'Top %d slowest %s:' % (self.num_slowest_urls,
                                               re.sub(' [(].*[)]', '', key))
      slowest = heapq.nlargest(self.num_slowest_urls, values)
      for value, url in slowest:
        print >> stdout, '%10d%s\t%s (#%s)' % (value, units, url,
                                               self._GetRank(url))

    if self.network_percents:
      print >> stdout
      print >> stdout, 'Top %d highest network to CPU time ratios:' % (
          self.num_slowest_urls)
      for percent, url in sorted(
          self.network_percents, reverse=True)[:self.num_slowest_urls]:
        percent *= 100
        print >> stdout, '\t', '%.1f%%' % percent, url, '(#%s)' % (
            self._GetRank(url))


def main(arguments, stdout=sys.stdout):
  prog_desc = 'Parses CSV output from the loading_measurement'
  parser = optparse.OptionParser(usage=('%prog [options]' + '\n\n' + prog_desc))

  parser.add_option('--max-rows', type='int',
                    help='Only process this many rows')
  parser.add_option('--num-slowest-urls', type='int',
                    help='Output this many slowest URLs for each category')
  parser.add_option('--rank-csv-file', help='A CSV file of <rank,url>')
  parser.add_option('--rank-limit', type='int',
                    help='Only process pages higher than this rank')
  parser.add_option('--show-network', action='store_true',
                    help='Whether to display Network as a category')
  parser.add_option('--display-zeros', action='store_true',
                    help='Whether to display categories with zero time')

  options, args = parser.parse_args(arguments)

  assert len(args) == 1, 'Must pass exactly one CSV file to analyze'
  if options.rank_limit and not options.rank_csv_file:
    print 'Must pass --rank-csv-file with --rank-limit'
    return 1

  LoadingMeasurementAnalyzer(args[0], options).PrintSummary(stdout)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
