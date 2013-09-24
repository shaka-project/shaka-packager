// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
   "pageSets" is a list of lists.
     - Before each sublist:
           Run chrome.browingData.remove and close the connections.
     - Before each url in a sublist:
           Close the connections.
*/
var pageSets = [
    ["http://superfastpageload/web-page-replay-generate-200"],

    // Load url pairs without clearing browser data (e.g. cache) in-between.
    ["http://go.com/",
     "http://espn.go.com/"],
    ["http://www.amazon.com/",
     "http://www.amazon.com/Kindle-Fire-Amazon-Tablet/dp/B0051VVOB2/"],
    ["http://www.baidu.com/",
     "http://www.baidu.com/s?wd=obama"],
    ["http://www.bing.com/",
     "http://www.bing.com/search?q=cars"],
    ["http://www.ebay.com/",
     "http://fashion.ebay.com/womens-clothing"],
    ["http://www.google.com/",
     "http://www.google.com/search?q=dogs"],
    ["http://www.yandex.ru/",
     "http://yandex.ru/yandsearch?text=obama&lr=84"],
    ["http://www.youtube.com",
     "http://www.youtube.com/watch?v=xvN9Ri1GmuY&feature=g-sptl&cid=inp-hs-edt"],

    ["http://ameblo.jp/"],
    ["http://en.rakuten.co.jp/"],
    ["http://en.wikipedia.org/wiki/Lady_gaga"],
    ["http://news.google.co.in"],
    ["http://plus.google.com/"],  // iframe error (result of no cookies?)
    ["http://www.163.com/"],
    ["http://www.apple.com/"],
    ["http://www.bbc.co.uk/"],
    ["http://www.cnet.com/"],
    ["http://www.msn.com/"],
    ["http://www.nytimes.com/"],
    ["http://www.taobao.com/"],
    ["http://www.yahoo.co.jp/"],

    // HTTPS pages.
    ["https://wordpress.com/"],
    ["https://www.conduit.com/"],
    ["https://www.facebook.com",
     "https://www.facebook.com/barackobama"],
];

/*
    // Not included (need further investigation).
    ["http://twitter.com/BarackObama",
    "http://twitter.com/search?q=pizza"], // large variance on second page
    ["http://www.fc2.com/"],      // slow
    ["http://sina.com.cn/"],      // slow
    ["http://www.mail.ru/"],      // long load time (30s+)
    ["http://www.sohu.com/"],     // load does not finish (even without WPR)

    // Not included (trimmed pageSets to keep test under 10 minutes).
    ["http://sfbay.craigslist.org/",
    "http://sfbay.craigslist.org/search/sss?query=flowers"],
    ["http://www.flickr.com/",
    "http://www.flickr.com/photos/tags/flowers"],
    ["http://www.linkedin.com/",
    "http://www.linkedin.com/in/jeffweiner08"],
    ["http://www.yahoo.com/",
    "http://search.yahoo.com/search?p=disney"],
    ["http://googleblog.blogspot.com/"],
    ["http://www.adobe.com/reader/"],
    ["http://www.cnn.com/"],
    ["http://www.imdb.com/"],
    ["http://www.qq.com/"],
*/
