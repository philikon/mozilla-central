var EXPORTED_SYMBOLS = ["gWriteLog"];
var gLogStream = 0;
var localfile = Components.Constructor("@mozilla.org/file/local;1", "nsILocalFile", "initWithPath");

Components.utils.import("resource://gre/modules/FileUtils.jsm");

var gWriteLog = function dbg_writelog(msg) {
    msg = msg + '\n';
    if (gLogStream)
        gLogStream.write(msg, msg.length);
};

var f;
try {
    f = FileUtils.getFile('CurProcD', ['log.txt']);
    gLogStream = FileUtils.openSafeFileOutputStream(f,
        FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_APPEND);
    gWriteLog('opened log');
}
catch(e) {}




