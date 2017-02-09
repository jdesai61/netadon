var https = require('https');
var net = require('net');
var tls = require('tls');
var netadon = require('../../netadon');
var argv = require('yargs')
  .default('h', 'localhost')
  .default('p', 8901)
  .default('t', 1)
  .default('n', 10)
  .default('b', 65535)
  .number(['p', 't', 'n', 'b'])
  .argv;

process.env.UV_THREADPOOL_SIZE = 42;

var options = {
  keepAlive: true,
  maxSockets: 10
};

var agent = new https.Agent(options);
agent.createConnection = function (options) {
  var socket = new net.createConnection(options);
  socket.on('connect', () => {
    netadon.setSocketRecvBuffer(socket, argv.b);
    netadon.setSocketSendBuffer(socket, argv.b);
  });
  options.socket = socket;
  return tls.connect(options);
}

var total = 0;
var tally = 0;

function runNext(x, tally, total) {
  https.get({
    agent: agent,
    rejectUnauthorized : false,
    hostname: argv.h,
    port: argv.p,
    path: '/essence'
  }, (res) => {
    //console.log(`Got response: ${res.statusCode}`, keepAliveAgent.getCurrentStatus());
    // consume response body
    var startTime = process.hrtime();
    var count = 0;
    res.on('data', (x) => {
      count++;// console.log(x.length);
    });
    res.on('end', () => {
      total++;
      tally += process.hrtime(startTime)[1]/1000000;
      console.log("No more data!", tally / total, total, x, count);
      if (total < argv.n) runNext(x, tally, total);
    });
  }).on('error', (e) => {
    console.log(`Got error: ${e.message}`);
  });
}

for ( var x = 0 ; x < argv.t ; x ++) {
  runNext(x, tally, total);
}
