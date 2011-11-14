var EXPORTED_SYMBOLS = ["MarionetteLogger"];
var localfile = Components.Constructor("@mozilla.org/file/local;1", "nsILocalFile", "initWithPath");

Components.utils.import("resource://gre/modules/FileUtils.jsm");

var MarionetteLogger = {
    _logstream: null,

    init: function ML__init() {
        if (!this._logstream) {
            var logf = FileUtils.getFile('CurProcD', ['marionette.log']);
            this._logstream = FileUtils.openSafeFileOutputStream(logf,
                FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_APPEND);
            this.write('logfile created ' + new Date());
        }
    },

    write: function ML__write(msg) {
        msg = msg + '\n';
        if (this._logstream)
            this._logstream.write(msg, msg.length);
    },
};

MarionetteLogger.init();




