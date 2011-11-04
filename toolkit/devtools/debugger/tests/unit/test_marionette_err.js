/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
Cu.import("resource:///modules/dbg-server.jsm");
Cu.import("resource:///modules/dbg-client.jsm");

function run_test()
{
  //DebuggerServer.addActors("resource:///modules/marionette-actors.js");
  //DebuggerServer.init();

  add_test(test_error);

  run_next_test();
}
function test_error()
{
  //DebuggerServer.openListener(2929, true);
  do_test_pending();
  received = false;
  id = "";

  let transport = debuggerSocketConnect("127.0.0.1", 2929);
  transport.hooks = {
    onPacket: function(aPacket) {
      this.onPacket = function(aPacket) {
        if (received) {
          do_check_eq(aPacket.from, id);
          if(aPacket.type == "executeScript" && aPacket.error == undefined) {
            do_throw("Expected error, instead received 'done' packet!");
            transport.close();
          }
          else {
            transport.close();
          }
        }
        else {
          received = true;
          id = aPacket.id;
          transport.send({to: id,
                        type: "newSession",
                        });
          transport.send({to: id,
                        type: "executeScript",
                        value: "asdf();",
                        });
          transport.send({to: id,
                        type: "deleteSession"
                        });
        }
      }
      transport.send({to: "root",
                      type: "getMarionetteID",
                      });
    },
    onClosed: function(aStatus) {
      do_check_eq(aStatus, 0);
      do_test_finished();
      run_next_test();
    },
  };
  transport.ready();
}
