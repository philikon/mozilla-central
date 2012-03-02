/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


/*
 * test_nodb: Start search engine
 * - without search-metadata.json
 * - without search.sqlite
 *
 * Ensure that :
 * - nothing explodes;
 * - if we change the order, search-metadata.json is created;
 * - this search-medata.json can be parsed;
 * - the order stored in search-metadata.json is consistent.
 *
 * Notes:
 * - we install the search engines of test "test_downloadAndAddEngines.js"
 * to ensure that this test is independent from locale, commercial agreements
 * and configuration of Firefox.
 */

do_load_httpd_js();

function run_test()
{
  removeMetadata();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");
  do_load_manifest("data/chrome.manifest");

  let httpServer = new nsHttpServer();
  httpServer.start(4444);
  httpServer.registerDirectory("/", do_get_cwd());

  let search = Services.search;

  function observer(aSubject, aTopic, aData) {
    if ("engine-added" == aData) {
      let engine1 = search.getEngineByName("Test search engine");
      let engine2 = search.getEngineByName("Sherlock test search engine");
      dumpn("Got engine 2: "+engine2);
      if(engine1 && engine2)
      {
        search.moveEngine(engine1, 0);
        search.moveEngine(engine2, 1);
        do_timeout(0,
                   function() {
                     // Force flush
                     // Note: the timeout is needed, to avoid some reentrency
                     // issues in nsSearchService.
                     search.QueryInterface(Ci.nsIObserver).
                       observe(observer, "quit-application", "<no verb>");
                   });
        afterCommit(
          function()
          {
            //Check that search-metadata.json has been created
            let metadata = gProfD.clone();
            metadata.append("search-metadata.json");
            do_check_true(metadata.exists());

            //Check that the entries are placed as specified correctly
            let json = parseJsonFromStream(
              NetUtil.newChannel(metadata).open());
            do_check_eq(json["[app]/test-search-engine.xml"].order, 1);
            do_check_eq(json["[profile]/sherlock-test-search-engine.xml"].order, 2);
            removeMetadata();
            httpServer.stop(function() {});
            do_test_finished();
          }
        );
      }
    }
  };
  Services.obs.addObserver(observer, "browser-search-engine-modified",
                           false);

  do_test_pending();

  search.addEngine("http://localhost:4444/data/engine.xml",
                   Ci.nsISearchEngine.DATA_XML,
                   null, false);
  search.addEngine("http://localhost:4444/data/engine.src",
                   Ci.nsISearchEngine.DATA_TEXT,
                   "http://localhost:4444/data/ico-size-16x16-png.ico",
                   false);
}
