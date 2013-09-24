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

    ["http://cruises.orbitz.com/results.do?wdos=3&places=ALL&days=ALL&Month=ALL&dd=ALL&d=NaN%2FNaN%2FNaN&d2=NaN%2FNaN%2FNaN&fd=2&shoppingZipCode=Zip+Code&Search.x=29&Search.y=6&Search=Search&c=ALL&v=ALL&porttype=E&SType=P&ptype=c&type=c&p=ALL&SType=A&clp=1&sort=7&IncludeSeniorRates=false&IncludeAlumniRates=false&AlumniCruiseId=false",
     "http://en.wikipedia.org/wiki/List_of_Pok%C3%A9mon",
     "http://espn.go.com/",
     "http://fashion.ebay.com/womens-clothing",
     "http://news.baidu.com/",
     "http://news.google.co.in",
     "http://news.qq.com/",
     "http://techcrunch.com/",
     "http://techmeme.com/",
     "http://twitter.com/BarackObama",
     "http://www.weibo.com/yaochen",
     "http://www.163.com/",
     "http://www.amazon.com/Kindle-Fire-Amazon-Tablet/dp/B0051VVOB2/",
     "http://www.bbc.co.uk/",
     "http://www.cnet.com/",
     "http://www.engadget.com/",
     "http://www.flickr.com/photos/tags/flowers",
     "http://www.google.com/search?q=dogs",
     "http://www.imdb.com/",
     "http://www.linkedin.com/in/jeffweiner08",
     "http://www.msn.com/",
     "http://www.nytimes.com/pages/sports/index.html",
     "http://www.sina.com.cn/",
     "http://www.taobao.com/",
     "http://www.tudou.com/",
     "http://www.yahoo.com/",
     "http://www.yandex.ru/yandsearch?text=obama&lr=84",
     "http://www.youtube.com",
     "http://www.youtube.com/watch?v=xvN9Ri1GmuY&feature=g-sptl&cid=inp-hs-edt",
     "https://plus.google.com/102518365620075109973/posts",
     "https://plus.google.com/photos/102518365620075109973/albums/5630432074981280145",
     "https://www.tumblr.com/",
     "https://www.facebook.com/barackobama",
     ],
];
